/**
 * @file test_core_exception_handling.cpp
 * @brief Unit tests for NIMCP exception handling system with core module integration
 *
 * WHAT: Tests exception creation, dispatch, handler chain, and immune integration
 * WHY:  Verify exception handling works correctly for all core modules
 * HOW:  Test error code mapping, handler registration, and recovery actions
 *
 * TEST COVERAGE:
 * - Exception creation for all types (base, memory, brain, I/O, threading, etc.)
 * - Error code to category/severity mapping
 * - Handler registration and dispatch
 * - Reference counting and cleanup
 * - Immune system integration (when connected)
 * - Recovery action selection
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <gtest/gtest.h>
#include <string.h>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
}

// ============================================================================
// Test Fixture
// ============================================================================

class CoreExceptionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize exception system
        nimcp_exception_system_init();
    }

    void TearDown() override {
        // Clear any current exception
        nimcp_exception_clear_current();
        // Note: We don't call shutdown to avoid interfering with other tests
    }
};

// ============================================================================
// Exception Creation Tests
// ============================================================================

TEST_F(CoreExceptionTest, CreateBaseException) {
    // WHAT: Create a base exception with error code and message
    // WHY:  Verify basic exception creation works
    // HOW:  Call nimcp_exception_create and check fields

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test exception: %s", "unit test"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_ERROR);
    EXPECT_EQ(ex->type, EXCEPTION_TYPE_BASE);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_GENERIC);
    EXPECT_GE(ex->ref_count, 1);
    EXPECT_STRNE(ex->message, "");

    nimcp_exception_unref(ex);
}

TEST_F(CoreExceptionTest, CreateMemoryException) {
    // WHAT: Create a memory exception with allocation details
    // WHY:  Verify memory exception type captures memory-specific info
    // HOW:  Call nimcp_memory_exception_create and check fields

    nimcp_memory_exception_t* ex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1024 * 1024,  // 1MB requested
        "Memory allocation failed: %zu bytes", (size_t)(1024 * 1024)
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.code, NIMCP_ERROR_NO_MEMORY);
    EXPECT_EQ(ex->base.type, EXCEPTION_TYPE_MEMORY);
    EXPECT_EQ(ex->base.category, EXCEPTION_CATEGORY_MEMORY);
    EXPECT_EQ(ex->base.severity, EXCEPTION_SEVERITY_SEVERE);
    EXPECT_EQ(ex->requested_size, 1024 * 1024);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(CoreExceptionTest, CreateBrainException) {
    // WHAT: Create a brain exception with neural network details
    // WHY:  Verify brain exception captures network-specific info
    // HOW:  Call nimcp_brain_exception_create and check fields

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        42,  // brain_id
        "prefrontal_cortex",  // region_name
        "Forward pass failed in layer %d", 3
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.code, NIMCP_ERROR_FORWARD_PASS);
    EXPECT_EQ(ex->base.type, EXCEPTION_TYPE_BRAIN);
    EXPECT_EQ(ex->base.category, EXCEPTION_CATEGORY_BRAIN);
    EXPECT_EQ(ex->brain_id, 42u);
    EXPECT_STREQ(ex->region_name, "prefrontal_cortex");

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(CoreExceptionTest, CreateIOException) {
    // WHAT: Create an I/O exception with file details
    // WHY:  Verify I/O exception captures file-specific info
    // HOW:  Call nimcp_io_exception_create and check fields

    nimcp_io_exception_t* ex = nimcp_io_exception_create(
        NIMCP_ERROR_FILE_NOT_FOUND,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "/path/to/missing/file.dat",
        "File not found: %s", "/path/to/missing/file.dat"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.code, NIMCP_ERROR_FILE_NOT_FOUND);
    EXPECT_EQ(ex->base.type, EXCEPTION_TYPE_IO);
    EXPECT_EQ(ex->base.category, EXCEPTION_CATEGORY_IO);
    EXPECT_STREQ(ex->path, "/path/to/missing/file.dat");

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(CoreExceptionTest, CreateThreadingException) {
    // WHAT: Create a threading exception with thread details
    // WHY:  Verify threading exception captures concurrency info
    // HOW:  Call nimcp_threading_exception_create and check fields

    nimcp_threading_exception_t* ex = nimcp_threading_exception_create(
        NIMCP_ERROR_DEADLOCK,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        12345,  // thread_id
        "Deadlock detected in thread %lu", (unsigned long)12345
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.code, NIMCP_ERROR_DEADLOCK);
    EXPECT_EQ(ex->base.type, EXCEPTION_TYPE_THREADING);
    EXPECT_EQ(ex->base.category, EXCEPTION_CATEGORY_THREADING);
    EXPECT_EQ(ex->base.severity, EXCEPTION_SEVERITY_CRITICAL);
    EXPECT_EQ(ex->thread_id, 12345u);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(CoreExceptionTest, CreateGPUException) {
    // WHAT: Create a GPU exception with CUDA error details
    // WHY:  Verify GPU exception captures device-specific info
    // HOW:  Call nimcp_gpu_exception_create and check fields

    nimcp_gpu_exception_t* ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0,  // device_id
        999,  // cuda_error
        "CUDA kernel failed on device %d", 0
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.code, NIMCP_ERROR_GPU);
    EXPECT_EQ(ex->base.type, EXCEPTION_TYPE_GPU);
    EXPECT_EQ(ex->base.category, EXCEPTION_CATEGORY_GPU);
    EXPECT_EQ(ex->device_id, 0);
    EXPECT_EQ(ex->cuda_error, 999);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

// ============================================================================
// Error Code Mapping Tests
// ============================================================================

TEST_F(CoreExceptionTest, CategoryFromErrorCode) {
    // WHAT: Verify error code to category mapping
    // WHY:  Ensure exceptions are correctly categorized
    // HOW:  Test various error codes and check their categories

    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_UNKNOWN),
              EXCEPTION_CATEGORY_GENERIC);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_NO_MEMORY),
              EXCEPTION_CATEGORY_MEMORY);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_BRAIN_CREATION),
              EXCEPTION_CATEGORY_BRAIN);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_FILE_NOT_FOUND),
              EXCEPTION_CATEGORY_IO);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_CONFIG_INVALID),
              EXCEPTION_CATEGORY_CONFIG);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_DEADLOCK),
              EXCEPTION_CATEGORY_THREADING);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_SIGSEGV),
              EXCEPTION_CATEGORY_SIGNAL);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_WORKING_MEMORY),
              EXCEPTION_CATEGORY_COGNITIVE);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_GPU),
              EXCEPTION_CATEGORY_GPU);
    // Brain region errors (10000-19999)
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_MOTOR_PLANNING),
              EXCEPTION_CATEGORY_BRAIN_REGION);
}

TEST_F(CoreExceptionTest, SeverityFromErrorCode) {
    // WHAT: Verify error code to severity mapping
    // WHY:  Ensure exceptions have appropriate severity levels
    // HOW:  Test various error codes and check their severities

    // Fatal signals
    EXPECT_EQ(nimcp_exception_get_severity_from_code(NIMCP_ERROR_SIGSEGV),
              EXCEPTION_SEVERITY_FATAL);
    EXPECT_EQ(nimcp_exception_get_severity_from_code(NIMCP_ERROR_SIGABRT),
              EXCEPTION_SEVERITY_FATAL);

    // Critical errors
    EXPECT_EQ(nimcp_exception_get_severity_from_code(NIMCP_ERROR_DEADLOCK),
              EXCEPTION_SEVERITY_CRITICAL);

    // Severe memory errors
    EXPECT_EQ(nimcp_exception_get_severity_from_code(NIMCP_ERROR_NO_MEMORY),
              EXCEPTION_SEVERITY_SEVERE);

    // Generic errors
    EXPECT_EQ(nimcp_exception_get_severity_from_code(NIMCP_ERROR_OPERATION_FAILED),
              EXCEPTION_SEVERITY_ERROR);

    // Config warnings
    EXPECT_EQ(nimcp_exception_get_severity_from_code(NIMCP_ERROR_CONFIG_INVALID),
              EXCEPTION_SEVERITY_WARNING);
}

// ============================================================================
// Reference Counting Tests
// ============================================================================

TEST_F(CoreExceptionTest, ReferenceCountingBasic) {
    // WHAT: Test basic reference counting for exceptions
    // WHY:  Ensure exceptions are properly managed
    // HOW:  Create, ref, unref and check counts

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Ref counting test"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->ref_count, 1);

    // Add reference
    nimcp_exception_t* ref = nimcp_exception_ref(ex);
    EXPECT_EQ(ref, ex);
    EXPECT_EQ(ex->ref_count, 2);

    // Release one reference
    nimcp_exception_unref(ex);
    // Note: Cannot safely check ref_count here if ex was freed

    // Release second reference (should free)
    nimcp_exception_unref(ref);
}

TEST_F(CoreExceptionTest, ExceptionChaining) {
    // WHAT: Test exception cause chaining
    // WHY:  Verify exceptions can track their root cause
    // HOW:  Create nested exceptions with cause relationships

    nimcp_exception_t* cause = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Root cause: out of memory"
    );

    nimcp_exception_t* effect = nimcp_exception_create(
        NIMCP_ERROR_BRAIN_CREATION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Brain creation failed"
    );

    ASSERT_NE(cause, nullptr);
    ASSERT_NE(effect, nullptr);

    // Chain the exceptions
    nimcp_exception_set_cause(effect, cause);

    // Verify chain
    nimcp_exception_t* retrieved_cause = nimcp_exception_get_cause(effect);
    EXPECT_EQ(retrieved_cause, cause);
    EXPECT_EQ(cause->ref_count, 2);  // One from create, one from set_cause

    // Clean up
    nimcp_exception_unref(effect);
    // cause should be freed when effect is freed
}

// ============================================================================
// Context API Tests
// ============================================================================

TEST_F(CoreExceptionTest, SetGetContext) {
    // WHAT: Test setting and getting context key-value pairs
    // WHY:  Verify structured context data works correctly
    // HOW:  Set context, get context, verify values

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_BRAIN_CREATION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Brain creation failed"
    );

    ASSERT_NE(ex, nullptr);

    // Set context entries
    int result = nimcp_exception_set_context(ex, "brain_id", "42");
    EXPECT_EQ(result, 0);

    result = nimcp_exception_set_context(ex, "region", "prefrontal");
    EXPECT_EQ(result, 0);

    // Get context entries
    const char* brain_id = nimcp_exception_get_context(ex, "brain_id");
    EXPECT_STREQ(brain_id, "42");

    const char* region = nimcp_exception_get_context(ex, "region");
    EXPECT_STREQ(region, "prefrontal");

    // Non-existent key
    const char* missing = nimcp_exception_get_context(ex, "nonexistent");
    EXPECT_EQ(missing, nullptr);

    // Context count
    EXPECT_EQ(nimcp_exception_context_count(ex), 2u);

    nimcp_exception_unref(ex);
}

TEST_F(CoreExceptionTest, RemoveContext) {
    // WHAT: Test removing context entries
    // WHY:  Verify context entries can be removed
    // HOW:  Add, remove, verify absence

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test exception"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "key1", "value1");
    nimcp_exception_set_context(ex, "key2", "value2");
    EXPECT_EQ(nimcp_exception_context_count(ex), 2u);

    // Remove one entry
    int result = nimcp_exception_remove_context(ex, "key1");
    EXPECT_EQ(result, 0);
    EXPECT_EQ(nimcp_exception_context_count(ex), 1u);
    EXPECT_EQ(nimcp_exception_get_context(ex, "key1"), nullptr);
    EXPECT_NE(nimcp_exception_get_context(ex, "key2"), nullptr);

    nimcp_exception_unref(ex);
}

// ============================================================================
// Handler Registration Tests
// ============================================================================

// Test handler callback
static bool g_test_handler_called = false;
static nimcp_exception_t* g_test_handler_exception = nullptr;

static bool test_exception_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    g_test_handler_called = true;
    g_test_handler_exception = ex;
    return false;  // Don't consume, let other handlers run
}

static bool test_consuming_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    (void)ex;
    return true;  // Consume the exception
}

TEST_F(CoreExceptionTest, HandlerRegistration) {
    // WHAT: Test handler registration and unregistration
    // WHY:  Verify handlers can be added and removed from chain
    // HOW:  Register handler, verify, unregister

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "test_handler";
    options.handler = test_exception_handler;
    options.priority = NIMCP_HANDLER_PRIORITY_NORMAL;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);
    EXPECT_EQ(reg->active, true);

    // Verify handler count increased
    size_t count = nimcp_handler_count();
    EXPECT_GE(count, 1u);

    // Unregister
    int result = nimcp_handler_unregister(reg);
    EXPECT_EQ(result, 0);
}

TEST_F(CoreExceptionTest, HandlerDispatch) {
    // WHAT: Test exception dispatch through handler chain
    // WHY:  Verify handlers are called in priority order
    // HOW:  Register handler, dispatch exception, check callback

    g_test_handler_called = false;
    g_test_handler_exception = nullptr;

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "dispatch_test_handler";
    options.handler = test_exception_handler;
    options.priority = NIMCP_HANDLER_PRIORITY_HIGH;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);

    // Create and dispatch exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Dispatch test"
    );

    bool handled = nimcp_exception_dispatch(ex);
    // Handler returns false, so not fully handled
    EXPECT_FALSE(handled);
    EXPECT_TRUE(g_test_handler_called);
    EXPECT_EQ(g_test_handler_exception, ex);

    nimcp_exception_unref(ex);
    nimcp_handler_unregister(reg);
}

TEST_F(CoreExceptionTest, HandlerPriority) {
    // WHAT: Test handler priority ordering
    // WHY:  Verify higher priority handlers are called first
    // HOW:  Register handlers with different priorities, check call order

    static int call_order[3] = {0, 0, 0};
    static int call_counter = 0;

    auto handler_low = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        (void)user_data;
        call_order[call_counter++] = 1;  // Low
        return false;
    };

    auto handler_normal = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        (void)user_data;
        call_order[call_counter++] = 2;  // Normal
        return false;
    };

    auto handler_high = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        (void)user_data;
        call_order[call_counter++] = 3;  // High
        return false;
    };

    // Reset counters
    call_counter = 0;
    memset(call_order, 0, sizeof(call_order));

    // Register in wrong order - priorities should sort them
    nimcp_handler_options_t opts;

    nimcp_handler_default_options(&opts);
    opts.name = "low";
    opts.handler = handler_low;
    opts.priority = NIMCP_HANDLER_PRIORITY_LOW;
    auto* reg_low = nimcp_handler_register(&opts);

    nimcp_handler_default_options(&opts);
    opts.name = "high";
    opts.handler = handler_high;
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    auto* reg_high = nimcp_handler_register(&opts);

    nimcp_handler_default_options(&opts);
    opts.name = "normal";
    opts.handler = handler_normal;
    opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
    auto* reg_normal = nimcp_handler_register(&opts);

    // Dispatch
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Priority test"
    );

    nimcp_exception_dispatch(ex);

    // High priority (3) should be called first, then normal (2), then low (1)
    EXPECT_EQ(call_order[0], 3);  // High
    EXPECT_EQ(call_order[1], 2);  // Normal
    EXPECT_EQ(call_order[2], 1);  // Low

    nimcp_exception_unref(ex);
    nimcp_handler_unregister(reg_low);
    nimcp_handler_unregister(reg_normal);
    nimcp_handler_unregister(reg_high);
}

TEST_F(CoreExceptionTest, HandlerEnableDisable) {
    // WHAT: Test handler enable/disable
    // WHY:  Verify handlers can be temporarily disabled
    // HOW:  Register, disable, dispatch, verify not called

    g_test_handler_called = false;

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "enable_disable_test";
    options.handler = test_exception_handler;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);

    // Disable handler
    nimcp_handler_disable(reg);
    EXPECT_FALSE(reg->active);

    // Dispatch - handler should not be called
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Enable/disable test"
    );

    nimcp_exception_dispatch(ex);
    // Handler was disabled, so should not be called
    // (though other handlers may have been called)

    // Re-enable
    nimcp_handler_enable(reg);
    EXPECT_TRUE(reg->active);

    nimcp_exception_unref(ex);
    nimcp_handler_unregister(reg);
}

// ============================================================================
// Recovery Action Tests
// ============================================================================

TEST_F(CoreExceptionTest, SuggestedRecoveryAction) {
    // WHAT: Test recovery action suggestions based on exception type
    // WHY:  Verify correct recovery actions are suggested
    // HOW:  Create exceptions of different types, check suggested actions

    // Memory exception -> GC
    nimcp_exception_t* mem_ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Memory test"
    );
    mem_ex->category = EXCEPTION_CATEGORY_MEMORY;
    EXPECT_EQ(nimcp_exception_get_suggested_recovery(mem_ex), EXCEPTION_RECOVERY_GC);
    nimcp_exception_unref(mem_ex);

    // I/O exception -> Retry
    nimcp_exception_t* io_ex = nimcp_exception_create(
        NIMCP_ERROR_FILE_READ,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "IO test"
    );
    io_ex->category = EXCEPTION_CATEGORY_IO;
    EXPECT_EQ(nimcp_exception_get_suggested_recovery(io_ex), EXCEPTION_RECOVERY_RETRY);
    nimcp_exception_unref(io_ex);

    // Threading/deadlock -> Restart thread
    nimcp_exception_t* thread_ex = nimcp_exception_create(
        NIMCP_ERROR_DEADLOCK,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        "Threading test"
    );
    thread_ex->category = EXCEPTION_CATEGORY_THREADING;
    EXPECT_EQ(nimcp_exception_get_suggested_recovery(thread_ex), EXCEPTION_RECOVERY_RESTART_THREAD);
    nimcp_exception_unref(thread_ex);

    // Signal -> Emergency save
    nimcp_exception_t* signal_ex = nimcp_exception_create(
        NIMCP_ERROR_SIGSEGV,
        EXCEPTION_SEVERITY_FATAL,
        __FILE__, __LINE__, __func__,
        "Signal test"
    );
    signal_ex->category = EXCEPTION_CATEGORY_SIGNAL;
    EXPECT_EQ(nimcp_exception_get_suggested_recovery(signal_ex), EXCEPTION_RECOVERY_EMERGENCY_SAVE);
    nimcp_exception_unref(signal_ex);

    // Security -> Quarantine
    nimcp_exception_t* sec_ex = nimcp_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Security test"
    );
    sec_ex->category = EXCEPTION_CATEGORY_SECURITY;
    EXPECT_EQ(nimcp_exception_get_suggested_recovery(sec_ex), EXCEPTION_RECOVERY_QUARANTINE);
    nimcp_exception_unref(sec_ex);
}

// ============================================================================
// Thread-Local Current Exception Tests
// ============================================================================

TEST_F(CoreExceptionTest, ThreadLocalCurrentException) {
    // WHAT: Test thread-local current exception management
    // WHY:  Verify exception context is thread-local
    // HOW:  Set, get, clear current exception

    EXPECT_EQ(nimcp_exception_get_current(), nullptr);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Thread-local test"
    );

    nimcp_exception_set_current(ex);
    EXPECT_EQ(nimcp_exception_get_current(), ex);

    nimcp_exception_clear_current();
    EXPECT_EQ(nimcp_exception_get_current(), nullptr);

    nimcp_exception_unref(ex);
}

// ============================================================================
// Aggregate Exception Tests
// ============================================================================

TEST_F(CoreExceptionTest, AggregateException) {
    // WHAT: Test aggregate exception for batch errors
    // WHY:  Verify multiple exceptions can be grouped
    // HOW:  Create aggregate, add children, verify count and retrieval

    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Batch operation failed"
    );

    ASSERT_NE(agg, nullptr);
    EXPECT_EQ(agg->base.type, EXCEPTION_TYPE_AGGREGATE);
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 0u);

    // Add child exceptions
    nimcp_exception_t* child1 = nimcp_exception_create(
        NIMCP_ERROR_FILE_NOT_FOUND,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Child 1"
    );

    nimcp_exception_t* child2 = nimcp_exception_create(
        NIMCP_ERROR_FILE_READ,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Child 2"
    );

    int result = nimcp_aggregate_exception_add(agg, child1);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 1u);

    result = nimcp_aggregate_exception_add(agg, child2);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 2u);

    // Retrieve children
    nimcp_exception_t* retrieved1 = nimcp_aggregate_exception_get(agg, 0);
    EXPECT_EQ(retrieved1, child1);

    nimcp_exception_t* retrieved2 = nimcp_aggregate_exception_get(agg, 1);
    EXPECT_EQ(retrieved2, child2);

    // Out of bounds
    nimcp_exception_t* oob = nimcp_aggregate_exception_get(agg, 99);
    EXPECT_EQ(oob, nullptr);

    nimcp_exception_unref((nimcp_exception_t*)agg);
    // Children should be freed with aggregate
}

// ============================================================================
// String Conversion Tests
// ============================================================================

TEST_F(CoreExceptionTest, SeverityToString) {
    // WHAT: Test severity to string conversion
    // WHY:  Verify human-readable severity names
    // HOW:  Convert and check strings

    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_DEBUG), "DEBUG");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_INFO), "INFO");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_WARNING), "WARNING");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_ERROR), "ERROR");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_SEVERE), "SEVERE");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_CRITICAL), "CRITICAL");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_FATAL), "FATAL");
}

TEST_F(CoreExceptionTest, CategoryToString) {
    // WHAT: Test category to string conversion
    // WHY:  Verify human-readable category names
    // HOW:  Convert and check strings

    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_GENERIC), "GENERIC");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_MEMORY), "MEMORY");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_BRAIN), "BRAIN");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_IO), "IO");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_THREADING), "THREADING");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_SIGNAL), "SIGNAL");
}

TEST_F(CoreExceptionTest, RecoveryActionToString) {
    // WHAT: Test recovery action to string conversion
    // WHY:  Verify human-readable action names
    // HOW:  Convert and check strings

    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_NONE), "NONE");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_RETRY), "RETRY");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_GC), "GC");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_ROLLBACK), "ROLLBACK");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_QUARANTINE), "QUARANTINE");
}

// ============================================================================
// Exception Formatting Tests
// ============================================================================

TEST_F(CoreExceptionTest, ExceptionToString) {
    // WHAT: Test exception to string formatting
    // WHY:  Verify exceptions can be formatted for logging
    // HOW:  Create exception, format, check output

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_BRAIN_CREATION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Brain creation failed"
    );

    ASSERT_NE(ex, nullptr);

    char buffer[1024];
    size_t len = nimcp_exception_to_string(ex, buffer, sizeof(buffer));
    EXPECT_GT(len, 0u);
    EXPECT_NE(strstr(buffer, "Brain creation failed"), nullptr);

    nimcp_exception_unref(ex);
}

// ============================================================================
// Immune Integration Tests
// ============================================================================

TEST_F(CoreExceptionTest, ImmuneConfigDefaults) {
    // WHAT: Test immune integration default configuration
    // WHY:  Verify sensible defaults are set
    // HOW:  Get defaults and check values

    nimcp_exception_immune_config_t config;
    nimcp_exception_immune_default_config(&config);

    EXPECT_TRUE(config.enable_auto_present);
    EXPECT_EQ(config.min_present_severity, EXCEPTION_SEVERITY_SEVERE);
    EXPECT_TRUE(config.enable_auto_recovery);
    EXPECT_TRUE(config.enable_memory_formation);
    EXPECT_FALSE(config.async_presentation);
}

TEST_F(CoreExceptionTest, ImmuneInitShutdown) {
    // WHAT: Test immune integration init and shutdown
    // WHY:  Verify initialization and cleanup work correctly
    // HOW:  Init, check state, shutdown

    int result = nimcp_exception_immune_init(nullptr);
    EXPECT_EQ(result, 0);

    // Should not be connected to immune system yet
    EXPECT_FALSE(nimcp_exception_immune_is_connected());

    nimcp_exception_immune_shutdown();
}

TEST_F(CoreExceptionTest, AntigenSourceMapping) {
    // WHAT: Test exception category to antigen source mapping
    // WHY:  Verify correct immune antigen types are selected
    // HOW:  Map categories and check sources

    EXPECT_EQ(nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_SECURITY),
              EX_ANTIGEN_SOURCE_BBB);
    EXPECT_EQ(nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_BRAIN),
              EX_ANTIGEN_SOURCE_BBB);
    EXPECT_EQ(nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_THREADING),
              EX_ANTIGEN_SOURCE_BFT);
    EXPECT_EQ(nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_MEMORY),
              EX_ANTIGEN_SOURCE_ANOMALY);
    EXPECT_EQ(nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_IO),
              EX_ANTIGEN_SOURCE_ANOMALY);
}

TEST_F(CoreExceptionTest, ImmuneSeverityMapping) {
    // WHAT: Test exception severity to immune severity mapping
    // WHY:  Verify correct 1-10 scale mapping
    // HOW:  Map severities and check values

    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_DEBUG), 1u);
    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_INFO), 2u);
    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_WARNING), 3u);
    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_ERROR), 5u);
    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_SEVERE), 7u);
    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_CRITICAL), 9u);
    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_FATAL), 10u);
}

TEST_F(CoreExceptionTest, RecoveryStrategy) {
    // WHAT: Test recovery strategy selection for exceptions
    // WHY:  Verify correct recovery strategies are generated
    // HOW:  Create exceptions, get strategies, check actions

    nimcp_exception_t* mem_ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Memory test"
    );
    mem_ex->category = EXCEPTION_CATEGORY_MEMORY;
    mem_ex->type = EXCEPTION_TYPE_MEMORY;

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(mem_ex, &strategy);

    // Memory errors should suggest GC as primary
    EXPECT_EQ(strategy.primary_action, EXCEPTION_RECOVERY_GC);

    nimcp_exception_unref(mem_ex);
}

// ============================================================================
// Epitope Generation Tests
// ============================================================================

TEST_F(CoreExceptionTest, EpitopeGeneration) {
    // WHAT: Test immune epitope generation from exception
    // WHY:  Verify unique fingerprints are generated for pattern matching
    // HOW:  Generate epitope, check it's non-empty

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_BRAIN_CREATION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Epitope test"
    );

    ASSERT_NE(ex, nullptr);

    size_t len = nimcp_exception_generate_epitope(ex);
    EXPECT_GT(len, 0u);
    EXPECT_LE(len, NIMCP_EXCEPTION_EPITOPE_SIZE);
    EXPECT_EQ(ex->epitope_len, len);

    nimcp_exception_unref(ex);
}

// ============================================================================
// Core Module Exception Tests (Brain, Axon, Dendrite, etc.)
// ============================================================================

TEST_F(CoreExceptionTest, BrainModuleExceptions) {
    // WHAT: Test brain-specific exception creation and handling
    // WHY:  Verify brain module errors are properly handled
    // HOW:  Create various brain exceptions, check categorization

    // Forward pass error
    nimcp_brain_exception_t* fwd_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1, "cortex",
        "Forward pass failed at layer 5"
    );
    ASSERT_NE(fwd_ex, nullptr);
    EXPECT_EQ(fwd_ex->base.category, EXCEPTION_CATEGORY_BRAIN);
    EXPECT_EQ(nimcp_exception_get_suggested_recovery((nimcp_exception_t*)fwd_ex),
              EXCEPTION_RECOVERY_ROLLBACK);
    nimcp_exception_unref((nimcp_exception_t*)fwd_ex);

    // Backward pass error
    nimcp_brain_exception_t* bwd_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BACKWARD_PASS,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1, "cortex",
        "Backward pass gradient explosion"
    );
    ASSERT_NE(bwd_ex, nullptr);
    bwd_ex->has_nan_weights = true;
    bwd_ex->gradient_norm = INFINITY;
    EXPECT_EQ(nimcp_exception_get_suggested_recovery((nimcp_exception_t*)bwd_ex),
              EXCEPTION_RECOVERY_ROLLBACK);
    nimcp_exception_unref((nimcp_exception_t*)bwd_ex);
}

TEST_F(CoreExceptionTest, CoreModuleErrorMapping) {
    // WHAT: Test error code ranges for core modules
    // WHY:  Verify all core module errors map to correct categories
    // HOW:  Test error codes from different ranges

    // Brain region errors (10000-19999)
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_MOTOR_BASE),
              EXCEPTION_CATEGORY_BRAIN_REGION);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_HIPPOCAMPUS_BASE),
              EXCEPTION_CATEGORY_BRAIN_REGION);

    // KG wiring errors (3050-3099) - part of brain category
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_KG_WIRING_CREATE),
              EXCEPTION_CATEGORY_BRAIN);
}

// ============================================================================
// NULL Safety Tests
// ============================================================================

TEST_F(CoreExceptionTest, NullSafety) {
    // WHAT: Test NULL handling in all API functions
    // WHY:  Ensure no crashes with NULL inputs
    // HOW:  Call functions with NULL, verify safe behavior

    // These should not crash
    nimcp_exception_unref(nullptr);
    nimcp_exception_set_cause(nullptr, nullptr);
    nimcp_exception_clear_current();

    EXPECT_EQ(nimcp_exception_get_cause(nullptr), nullptr);
    EXPECT_EQ(nimcp_exception_ref(nullptr), nullptr);
    EXPECT_EQ(nimcp_exception_get_context(nullptr, "key"), nullptr);
    EXPECT_EQ(nimcp_exception_set_context(nullptr, "key", "value"), -1);
    EXPECT_EQ(nimcp_exception_context_count(nullptr), 0u);
    EXPECT_EQ(nimcp_aggregate_exception_count(nullptr), 0u);
    EXPECT_EQ(nimcp_aggregate_exception_get(nullptr, 0), nullptr);

    nimcp_handler_disable(nullptr);
    nimcp_handler_enable(nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(nullptr, &strategy);
    EXPECT_EQ(strategy.primary_action, EXCEPTION_RECOVERY_NONE);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
