/**
 * @file test_utils_exception_handling.cpp
 * @brief Unit tests for utils module exception handling integration
 *
 * WHAT: Tests exception handling for memory, threading, containers, platform modules
 * WHY:  Verify exception system properly handles errors from utils modules
 * HOW:  GoogleTest framework testing exception creation, dispatch, and handlers
 *
 * TEST PATTERNS:
 * - Memory allocation failure exceptions
 * - Threading error exceptions (mutex, deadlock)
 * - Container bounds checking exceptions
 * - Platform-specific exception handling
 *
 * @author NIMCP Development Team
 * @date 2025-01-16
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <atomic>
#include <thread>
#include <chrono>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/containers/nimcp_darray.h"
}

//=============================================================================
// Test Fixture Base
//=============================================================================

/**
 * WHAT: Base fixture for exception handling tests
 * WHY:  Setup/teardown exception system for each test
 */
class ExceptionHandlingTestBase : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_exception_system_init();
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_enable_debug_output(false);
    }

    void TearDown() override {
        nimcp_exception_clear_current();
        nimcp_memory_enable_tracking(false);
        nimcp_exception_system_shutdown();
    }
};

//=============================================================================
// Memory Exception Handling Tests
//=============================================================================

/**
 * WHAT: Test fixture for memory-related exception handling
 * WHY:  Isolate memory exception tests with proper cleanup
 */
class MemoryExceptionTest : public ExceptionHandlingTestBase {
protected:
    void SetUp() override {
        ExceptionHandlingTestBase::SetUp();
        nimcp_memory_clear_stats();
    }
};

/**
 * WHAT: Test creating memory allocation exception
 * WHY:  Verify memory exception captures allocation details
 */
TEST_F(MemoryExceptionTest, CreateMemoryAllocationException) {
    size_t requested_size = 1024 * 1024;  // 1 MB

    nimcp_memory_exception_t* mex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__,
        __LINE__,
        __func__,
        requested_size,
        "Failed to allocate %zu bytes",
        requested_size
    );

    ASSERT_NE(mex, nullptr);
    EXPECT_EQ(mex->base.code, NIMCP_ERROR_NO_MEMORY);
    EXPECT_EQ(mex->base.severity, EXCEPTION_SEVERITY_SEVERE);
    EXPECT_EQ(mex->base.type, EXCEPTION_TYPE_MEMORY);
    EXPECT_EQ(mex->base.category, EXCEPTION_CATEGORY_MEMORY);
    EXPECT_EQ(mex->requested_size, requested_size);
    EXPECT_NE(mex->base.timestamp_us, 0UL);

    nimcp_exception_unref((nimcp_exception_t*)mex);
}

/**
 * WHAT: Test memory exception with zero size
 * WHY:  Verify edge case handling for zero allocation
 */
TEST_F(MemoryExceptionTest, CreateMemoryExceptionZeroSize) {
    nimcp_memory_exception_t* mex = nimcp_memory_exception_create(
        NIMCP_ERROR_INVALID_PARAM,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__,
        __LINE__,
        __func__,
        0,
        "Zero-size allocation request"
    );

    ASSERT_NE(mex, nullptr);
    EXPECT_EQ(mex->base.code, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(mex->requested_size, 0);

    nimcp_exception_unref((nimcp_exception_t*)mex);
}

/**
 * WHAT: Test memory exception generates proper epitope
 * WHY:  Verify immune system integration fingerprint
 */
TEST_F(MemoryExceptionTest, MemoryExceptionGeneratesEpitope) {
    nimcp_memory_exception_t* mex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__,
        __LINE__,
        __func__,
        1024,
        "Allocation failed"
    );

    ASSERT_NE(mex, nullptr);

    size_t epitope_len = nimcp_exception_generate_epitope((nimcp_exception_t*)mex);
    EXPECT_GT(epitope_len, 0);
    EXPECT_LE(epitope_len, NIMCP_EXCEPTION_EPITOPE_SIZE);

    // Epitope should be non-zero
    bool has_nonzero = false;
    for (size_t i = 0; i < epitope_len; i++) {
        if (mex->base.epitope[i] != 0) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);

    nimcp_exception_unref((nimcp_exception_t*)mex);
}

/**
 * WHAT: Test memory exception suggested recovery action
 * WHY:  Verify appropriate recovery action for memory errors
 */
TEST_F(MemoryExceptionTest, MemoryExceptionSuggestsRecovery) {
    nimcp_memory_exception_t* mex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__,
        __LINE__,
        __func__,
        1024 * 1024,
        "Out of memory"
    );

    ASSERT_NE(mex, nullptr);

    nimcp_exception_recovery_action_t action = nimcp_exception_get_suggested_recovery(
        (nimcp_exception_t*)mex
    );

    // Memory errors should suggest GC or compact
    EXPECT_TRUE(action == EXCEPTION_RECOVERY_GC ||
                action == EXCEPTION_RECOVERY_COMPACT ||
                action == EXCEPTION_RECOVERY_NONE);

    nimcp_exception_unref((nimcp_exception_t*)mex);
}

/**
 * WHAT: Test double-free exception creation
 * WHY:  Verify proper handling of double-free errors
 */
TEST_F(MemoryExceptionTest, CreateDoubleFreeException) {
    nimcp_memory_exception_t* mex = nimcp_memory_exception_create(
        NIMCP_ERROR_DOUBLE_FREE,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        0,
        "Double free detected at address %p",
        (void*)0xDEADBEEF
    );

    ASSERT_NE(mex, nullptr);
    EXPECT_EQ(mex->base.code, NIMCP_ERROR_DOUBLE_FREE);
    EXPECT_EQ(mex->base.category, EXCEPTION_CATEGORY_MEMORY);

    nimcp_exception_unref((nimcp_exception_t*)mex);
}

/**
 * WHAT: Test buffer overflow exception
 * WHY:  Verify buffer overflow error handling
 */
TEST_F(MemoryExceptionTest, CreateBufferOverflowException) {
    nimcp_memory_exception_t* mex = nimcp_memory_exception_create(
        NIMCP_ERROR_BUFFER_OVERFLOW,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__,
        __LINE__,
        __func__,
        256,
        "Buffer overflow: wrote %zu bytes past end",
        (size_t)16
    );

    ASSERT_NE(mex, nullptr);
    EXPECT_EQ(mex->base.code, NIMCP_ERROR_BUFFER_OVERFLOW);
    EXPECT_EQ(mex->base.severity, EXCEPTION_SEVERITY_CRITICAL);

    nimcp_exception_unref((nimcp_exception_t*)mex);
}

//=============================================================================
// Threading Exception Handling Tests
//=============================================================================

/**
 * WHAT: Test fixture for threading-related exception handling
 * WHY:  Isolate threading exception tests
 */
class ThreadingExceptionTest : public ExceptionHandlingTestBase {
protected:
    void SetUp() override {
        ExceptionHandlingTestBase::SetUp();
        nimcp_thread_init();
    }
};

/**
 * WHAT: Test creating threading exception
 * WHY:  Verify threading exception captures thread details
 */
TEST_F(ThreadingExceptionTest, CreateThreadingException) {
    uint64_t thread_id = 12345;

    nimcp_threading_exception_t* tex = nimcp_threading_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        thread_id,
        "Mutex lock failed on thread %lu",
        (unsigned long)thread_id
    );

    ASSERT_NE(tex, nullptr);
    EXPECT_EQ(tex->base.code, NIMCP_ERROR_OPERATION_FAILED);
    EXPECT_EQ(tex->base.severity, EXCEPTION_SEVERITY_ERROR);
    EXPECT_EQ(tex->base.type, EXCEPTION_TYPE_THREADING);
    EXPECT_EQ(tex->base.category, EXCEPTION_CATEGORY_THREADING);
    EXPECT_EQ(tex->thread_id, thread_id);

    nimcp_exception_unref((nimcp_exception_t*)tex);
}

/**
 * WHAT: Test creating deadlock exception
 * WHY:  Verify deadlock detection exception handling
 */
TEST_F(ThreadingExceptionTest, CreateDeadlockException) {
    nimcp_threading_exception_t* tex = nimcp_threading_exception_create(
        NIMCP_ERROR_DEADLOCK,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__,
        __LINE__,
        __func__,
        9999,
        "Deadlock detected involving %d threads",
        3
    );

    ASSERT_NE(tex, nullptr);
    EXPECT_EQ(tex->base.code, NIMCP_ERROR_DEADLOCK);
    EXPECT_EQ(tex->base.severity, EXCEPTION_SEVERITY_CRITICAL);
    EXPECT_EQ(tex->base.type, EXCEPTION_TYPE_THREADING);

    nimcp_exception_unref((nimcp_exception_t*)tex);
}

/**
 * WHAT: Test threading exception with mutex address
 * WHY:  Verify mutex context is preserved
 */
TEST_F(ThreadingExceptionTest, ThreadingExceptionWithMutexContext) {
    nimcp_threading_exception_t* tex = nimcp_threading_exception_create(
        NIMCP_ERROR_MUTEX_UNLOCK,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        1234,
        "Mutex unlock failed"
    );

    ASSERT_NE(tex, nullptr);

    // Set additional context
    int result = nimcp_exception_set_context(
        (nimcp_exception_t*)tex,
        "mutex_addr",
        "0xABCD1234"
    );
    EXPECT_EQ(result, 0);

    const char* addr = nimcp_exception_get_context((nimcp_exception_t*)tex, "mutex_addr");
    EXPECT_NE(addr, nullptr);
    EXPECT_STREQ(addr, "0xABCD1234");

    nimcp_exception_unref((nimcp_exception_t*)tex);
}

/**
 * WHAT: Test threading exception suggested recovery
 * WHY:  Verify thread-related recovery actions
 */
TEST_F(ThreadingExceptionTest, ThreadingExceptionSuggestsRecovery) {
    nimcp_threading_exception_t* tex = nimcp_threading_exception_create(
        NIMCP_ERROR_THREAD_CREATE,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        0,
        "Thread creation failed"
    );

    ASSERT_NE(tex, nullptr);

    nimcp_exception_recovery_action_t action = nimcp_exception_get_suggested_recovery(
        (nimcp_exception_t*)tex
    );

    // Threading errors might suggest restart
    EXPECT_TRUE(action == EXCEPTION_RECOVERY_RESTART_THREAD ||
                action == EXCEPTION_RECOVERY_RETRY ||
                action == EXCEPTION_RECOVERY_NONE);

    nimcp_exception_unref((nimcp_exception_t*)tex);
}

//=============================================================================
// Container Bounds Exception Tests
//=============================================================================

/**
 * WHAT: Test fixture for container-related exception handling
 * WHY:  Isolate container bounds checking tests
 */
class ContainerExceptionTest : public ExceptionHandlingTestBase {
protected:
    nimcp_darray_t* arr;

    void SetUp() override {
        ExceptionHandlingTestBase::SetUp();
        arr = nimcp_darray_create(sizeof(int), 16);
    }

    void TearDown() override {
        if (arr) {
            nimcp_darray_destroy(arr);
        }
        ExceptionHandlingTestBase::TearDown();
    }
};

/**
 * WHAT: Test creating out-of-bounds exception
 * WHY:  Verify container bounds checking exception handling
 */
TEST_F(ContainerExceptionTest, CreateOutOfBoundsException) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Index %zu out of bounds (size: %zu)",
        (size_t)10,
        (size_t)5
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_ERROR);

    nimcp_exception_unref(ex);
}

/**
 * WHAT: Test container bounds check with context
 * WHY:  Verify context information is preserved
 */
TEST_F(ContainerExceptionTest, ContainerExceptionWithContext) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Array access out of bounds"
    );

    ASSERT_NE(ex, nullptr);

    // Add container-specific context
    nimcp_exception_set_context(ex, "container_type", "darray");
    nimcp_exception_set_context(ex, "index", "10");
    nimcp_exception_set_context(ex, "size", "5");

    EXPECT_EQ(nimcp_exception_context_count(ex), 3);

    const char* type = nimcp_exception_get_context(ex, "container_type");
    EXPECT_NE(type, nullptr);
    EXPECT_STREQ(type, "darray");

    nimcp_exception_unref(ex);
}

/**
 * WHAT: Test buffer too small exception
 * WHY:  Verify buffer size validation exceptions
 */
TEST_F(ContainerExceptionTest, CreateBufferTooSmallException) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_BUFFER_TOO_SMALL,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Buffer size %zu too small, need %zu",
        (size_t)64,
        (size_t)128
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_BUFFER_TOO_SMALL);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Exception Handler Registration Tests
//=============================================================================

/**
 * WHAT: Test fixture for handler registration tests
 * WHY:  Isolate handler registration/unregistration tests
 */
class HandlerRegistrationTest : public ExceptionHandlingTestBase {
protected:
    static std::atomic<int> handler_call_count;
    static nimcp_exception_t* last_handled_exception;

    static bool test_handler(nimcp_exception_t* ex, void* user_data) {
        handler_call_count++;
        last_handled_exception = ex;
        return false;  // Don't consume exception
    }

    void SetUp() override {
        ExceptionHandlingTestBase::SetUp();
        handler_call_count = 0;
        last_handled_exception = nullptr;
    }
};

std::atomic<int> HandlerRegistrationTest::handler_call_count{0};
nimcp_exception_t* HandlerRegistrationTest::last_handled_exception = nullptr;

/**
 * WHAT: Test registering exception handler
 * WHY:  Verify handler registration API
 */
TEST_F(HandlerRegistrationTest, RegisterHandler) {
    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "test_handler";
    options.handler = test_handler;
    options.priority = NIMCP_HANDLER_PRIORITY_NORMAL;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);
    EXPECT_EQ(reg->active, true);

    nimcp_handler_unregister(reg);
}

/**
 * WHAT: Test handler is called on exception dispatch
 * WHY:  Verify exception dispatch invokes handlers
 */
TEST_F(HandlerRegistrationTest, HandlerCalledOnDispatch) {
    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "test_handler";
    options.handler = test_handler;
    options.priority = NIMCP_HANDLER_PRIORITY_NORMAL;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Test exception"
    );

    nimcp_exception_dispatch(ex);

    EXPECT_GT(handler_call_count.load(), 0);
    EXPECT_EQ(last_handled_exception, ex);

    nimcp_handler_unregister(reg);
    nimcp_exception_unref(ex);
}

/**
 * WHAT: Test handler priority ordering
 * WHY:  Verify handlers are called in priority order
 */
TEST_F(HandlerRegistrationTest, HandlerPriorityOrdering) {
    static std::vector<int> call_order;
    call_order.clear();

    auto high_handler = [](nimcp_exception_t*, void*) -> bool {
        call_order.push_back(100);
        return false;
    };
    auto low_handler = [](nimcp_exception_t*, void*) -> bool {
        call_order.push_back(10);
        return false;
    };

    nimcp_handler_options_t low_opts, high_opts;
    nimcp_handler_default_options(&low_opts);
    nimcp_handler_default_options(&high_opts);

    low_opts.name = "low_handler";
    low_opts.handler = low_handler;
    low_opts.priority = NIMCP_HANDLER_PRIORITY_LOW;

    high_opts.name = "high_handler";
    high_opts.handler = high_handler;
    high_opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;

    // Register low first, then high
    nimcp_handler_registration_t* low_reg = nimcp_handler_register(&low_opts);
    nimcp_handler_registration_t* high_reg = nimcp_handler_register(&high_opts);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Test"
    );

    nimcp_exception_dispatch(ex);

    // High priority should be called first
    EXPECT_GE(call_order.size(), 2);
    if (call_order.size() >= 2) {
        EXPECT_EQ(call_order[0], 100);  // High first
        EXPECT_EQ(call_order[1], 10);   // Low second
    }

    nimcp_handler_unregister(low_reg);
    nimcp_handler_unregister(high_reg);
    nimcp_exception_unref(ex);
}

/**
 * WHAT: Test handler category filtering
 * WHY:  Verify handlers can filter by exception category
 */
TEST_F(HandlerRegistrationTest, HandlerCategoryFiltering) {
    static bool memory_handler_called = false;
    auto memory_handler = [](nimcp_exception_t*, void*) -> bool {
        memory_handler_called = true;
        return false;
    };

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "memory_only_handler";
    options.handler = memory_handler;
    options.category_filter = EXCEPTION_CATEGORY_MEMORY;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);

    // Dispatch non-memory exception
    memory_handler_called = false;
    nimcp_threading_exception_t* tex = nimcp_threading_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        1234,
        "Test"
    );
    nimcp_exception_dispatch((nimcp_exception_t*)tex);
    EXPECT_FALSE(memory_handler_called);  // Should not be called

    // Dispatch memory exception
    nimcp_memory_exception_t* mex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        1024,
        "Test"
    );
    nimcp_exception_dispatch((nimcp_exception_t*)mex);
    EXPECT_TRUE(memory_handler_called);  // Should be called

    nimcp_handler_unregister(reg);
    nimcp_exception_unref((nimcp_exception_t*)tex);
    nimcp_exception_unref((nimcp_exception_t*)mex);
}

/**
 * WHAT: Test disabling and enabling handler
 * WHY:  Verify handler enable/disable API
 */
TEST_F(HandlerRegistrationTest, DisableEnableHandler) {
    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "toggle_handler";
    options.handler = test_handler;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    EXPECT_TRUE(reg->active);

    nimcp_handler_disable(reg);
    EXPECT_FALSE(reg->active);

    nimcp_handler_enable(reg);
    EXPECT_TRUE(reg->active);

    nimcp_handler_unregister(reg);
}

//=============================================================================
// Exception Context Tests
//=============================================================================

/**
 * WHAT: Test fixture for exception context tests
 * WHY:  Isolate context key-value tests
 */
class ExceptionContextTest : public ExceptionHandlingTestBase {};

/**
 * WHAT: Test setting and getting context
 * WHY:  Verify context API works correctly
 */
TEST_F(ExceptionContextTest, SetGetContext) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Test"
    );

    int result = nimcp_exception_set_context(ex, "key1", "value1");
    EXPECT_EQ(result, 0);

    const char* value = nimcp_exception_get_context(ex, "key1");
    EXPECT_NE(value, nullptr);
    EXPECT_STREQ(value, "value1");

    nimcp_exception_unref(ex);
}

/**
 * WHAT: Test multiple context entries
 * WHY:  Verify multiple key-value pairs work
 */
TEST_F(ExceptionContextTest, MultipleContextEntries) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Test"
    );

    nimcp_exception_set_context(ex, "key1", "value1");
    nimcp_exception_set_context(ex, "key2", "value2");
    nimcp_exception_set_context(ex, "key3", "value3");

    EXPECT_EQ(nimcp_exception_context_count(ex), 3);

    EXPECT_STREQ(nimcp_exception_get_context(ex, "key1"), "value1");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "key2"), "value2");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "key3"), "value3");

    nimcp_exception_unref(ex);
}

/**
 * WHAT: Test removing context entry
 * WHY:  Verify context removal works
 */
TEST_F(ExceptionContextTest, RemoveContext) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Test"
    );

    nimcp_exception_set_context(ex, "key1", "value1");
    EXPECT_EQ(nimcp_exception_context_count(ex), 1);

    int result = nimcp_exception_remove_context(ex, "key1");
    EXPECT_EQ(result, 0);
    EXPECT_EQ(nimcp_exception_context_count(ex), 0);
    EXPECT_EQ(nimcp_exception_get_context(ex, "key1"), nullptr);

    nimcp_exception_unref(ex);
}

/**
 * WHAT: Test context with NULL parameters
 * WHY:  Verify error handling for NULL inputs
 */
TEST_F(ExceptionContextTest, ContextNullHandling) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Test"
    );

    EXPECT_EQ(nimcp_exception_set_context(ex, nullptr, "value"), -1);
    EXPECT_EQ(nimcp_exception_set_context(ex, "key", nullptr), -1);
    EXPECT_EQ(nimcp_exception_set_context(nullptr, "key", "value"), -1);

    EXPECT_EQ(nimcp_exception_get_context(ex, nullptr), nullptr);
    EXPECT_EQ(nimcp_exception_get_context(nullptr, "key"), nullptr);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Exception Chaining Tests
//=============================================================================

/**
 * WHAT: Test fixture for exception chaining tests
 * WHY:  Isolate cause chain tests
 */
class ExceptionChainingTest : public ExceptionHandlingTestBase {};

/**
 * WHAT: Test setting exception cause
 * WHY:  Verify exception chaining works
 */
TEST_F(ExceptionChainingTest, SetCause) {
    nimcp_exception_t* cause = nimcp_exception_create(
        NIMCP_ERROR_FILE_NOT_FOUND,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "File not found"
    );

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Operation failed"
    );

    nimcp_exception_set_cause(ex, cause);

    nimcp_exception_t* retrieved_cause = nimcp_exception_get_cause(ex);
    EXPECT_EQ(retrieved_cause, cause);

    nimcp_exception_unref(ex);
    // cause is automatically unref'd when ex is freed
}

/**
 * WHAT: Test exception chain traversal
 * WHY:  Verify we can walk the cause chain
 */
TEST_F(ExceptionChainingTest, ChainTraversal) {
    nimcp_exception_t* root = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Root cause"
    );

    nimcp_exception_t* middle = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Middle"
    );
    nimcp_exception_set_cause(middle, root);

    nimcp_exception_t* top = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Top level"
    );
    nimcp_exception_set_cause(top, middle);

    // Walk the chain
    int depth = 0;
    nimcp_exception_t* current = top;
    while (current != nullptr) {
        depth++;
        current = nimcp_exception_get_cause(current);
    }

    EXPECT_EQ(depth, 3);

    nimcp_exception_unref(top);
}

//=============================================================================
// Reference Counting Tests
//=============================================================================

/**
 * WHAT: Test fixture for reference counting tests
 * WHY:  Isolate refcount tests
 */
class RefCountTest : public ExceptionHandlingTestBase {};

/**
 * WHAT: Test reference counting
 * WHY:  Verify ref/unref works correctly
 */
TEST_F(RefCountTest, BasicRefCounting) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Test"
    );

    EXPECT_EQ(ex->ref_count, 1);

    nimcp_exception_ref(ex);
    EXPECT_EQ(ex->ref_count, 2);

    nimcp_exception_unref(ex);
    EXPECT_EQ(ex->ref_count, 1);

    nimcp_exception_unref(ex);
    // ex is now freed
}

/**
 * WHAT: Test multiple references
 * WHY:  Verify multiple refs/unrefs work
 */
TEST_F(RefCountTest, MultipleReferences) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Test"
    );

    for (int i = 0; i < 10; i++) {
        nimcp_exception_ref(ex);
    }
    EXPECT_EQ(ex->ref_count, 11);

    for (int i = 0; i < 10; i++) {
        nimcp_exception_unref(ex);
    }
    EXPECT_EQ(ex->ref_count, 1);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Thread-Local Current Exception Tests
//=============================================================================

/**
 * WHAT: Test fixture for thread-local exception tests
 * WHY:  Isolate thread-local storage tests
 */
class ThreadLocalExceptionTest : public ExceptionHandlingTestBase {};

/**
 * WHAT: Test set/get current exception
 * WHY:  Verify thread-local exception storage
 */
TEST_F(ThreadLocalExceptionTest, SetGetCurrent) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Test"
    );

    nimcp_exception_set_current(ex);

    nimcp_exception_t* current = nimcp_exception_get_current();
    EXPECT_EQ(current, ex);

    nimcp_exception_clear_current();
    EXPECT_EQ(nimcp_exception_get_current(), nullptr);

    nimcp_exception_unref(ex);
}

/**
 * WHAT: Test clear current exception
 * WHY:  Verify clear works correctly
 */
TEST_F(ThreadLocalExceptionTest, ClearCurrent) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Test"
    );

    nimcp_exception_set_current(ex);
    EXPECT_NE(nimcp_exception_get_current(), nullptr);

    nimcp_exception_clear_current();
    EXPECT_EQ(nimcp_exception_get_current(), nullptr);

    nimcp_exception_unref(ex);
}

//=============================================================================
// String Conversion Tests
//=============================================================================

/**
 * WHAT: Test fixture for string conversion tests
 * WHY:  Isolate to_string tests
 */
class StringConversionTest : public ExceptionHandlingTestBase {};

/**
 * WHAT: Test exception to string conversion
 * WHY:  Verify exception formatting works
 */
TEST_F(StringConversionTest, ExceptionToString) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Null pointer exception"
    );

    char buffer[1024];
    size_t len = nimcp_exception_to_string(ex, buffer, sizeof(buffer));

    EXPECT_GT(len, 0);
    EXPECT_NE(strstr(buffer, "NULL"), nullptr);  // Should mention null pointer

    nimcp_exception_unref(ex);
}

/**
 * WHAT: Test severity to string conversion
 * WHY:  Verify severity enum conversion
 */
TEST_F(StringConversionTest, SeverityToString) {
    const char* debug = nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_DEBUG);
    const char* info = nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_INFO);
    const char* warning = nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_WARNING);
    const char* error = nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_ERROR);
    const char* severe = nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_SEVERE);
    const char* critical = nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_CRITICAL);
    const char* fatal = nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_FATAL);

    EXPECT_NE(debug, nullptr);
    EXPECT_NE(info, nullptr);
    EXPECT_NE(warning, nullptr);
    EXPECT_NE(error, nullptr);
    EXPECT_NE(severe, nullptr);
    EXPECT_NE(critical, nullptr);
    EXPECT_NE(fatal, nullptr);
}

/**
 * WHAT: Test category to string conversion
 * WHY:  Verify category enum conversion
 */
TEST_F(StringConversionTest, CategoryToString) {
    const char* generic = nimcp_exception_category_to_string(EXCEPTION_CATEGORY_GENERIC);
    const char* memory = nimcp_exception_category_to_string(EXCEPTION_CATEGORY_MEMORY);
    const char* brain = nimcp_exception_category_to_string(EXCEPTION_CATEGORY_BRAIN);
    const char* io = nimcp_exception_category_to_string(EXCEPTION_CATEGORY_IO);
    const char* threading = nimcp_exception_category_to_string(EXCEPTION_CATEGORY_THREADING);

    EXPECT_NE(generic, nullptr);
    EXPECT_NE(memory, nullptr);
    EXPECT_NE(brain, nullptr);
    EXPECT_NE(io, nullptr);
    EXPECT_NE(threading, nullptr);
}

/**
 * WHAT: Test recovery action to string conversion
 * WHY:  Verify recovery action enum conversion
 */
TEST_F(StringConversionTest, RecoveryActionToString) {
    const char* none = nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_NONE);
    const char* retry = nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_RETRY);
    const char* gc = nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_GC);
    const char* rollback = nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_ROLLBACK);

    EXPECT_NE(none, nullptr);
    EXPECT_NE(retry, nullptr);
    EXPECT_NE(gc, nullptr);
    EXPECT_NE(rollback, nullptr);
}

//=============================================================================
// Aggregate Exception Tests
//=============================================================================

/**
 * WHAT: Test fixture for aggregate exception tests
 * WHY:  Isolate aggregate exception tests
 */
class AggregateExceptionTest : public ExceptionHandlingTestBase {};

/**
 * WHAT: Test creating aggregate exception
 * WHY:  Verify aggregate exception creation
 */
TEST_F(AggregateExceptionTest, CreateAggregate) {
    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Multiple errors occurred"
    );

    ASSERT_NE(agg, nullptr);
    EXPECT_EQ(agg->base.type, EXCEPTION_TYPE_AGGREGATE);
    EXPECT_EQ(agg->child_count, 0);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

/**
 * WHAT: Test adding children to aggregate
 * WHY:  Verify child addition works
 */
TEST_F(AggregateExceptionTest, AddChildren) {
    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Batch errors"
    );

    nimcp_exception_t* child1 = nimcp_exception_create(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Child 1"
    );

    nimcp_exception_t* child2 = nimcp_exception_create(
        NIMCP_ERROR_INVALID_PARAM,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Child 2"
    );

    int result1 = nimcp_aggregate_exception_add(agg, child1);
    int result2 = nimcp_aggregate_exception_add(agg, child2);

    EXPECT_EQ(result1, 0);
    EXPECT_EQ(result2, 0);
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 2);

    nimcp_exception_t* retrieved1 = nimcp_aggregate_exception_get(agg, 0);
    nimcp_exception_t* retrieved2 = nimcp_aggregate_exception_get(agg, 1);

    EXPECT_EQ(retrieved1, child1);
    EXPECT_EQ(retrieved2, child2);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

/**
 * WHAT: Test aggregate with max children
 * WHY:  Verify aggregate limits work
 */
TEST_F(AggregateExceptionTest, MaxChildrenLimit) {
    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Batch errors"
    );

    // Add up to max children
    for (int i = 0; i < NIMCP_EXCEPTION_MAX_CHILDREN; i++) {
        nimcp_exception_t* child = nimcp_exception_create(
            NIMCP_ERROR_UNKNOWN,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__,
            __LINE__,
            __func__,
            "Child %d",
            i
        );
        int result = nimcp_aggregate_exception_add(agg, child);
        EXPECT_EQ(result, 0);
    }

    EXPECT_EQ(nimcp_aggregate_exception_count(agg), NIMCP_EXCEPTION_MAX_CHILDREN);

    // Adding one more should fail
    nimcp_exception_t* extra = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Extra"
    );
    int result = nimcp_aggregate_exception_add(agg, extra);
    EXPECT_EQ(result, -1);  // Should fail

    nimcp_exception_unref(extra);
    nimcp_exception_unref((nimcp_exception_t*)agg);
}

//=============================================================================
// Platform-Specific Exception Tests
//=============================================================================

/**
 * WHAT: Test fixture for platform-specific tests
 * WHY:  Test platform layer exception handling
 */
class PlatformExceptionTest : public ExceptionHandlingTestBase {};

/**
 * WHAT: Test IO exception creation
 * WHY:  Verify I/O exception handling
 */
TEST_F(PlatformExceptionTest, CreateIOException) {
    nimcp_io_exception_t* iex = nimcp_io_exception_create(
        NIMCP_ERROR_FILE_READ,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "/etc/nonexistent",
        "Failed to read file"
    );

    ASSERT_NE(iex, nullptr);
    EXPECT_EQ(iex->base.type, EXCEPTION_TYPE_IO);
    EXPECT_EQ(iex->base.category, EXCEPTION_CATEGORY_IO);
    EXPECT_STREQ(iex->path, "/etc/nonexistent");

    nimcp_exception_unref((nimcp_exception_t*)iex);
}

/**
 * WHAT: Test security exception creation
 * WHY:  Verify security exception handling
 */
TEST_F(PlatformExceptionTest, CreateSecurityException) {
    nimcp_security_exception_t* sex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__,
        __LINE__,
        __func__,
        1,  // threat_type
        "Security threat detected"
    );

    ASSERT_NE(sex, nullptr);
    EXPECT_EQ(sex->base.type, EXCEPTION_TYPE_SECURITY);
    EXPECT_EQ(sex->base.category, EXCEPTION_CATEGORY_SECURITY);
    EXPECT_EQ(sex->threat_type, 1);

    nimcp_exception_unref((nimcp_exception_t*)sex);
}

/**
 * WHAT: Test GPU exception creation
 * WHY:  Verify GPU exception handling
 */
TEST_F(PlatformExceptionTest, CreateGPUException) {
    nimcp_gpu_exception_t* gex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__,
        __LINE__,
        __func__,
        0,   // device_id
        700, // cuda_error (hypothetical)
        "CUDA kernel launch failed"
    );

    ASSERT_NE(gex, nullptr);
    EXPECT_EQ(gex->base.type, EXCEPTION_TYPE_GPU);
    EXPECT_EQ(gex->base.category, EXCEPTION_CATEGORY_GPU);
    EXPECT_EQ(gex->device_id, 0);
    EXPECT_EQ(gex->cuda_error, 700);

    nimcp_exception_unref((nimcp_exception_t*)gex);
}
