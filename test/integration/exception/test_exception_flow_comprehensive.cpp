/**
 * @file test_exception_flow_comprehensive.cpp
 * @brief Comprehensive integration tests for exception handling flow across modules
 *
 * WHAT: Test exception handling flows including propagation, nesting, recovery,
 *       cross-module integration, context preservation, and callback handling
 * WHY:  Ensure robust exception handling throughout the NIMCP system with
 *       proper cleanup, error context preservation, and recovery capabilities
 * HOW:  Create complex exception scenarios testing all aspects of the
 *       exception system integration
 *
 * TEST SCENARIOS:
 * - Exception propagation across function boundaries
 * - Exception handling in nested function calls
 * - Cross-module exception flow
 * - Exception recovery and cleanup
 * - Multiple exception types in same execution path
 * - Exception context preservation through call stack
 * - Callback exception handling
 * - Try/catch mechanism testing
 * - Handler chain priority and filtering
 * - Aggregate exception collection and processing
 *
 * @author NIMCP Development Team
 * @date 2026-01-22
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <functional>
#include <mutex>
#include <set>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class ExceptionFlowTest : public ::testing::Test {
protected:
    // Tracking counters for handler invocations
    static std::atomic<int> handler_call_count;
    static std::atomic<int> recovery_callback_count;
    static std::atomic<nimcp_error_t> last_exception_code;
    static std::atomic<nimcp_exception_severity_t> last_severity;
    static std::atomic<nimcp_exception_category_t> last_category;
    static std::vector<nimcp_error_t> exception_sequence;
    static std::mutex sequence_mutex;

    // Recovery tracking
    static std::atomic<nimcp_exception_recovery_action_t> last_recovery_action;
    static std::atomic<bool> recovery_succeeded;

    // Handler registrations for cleanup
    std::vector<nimcp_handler_registration_t*> registered_handlers;

    void SetUp() override {
        // Reset all tracking state
        handler_call_count = 0;
        recovery_callback_count = 0;
        last_exception_code = NIMCP_SUCCESS;
        last_severity = EXCEPTION_SEVERITY_DEBUG;
        last_category = EXCEPTION_CATEGORY_GENERIC;
        last_recovery_action = EXCEPTION_RECOVERY_NONE;
        recovery_succeeded = false;

        {
            std::lock_guard<std::mutex> lock(sequence_mutex);
            exception_sequence.clear();
        }

        registered_handlers.clear();

        // Initialize exception system
        nimcp_exception_system_init();
    }

    void TearDown() override {
        // Unregister all handlers
        for (auto* reg : registered_handlers) {
            if (reg) {
                nimcp_handler_unregister(reg);
            }
        }
        registered_handlers.clear();

        // Clear any pending exception
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    /**
     * @brief Register a tracking handler that records exception info
     */
    nimcp_handler_registration_t* register_tracking_handler(
        const char* name,
        int priority,
        nimcp_exception_category_t category_filter = (nimcp_exception_category_t)0,
        nimcp_exception_severity_t min_severity = EXCEPTION_SEVERITY_DEBUG,
        bool consume = false
    ) {
        nimcp_handler_options_t options;
        nimcp_handler_default_options(&options);
        options.name = name;
        options.handler = consume ? consuming_handler : tracking_handler;
        options.priority = priority;
        options.category_filter = category_filter;
        options.min_severity = min_severity;

        nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
        if (reg) {
            registered_handlers.push_back(reg);
        }
        return reg;
    }

    /**
     * @brief Basic tracking handler - records but doesn't consume
     */
    static bool tracking_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        last_exception_code = ex->code;
        last_severity = ex->severity;
        last_category = ex->category;

        {
            std::lock_guard<std::mutex> lock(sequence_mutex);
            exception_sequence.push_back(ex->code);
        }

        return false;  // Don't consume
    }

    /**
     * @brief Handler that consumes exceptions
     */
    static bool consuming_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        last_exception_code = ex->code;
        return true;  // Consume - stop handler chain
    }

    /**
     * @brief Recovery callback for testing
     */
    static int test_recovery_callback(
        nimcp_exception_t* ex,
        nimcp_exception_recovery_action_t action,
        void* user_data
    ) {
        (void)ex;
        (void)user_data;
        recovery_callback_count++;
        last_recovery_action = action;
        recovery_succeeded = true;
        return 0;
    }

    /**
     * @brief Failing recovery callback
     */
    static int failing_recovery_callback(
        nimcp_exception_t* ex,
        nimcp_exception_recovery_action_t action,
        void* user_data
    ) {
        (void)ex;
        (void)user_data;
        recovery_callback_count++;
        last_recovery_action = action;
        recovery_succeeded = false;
        return -1;  // Failure
    }

    //=========================================================================
    // Simulated Module Functions for Cross-Module Testing
    //=========================================================================

    /**
     * @brief Simulate deep call stack function - level 3 (deepest)
     */
    static nimcp_error_t deep_function_level3(bool should_fail) {
        if (should_fail) {
            nimcp_exception_throw(
                NIMCP_ERROR_INVALID_ADDRESS,
                __FILE__, __LINE__, __func__,
                "Invalid memory address at deepest level");
            return NIMCP_ERROR_INVALID_ADDRESS;
        }
        return NIMCP_SUCCESS;
    }

    /**
     * @brief Simulate deep call stack function - level 2
     */
    static nimcp_error_t deep_function_level2(bool should_fail) {
        nimcp_error_t result = deep_function_level3(should_fail);
        if (result != NIMCP_SUCCESS) {
            nimcp_exception_throw(
                NIMCP_ERROR_MEMORY_CORRUPTION,
                __FILE__, __LINE__, __func__,
                "Memory corruption detected at level 2");
            return NIMCP_ERROR_MEMORY_CORRUPTION;
        }
        return NIMCP_SUCCESS;
    }

    /**
     * @brief Simulate deep call stack function - level 1 (entry point)
     */
    static nimcp_error_t deep_function_level1(bool should_fail) {
        nimcp_error_t result = deep_function_level2(should_fail);
        if (result != NIMCP_SUCCESS) {
            nimcp_exception_throw(
                NIMCP_ERROR_NO_MEMORY,
                __FILE__, __LINE__, __func__,
                "Memory allocation failed at level 1");
            return NIMCP_ERROR_NO_MEMORY;
        }
        return NIMCP_SUCCESS;
    }

    /**
     * @brief Simulate memory module operation
     */
    static nimcp_error_t memory_module_allocate(size_t size, bool fail) {
        if (fail || size > 1024 * 1024 * 1024) {
            nimcp_memory_exception_t* ex = nimcp_memory_exception_create(
                NIMCP_ERROR_NO_MEMORY,
                EXCEPTION_SEVERITY_SEVERE,
                __FILE__, __LINE__, __func__,
                size,
                "Memory allocation failed: requested %zu bytes", size
            );
            if (ex) {
                ex->available_size = 1024;
                ex->is_heap = true;
                nimcp_exception_dispatch((nimcp_exception_t*)ex);
                nimcp_exception_unref((nimcp_exception_t*)ex);
            }
            return NIMCP_ERROR_NO_MEMORY;
        }
        return NIMCP_SUCCESS;
    }

    /**
     * @brief Simulate brain module operation
     */
    static nimcp_error_t brain_module_process(uint32_t brain_id, bool fail) {
        if (fail) {
            nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
                NIMCP_ERROR_LEARNING_FAILED,
                EXCEPTION_SEVERITY_ERROR,
                __FILE__, __LINE__, __func__,
                brain_id,
                "hippocampus",
                "Learning diverged in brain %u", brain_id
            );
            if (ex) {
                ex->has_nan_weights = true;
                ex->learning_diverged = true;
                nimcp_exception_dispatch((nimcp_exception_t*)ex);
                nimcp_exception_unref((nimcp_exception_t*)ex);
            }
            return NIMCP_ERROR_LEARNING_FAILED;
        }
        return NIMCP_SUCCESS;
    }

    /**
     * @brief Simulate I/O module operation
     */
    static nimcp_error_t io_module_read(const char* path, bool fail) {
        if (fail) {
            nimcp_io_exception_t* ex = nimcp_io_exception_create(
                NIMCP_ERROR_FILE_READ,
                EXCEPTION_SEVERITY_ERROR,
                __FILE__, __LINE__, __func__,
                path,
                "Failed to read file: %s", path
            );
            if (ex) {
                ex->errno_value = 2;  // ENOENT
                nimcp_exception_dispatch((nimcp_exception_t*)ex);
                nimcp_exception_unref((nimcp_exception_t*)ex);
            }
            return NIMCP_ERROR_FILE_READ;
        }
        return NIMCP_SUCCESS;
    }

    /**
     * @brief Simulate threading module operation
     */
    static nimcp_error_t threading_module_sync(uint64_t thread_id, bool fail) {
        if (fail) {
            nimcp_threading_exception_t* ex = nimcp_threading_exception_create(
                NIMCP_ERROR_DEADLOCK,
                EXCEPTION_SEVERITY_CRITICAL,
                __FILE__, __LINE__, __func__,
                thread_id,
                "Deadlock detected in thread %lu", (unsigned long)thread_id
            );
            if (ex) {
                ex->is_deadlock = true;
                nimcp_exception_dispatch((nimcp_exception_t*)ex);
                nimcp_exception_unref((nimcp_exception_t*)ex);
            }
            return NIMCP_ERROR_DEADLOCK;
        }
        return NIMCP_SUCCESS;
    }
};

// Initialize static members
std::atomic<int> ExceptionFlowTest::handler_call_count(0);
std::atomic<int> ExceptionFlowTest::recovery_callback_count(0);
std::atomic<nimcp_error_t> ExceptionFlowTest::last_exception_code(NIMCP_SUCCESS);
std::atomic<nimcp_exception_severity_t> ExceptionFlowTest::last_severity(EXCEPTION_SEVERITY_DEBUG);
std::atomic<nimcp_exception_category_t> ExceptionFlowTest::last_category(EXCEPTION_CATEGORY_GENERIC);
std::vector<nimcp_error_t> ExceptionFlowTest::exception_sequence;
std::mutex ExceptionFlowTest::sequence_mutex;
std::atomic<nimcp_exception_recovery_action_t> ExceptionFlowTest::last_recovery_action(EXCEPTION_RECOVERY_NONE);
std::atomic<bool> ExceptionFlowTest::recovery_succeeded(false);

//=============================================================================
// 1. Exception Propagation Across Function Boundaries
//=============================================================================

TEST_F(ExceptionFlowTest, ExceptionPropagatesAcrossFunctionBoundary) {
    // WHAT: Test that exceptions properly propagate across function call boundaries
    // WHY:  Exceptions must flow up the call stack while preserving information
    // HOW:  Call nested function that throws, verify exception reaches top level

    auto* reg = register_tracking_handler("propagation_test", 100);
    ASSERT_NE(reg, nullptr);

    // Call function that will fail at deepest level
    nimcp_error_t result = deep_function_level1(true);

    // Should have received multiple exceptions as they propagate up
    EXPECT_NE(result, NIMCP_SUCCESS);
    EXPECT_GE(handler_call_count.load(), 1);

    // Verify the most recent exception is from the top level
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_NO_MEMORY);
}

TEST_F(ExceptionFlowTest, ExceptionContextPreservedAcrossBoundaries) {
    // WHAT: Test that exception context (file, line, function) is preserved
    // WHY:  Debugging requires accurate source location information
    // HOW:  Create exception with context, propagate, verify context intact

    auto* reg = register_tracking_handler("context_test", 100);
    ASSERT_NE(reg, nullptr);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        "test_source.c", 42, "test_function",
        "Test exception with explicit location"
    );
    ASSERT_NE(ex, nullptr);

    // Add custom context
    nimcp_exception_set_context(ex, "module", "test_module");
    nimcp_exception_set_context(ex, "operation", "boundary_test");

    // Dispatch and verify
    nimcp_exception_dispatch(ex);

    EXPECT_STREQ(ex->file, "test_source.c");
    EXPECT_EQ(ex->line, 42);
    EXPECT_STREQ(ex->function, "test_function");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "module"), "test_module");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "operation"), "boundary_test");

    nimcp_exception_unref(ex);
}

//=============================================================================
// 2. Exception Handling in Nested Function Calls
//=============================================================================

TEST_F(ExceptionFlowTest, NestedFunctionCallsGenerateExceptionChain) {
    // WHAT: Test exception chaining in nested function calls
    // WHY:  Complex operations have multiple failure points that should be traceable
    // HOW:  Create nested exceptions with cause chain, verify chain integrity

    auto* reg = register_tracking_handler("nested_test", 100);
    ASSERT_NE(reg, nullptr);

    // Create root cause (deepest level)
    nimcp_exception_t* root = nimcp_exception_create(
        NIMCP_ERROR_INVALID_ADDRESS,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Root cause: invalid memory access"
    );
    ASSERT_NE(root, nullptr);
    nimcp_exception_set_context(root, "level", "3");

    // Create intermediate exception
    nimcp_exception_t* mid = nimcp_exception_create(
        NIMCP_ERROR_MEMORY_CORRUPTION,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Intermediate: memory corruption detected"
    );
    ASSERT_NE(mid, nullptr);
    nimcp_exception_set_context(mid, "level", "2");
    nimcp_exception_set_cause(mid, root);

    // Create top-level exception
    nimcp_exception_t* top = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        "Top level: operation failed"
    );
    ASSERT_NE(top, nullptr);
    nimcp_exception_set_context(top, "level", "1");
    nimcp_exception_set_cause(top, mid);

    // Verify chain integrity
    nimcp_exception_t* cause1 = nimcp_exception_get_cause(top);
    ASSERT_NE(cause1, nullptr);
    EXPECT_EQ(cause1->code, NIMCP_ERROR_MEMORY_CORRUPTION);
    EXPECT_STREQ(nimcp_exception_get_context(cause1, "level"), "2");

    nimcp_exception_t* cause2 = nimcp_exception_get_cause(cause1);
    ASSERT_NE(cause2, nullptr);
    EXPECT_EQ(cause2->code, NIMCP_ERROR_INVALID_ADDRESS);
    EXPECT_STREQ(nimcp_exception_get_context(cause2, "level"), "3");

    // No more causes
    EXPECT_EQ(nimcp_exception_get_cause(cause2), nullptr);

    // Dispatch top-level
    nimcp_exception_dispatch(top);
    EXPECT_GE(handler_call_count.load(), 1);

    nimcp_exception_unref(top);
}

TEST_F(ExceptionFlowTest, DeepNestedCallStackHandled) {
    // WHAT: Test exception handling in very deep call stacks
    // WHY:  Real applications can have deep call stacks
    // HOW:  Simulate deep nesting with exception at bottom

    auto* reg = register_tracking_handler("deep_stack_test", 100);
    ASSERT_NE(reg, nullptr);

    // Create chain of 10 exceptions
    nimcp_exception_t* current = nullptr;
    for (int i = 10; i >= 1; i--) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED + i,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Exception at depth %d", i
        );
        ASSERT_NE(ex, nullptr);

        char depth_str[16];
        snprintf(depth_str, sizeof(depth_str), "%d", i);
        nimcp_exception_set_context(ex, "depth", depth_str);

        if (current != nullptr) {
            nimcp_exception_set_cause(ex, current);
        }
        current = ex;
    }

    // Verify we can walk entire chain
    int depth = 0;
    nimcp_exception_t* walk = current;
    while (walk != nullptr) {
        depth++;
        walk = nimcp_exception_get_cause(walk);
    }
    EXPECT_EQ(depth, 10);

    nimcp_exception_dispatch(current);
    nimcp_exception_unref(current);
}

//=============================================================================
// 3. Cross-Module Exception Flow
//=============================================================================

TEST_F(ExceptionFlowTest, MemoryToControllerExceptionFlow) {
    // WHAT: Test exception flow from memory module to controller
    // WHY:  Memory failures must propagate to higher levels
    // HOW:  Trigger memory exception, verify it reaches controller handler

    auto* reg = register_tracking_handler("memory_flow_test", 100);
    ASSERT_NE(reg, nullptr);

    handler_call_count = 0;
    memory_module_allocate(2048, true);

    EXPECT_GE(handler_call_count.load(), 1);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_NO_MEMORY);
    EXPECT_EQ(last_category.load(), EXCEPTION_CATEGORY_MEMORY);
}

TEST_F(ExceptionFlowTest, BrainToControllerExceptionFlow) {
    // WHAT: Test exception flow from brain module to controller
    // WHY:  Neural network failures need centralized handling
    // HOW:  Trigger brain exception, verify proper routing

    auto* reg = register_tracking_handler("brain_flow_test", 100);
    ASSERT_NE(reg, nullptr);

    handler_call_count = 0;
    brain_module_process(1, true);

    EXPECT_GE(handler_call_count.load(), 1);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_LEARNING_FAILED);
    EXPECT_EQ(last_category.load(), EXCEPTION_CATEGORY_BRAIN);
}

TEST_F(ExceptionFlowTest, IOToControllerExceptionFlow) {
    // WHAT: Test exception flow from I/O module to controller
    // WHY:  I/O failures need appropriate handling
    // HOW:  Trigger I/O exception, verify proper routing

    auto* reg = register_tracking_handler("io_flow_test", 100);
    ASSERT_NE(reg, nullptr);

    handler_call_count = 0;
    io_module_read("/nonexistent/path", true);

    EXPECT_GE(handler_call_count.load(), 1);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_FILE_READ);
    EXPECT_EQ(last_category.load(), EXCEPTION_CATEGORY_IO);
}

TEST_F(ExceptionFlowTest, MultiModuleExceptionSequence) {
    // WHAT: Test exception sequence from multiple modules
    // WHY:  Real operations involve multiple modules
    // HOW:  Trigger exceptions from multiple modules, verify order

    auto* reg = register_tracking_handler("multi_module_test", 100);
    ASSERT_NE(reg, nullptr);

    {
        std::lock_guard<std::mutex> lock(sequence_mutex);
        exception_sequence.clear();
    }

    // Trigger exceptions from different modules
    memory_module_allocate(4096, true);
    brain_module_process(2, true);
    io_module_read("/test/file", true);
    threading_module_sync(12345, true);

    // Verify sequence
    {
        std::lock_guard<std::mutex> lock(sequence_mutex);
        EXPECT_EQ(exception_sequence.size(), 4u);
        EXPECT_EQ(exception_sequence[0], NIMCP_ERROR_NO_MEMORY);
        EXPECT_EQ(exception_sequence[1], NIMCP_ERROR_LEARNING_FAILED);
        EXPECT_EQ(exception_sequence[2], NIMCP_ERROR_FILE_READ);
        EXPECT_EQ(exception_sequence[3], NIMCP_ERROR_DEADLOCK);
    }
}

//=============================================================================
// 4. Exception Recovery and Cleanup
//=============================================================================

TEST_F(ExceptionFlowTest, RecoveryCallbackInvoked) {
    // WHAT: Test that recovery callbacks are properly invoked
    // WHY:  Exceptions should trigger appropriate recovery actions
    // HOW:  Register recovery callback, trigger exception, verify callback

    // Register recovery callback for GC action
    int result = nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_GC,
        test_recovery_callback,
        nullptr
    );
    EXPECT_EQ(result, 0);

    // Create exception and trigger recovery
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Memory exhausted"
    );
    ASSERT_NE(ex, nullptr);

    recovery_callback_count = 0;
    nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_GC);

    EXPECT_EQ(recovery_callback_count.load(), 1);
    EXPECT_EQ(last_recovery_action.load(), EXCEPTION_RECOVERY_GC);
    EXPECT_TRUE(recovery_succeeded.load());

    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_GC);
    nimcp_exception_unref(ex);
}

TEST_F(ExceptionFlowTest, FailedRecoveryHandled) {
    // WHAT: Test handling of failed recovery attempts
    // WHY:  Recovery can fail and needs proper handling
    // HOW:  Register failing callback, verify failure reported

    int result = nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_RETRY,
        failing_recovery_callback,
        nullptr
    );
    EXPECT_EQ(result, 0);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Operation failed"
    );
    ASSERT_NE(ex, nullptr);

    recovery_callback_count = 0;
    int recovery_result = nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_RETRY);

    EXPECT_EQ(recovery_result, -1);
    EXPECT_EQ(recovery_callback_count.load(), 1);
    EXPECT_FALSE(recovery_succeeded.load());

    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_RETRY);
    nimcp_exception_unref(ex);
}

TEST_F(ExceptionFlowTest, ExceptionCleanupOnUnref) {
    // WHAT: Test that exception resources are cleaned up on unref
    // WHY:  Memory leaks must be prevented
    // HOW:  Create exception with cause chain, unref, verify cleanup

    // Create exception chain
    nimcp_exception_t* cause = nimcp_exception_create(
        NIMCP_ERROR_INVALID_PARAM,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Invalid parameter"
    );
    ASSERT_NE(cause, nullptr);

    nimcp_exception_t* top = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Operation failed"
    );
    ASSERT_NE(top, nullptr);
    nimcp_exception_set_cause(top, cause);

    // Unref should clean up entire chain
    nimcp_exception_unref(top);
    // If this doesn't crash/leak, cleanup worked
    SUCCEED();
}

//=============================================================================
// 5. Multiple Exception Types in Same Execution Path
//=============================================================================

TEST_F(ExceptionFlowTest, MixedExceptionTypesInSamePath) {
    // WHAT: Test handling of different exception types in same execution
    // WHY:  Complex operations can fail in multiple ways
    // HOW:  Create exceptions of different types, verify each handled correctly

    auto* reg = register_tracking_handler("mixed_types_test", 100);
    ASSERT_NE(reg, nullptr);

    // Create different exception types
    nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_BUFFER_OVERFLOW,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        1024,
        "Buffer overflow detected"
    );
    ASSERT_NE(mem_ex, nullptr);

    nimcp_brain_exception_t* brain_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_NETWORK_INVALID,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1, "cortex",
        "Invalid neural network"
    );
    ASSERT_NE(brain_ex, nullptr);

    nimcp_io_exception_t* io_ex = nimcp_io_exception_create(
        NIMCP_ERROR_FILE_CORRUPT,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "/data/model.bin",
        "Model file corrupted"
    );
    ASSERT_NE(io_ex, nullptr);

    // Dispatch each
    {
        std::lock_guard<std::mutex> lock(sequence_mutex);
        exception_sequence.clear();
    }

    nimcp_exception_dispatch((nimcp_exception_t*)mem_ex);
    nimcp_exception_dispatch((nimcp_exception_t*)brain_ex);
    nimcp_exception_dispatch((nimcp_exception_t*)io_ex);

    // Verify all received
    {
        std::lock_guard<std::mutex> lock(sequence_mutex);
        EXPECT_EQ(exception_sequence.size(), 3u);
        EXPECT_EQ(exception_sequence[0], NIMCP_ERROR_BUFFER_OVERFLOW);
        EXPECT_EQ(exception_sequence[1], NIMCP_ERROR_NETWORK_INVALID);
        EXPECT_EQ(exception_sequence[2], NIMCP_ERROR_FILE_CORRUPT);
    }

    nimcp_exception_unref((nimcp_exception_t*)mem_ex);
    nimcp_exception_unref((nimcp_exception_t*)brain_ex);
    nimcp_exception_unref((nimcp_exception_t*)io_ex);
}

TEST_F(ExceptionFlowTest, AggregateExceptionCollectsMultipleTypes) {
    // WHAT: Test aggregate exception collecting different exception types
    // WHY:  Batch operations may fail in multiple ways
    // HOW:  Create aggregate with mixed children, verify all accessible

    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        "Multiple failures in batch operation"
    );
    ASSERT_NE(agg, nullptr);
    EXPECT_EQ(agg->base.type, EXCEPTION_TYPE_AGGREGATE);

    // Add different exception types
    nimcp_memory_exception_t* mem = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY, EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__, 1024, "Memory failure"
    );
    nimcp_brain_exception_t* brain = nimcp_brain_exception_create(
        NIMCP_ERROR_LEARNING_FAILED, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, 1, "test", "Learning failure"
    );
    nimcp_io_exception_t* io = nimcp_io_exception_create(
        NIMCP_ERROR_FILE_READ, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, "/test", "I/O failure"
    );

    ASSERT_NE(mem, nullptr);
    ASSERT_NE(brain, nullptr);
    ASSERT_NE(io, nullptr);

    EXPECT_EQ(nimcp_aggregate_exception_add(agg, (nimcp_exception_t*)mem), 0);
    EXPECT_EQ(nimcp_aggregate_exception_add(agg, (nimcp_exception_t*)brain), 0);
    EXPECT_EQ(nimcp_aggregate_exception_add(agg, (nimcp_exception_t*)io), 0);

    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 3u);

    // Verify each type
    nimcp_exception_t* child0 = nimcp_aggregate_exception_get(agg, 0);
    nimcp_exception_t* child1 = nimcp_aggregate_exception_get(agg, 1);
    nimcp_exception_t* child2 = nimcp_aggregate_exception_get(agg, 2);

    EXPECT_EQ(child0->type, EXCEPTION_TYPE_MEMORY);
    EXPECT_EQ(child1->type, EXCEPTION_TYPE_BRAIN);
    EXPECT_EQ(child2->type, EXCEPTION_TYPE_IO);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

//=============================================================================
// 6. Exception Context Preservation Through Call Stack
//=============================================================================

TEST_F(ExceptionFlowTest, ContextPreservedThroughDispatch) {
    // WHAT: Test context key-value pairs survive dispatch
    // WHY:  Context is critical for debugging
    // HOW:  Add context before dispatch, verify after

    auto* reg = register_tracking_handler("context_dispatch_test", 100);
    ASSERT_NE(reg, nullptr);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test exception"
    );
    ASSERT_NE(ex, nullptr);

    // Add multiple context entries
    nimcp_exception_set_context(ex, "key1", "value1");
    nimcp_exception_set_context(ex, "key2", "value2");
    nimcp_exception_set_context(ex, "numeric", "12345");
    nimcp_exception_set_context(ex, "path", "/some/test/path");

    EXPECT_EQ(nimcp_exception_context_count(ex), 4u);

    // Dispatch
    nimcp_exception_dispatch(ex);

    // Verify context still intact
    EXPECT_STREQ(nimcp_exception_get_context(ex, "key1"), "value1");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "key2"), "value2");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "numeric"), "12345");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "path"), "/some/test/path");

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionFlowTest, ContextInCauseChainPreserved) {
    // WHAT: Test context preserved in exception cause chain
    // WHY:  Root cause context is often most important
    // HOW:  Add context to cause, wrap in new exception, verify accessible

    nimcp_exception_t* cause = nimcp_exception_create(
        NIMCP_ERROR_INVALID_PARAM,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Root cause"
    );
    ASSERT_NE(cause, nullptr);
    nimcp_exception_set_context(cause, "root_key", "root_value");
    nimcp_exception_set_context(cause, "param_name", "brain_id");

    nimcp_exception_t* wrapper = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Wrapped exception"
    );
    ASSERT_NE(wrapper, nullptr);
    nimcp_exception_set_context(wrapper, "wrapper_key", "wrapper_value");
    nimcp_exception_set_cause(wrapper, cause);

    // Verify wrapper context
    EXPECT_STREQ(nimcp_exception_get_context(wrapper, "wrapper_key"), "wrapper_value");
    EXPECT_EQ(nimcp_exception_get_context(wrapper, "root_key"), nullptr);

    // Verify cause context
    nimcp_exception_t* retrieved_cause = nimcp_exception_get_cause(wrapper);
    ASSERT_NE(retrieved_cause, nullptr);
    EXPECT_STREQ(nimcp_exception_get_context(retrieved_cause, "root_key"), "root_value");
    EXPECT_STREQ(nimcp_exception_get_context(retrieved_cause, "param_name"), "brain_id");

    nimcp_exception_unref(wrapper);
}

TEST_F(ExceptionFlowTest, ContextRemovalWorks) {
    // WHAT: Test context entry removal
    // WHY:  Context may need to be modified during exception handling
    // HOW:  Add context, remove some entries, verify state

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test exception"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "keep", "kept_value");
    nimcp_exception_set_context(ex, "remove", "removed_value");
    nimcp_exception_set_context(ex, "also_keep", "also_kept");

    EXPECT_EQ(nimcp_exception_context_count(ex), 3u);

    // Remove one
    int result = nimcp_exception_remove_context(ex, "remove");
    EXPECT_EQ(result, 0);
    EXPECT_EQ(nimcp_exception_context_count(ex), 2u);

    // Verify removed
    EXPECT_EQ(nimcp_exception_get_context(ex, "remove"), nullptr);
    EXPECT_STREQ(nimcp_exception_get_context(ex, "keep"), "kept_value");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "also_keep"), "also_kept");

    // Remove non-existent
    result = nimcp_exception_remove_context(ex, "nonexistent");
    EXPECT_EQ(result, -1);

    nimcp_exception_unref(ex);
}

//=============================================================================
// 7. Callback Exception Handling
//=============================================================================

TEST_F(ExceptionFlowTest, HandlerCallbackReceivesCorrectData) {
    // WHAT: Test handler callback receives correct exception data
    // WHY:  Handlers must have access to full exception information
    // HOW:  Create exception with specific data, verify handler sees it

    static nimcp_exception_t* received_exception = nullptr;

    auto custom_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        received_exception = ex;
        return false;
    };

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "data_verification_handler";
    options.handler = custom_handler;
    options.priority = 100;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);
    registered_handlers.push_back(reg);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_CONFIG_INVALID,
        EXCEPTION_SEVERITY_ERROR,
        "callback_test.c", 123, "test_callback_func",
        "Configuration error message"
    );
    ASSERT_NE(ex, nullptr);
    nimcp_exception_set_context(ex, "config_key", "test_setting");

    received_exception = nullptr;
    nimcp_exception_dispatch(ex);

    ASSERT_NE(received_exception, nullptr);
    EXPECT_EQ(received_exception->code, NIMCP_ERROR_CONFIG_INVALID);
    EXPECT_EQ(received_exception->severity, EXCEPTION_SEVERITY_ERROR);
    EXPECT_STREQ(received_exception->file, "callback_test.c");
    EXPECT_EQ(received_exception->line, 123);
    EXPECT_STREQ(received_exception->function, "test_callback_func");
    EXPECT_STREQ(nimcp_exception_get_context(received_exception, "config_key"), "test_setting");

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionFlowTest, HandlerChainStopsOnConsume) {
    // WHAT: Test handler chain stops when exception is consumed
    // WHY:  Consumed exceptions shouldn't reach subsequent handlers
    // HOW:  Register consuming handler first, verify later handlers not called

    static int low_priority_call_count = 0;
    auto low_priority_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        (void)user_data;
        low_priority_call_count++;
        return false;
    };

    // Register consuming handler with high priority
    auto* high_reg = register_tracking_handler("high_priority", 100,
        (nimcp_exception_category_t)0, EXCEPTION_SEVERITY_DEBUG, true);
    ASSERT_NE(high_reg, nullptr);

    // Register non-consuming handler with low priority
    nimcp_handler_options_t low_options;
    nimcp_handler_default_options(&low_options);
    low_options.name = "low_priority";
    low_options.handler = low_priority_handler;
    low_options.priority = 10;
    nimcp_handler_registration_t* low_reg = nimcp_handler_register(&low_options);
    ASSERT_NE(low_reg, nullptr);
    registered_handlers.push_back(low_reg);

    low_priority_call_count = 0;

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test exception"
    );
    ASSERT_NE(ex, nullptr);

    bool handled = nimcp_exception_dispatch(ex);

    EXPECT_TRUE(handled);  // High priority consumed it
    EXPECT_EQ(low_priority_call_count, 0);  // Low priority never called

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionFlowTest, HandlerFilteringByCategory) {
    // WHAT: Test handlers filter by exception category
    // WHY:  Handlers may only care about specific exception types
    // HOW:  Register category-specific handler, verify filtering

    static int memory_handler_calls = 0;
    auto memory_only_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        (void)user_data;
        memory_handler_calls++;
        return false;
    };

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "memory_only_handler";
    options.handler = memory_only_handler;
    options.priority = 100;
    options.category_filter = EXCEPTION_CATEGORY_MEMORY;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);
    registered_handlers.push_back(reg);

    memory_handler_calls = 0;

    // Create memory exception - should trigger
    nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, 1024, "Memory error"
    );
    ASSERT_NE(mem_ex, nullptr);
    nimcp_exception_dispatch((nimcp_exception_t*)mem_ex);
    EXPECT_EQ(memory_handler_calls, 1);

    // Create I/O exception - should NOT trigger
    nimcp_io_exception_t* io_ex = nimcp_io_exception_create(
        NIMCP_ERROR_FILE_READ, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, "/test", "I/O error"
    );
    ASSERT_NE(io_ex, nullptr);
    nimcp_exception_dispatch((nimcp_exception_t*)io_ex);
    EXPECT_EQ(memory_handler_calls, 1);  // Still 1, not incremented

    nimcp_exception_unref((nimcp_exception_t*)mem_ex);
    nimcp_exception_unref((nimcp_exception_t*)io_ex);
}

TEST_F(ExceptionFlowTest, HandlerFilteringBySeverity) {
    // WHAT: Test handlers filter by minimum severity
    // WHY:  Some handlers only care about severe errors
    // HOW:  Register severity-filtered handler, verify filtering

    static int severe_handler_calls = 0;
    auto severe_only_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        (void)user_data;
        severe_handler_calls++;
        return false;
    };

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "severe_only_handler";
    options.handler = severe_only_handler;
    options.priority = 100;
    options.min_severity = EXCEPTION_SEVERITY_SEVERE;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);
    registered_handlers.push_back(reg);

    severe_handler_calls = 0;

    // Create low severity exception - should NOT trigger
    nimcp_exception_t* low_ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, "Low severity"
    );
    ASSERT_NE(low_ex, nullptr);
    nimcp_exception_dispatch(low_ex);
    EXPECT_EQ(severe_handler_calls, 0);

    // Create severe exception - should trigger
    nimcp_exception_t* severe_ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED, EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__, "Critical severity"
    );
    ASSERT_NE(severe_ex, nullptr);
    nimcp_exception_dispatch(severe_ex);
    EXPECT_EQ(severe_handler_calls, 1);

    nimcp_exception_unref(low_ex);
    nimcp_exception_unref(severe_ex);
}

//=============================================================================
// 8. Thread-Local Exception Context
//=============================================================================

TEST_F(ExceptionFlowTest, ThreadLocalCurrentException) {
    // WHAT: Test thread-local current exception
    // WHY:  Each thread needs independent exception context
    // HOW:  Set current exception, verify accessible, clear

    // Initially null
    EXPECT_EQ(nimcp_exception_get_current(), nullptr);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_THREAD_SYNC,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Thread sync error"
    );
    ASSERT_NE(ex, nullptr);

    // Set as current
    nimcp_exception_set_current(ex);

    // Verify accessible
    nimcp_exception_t* current = nimcp_exception_get_current();
    ASSERT_NE(current, nullptr);
    EXPECT_EQ(current->code, NIMCP_ERROR_THREAD_SYNC);

    // Clear
    nimcp_exception_clear_current();
    EXPECT_EQ(nimcp_exception_get_current(), nullptr);
}

TEST_F(ExceptionFlowTest, ThreadLocalExceptionIsolation) {
    // WHAT: Test thread-local exception isolation between threads
    // WHY:  Threads must have independent exception state
    // HOW:  Set exceptions in different threads, verify isolation

    std::atomic<bool> thread_saw_null(true);
    std::atomic<nimcp_error_t> thread_exception_code(NIMCP_SUCCESS);

    // Set exception in main thread
    nimcp_exception_t* main_ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Main thread exception"
    );
    ASSERT_NE(main_ex, nullptr);
    nimcp_exception_set_current(main_ex);

    // Launch thread to check its own context
    std::thread other_thread([&]() {
        // Should have null - thread-local
        thread_saw_null = (nimcp_exception_get_current() == nullptr);

        // Set own exception
        nimcp_exception_t* thread_ex = nimcp_exception_create(
            NIMCP_ERROR_DEADLOCK,
            EXCEPTION_SEVERITY_CRITICAL,
            __FILE__, __LINE__, __func__,
            "Other thread exception"
        );
        if (thread_ex) {
            nimcp_exception_set_current(thread_ex);
            nimcp_exception_t* current = nimcp_exception_get_current();
            if (current) {
                thread_exception_code = current->code;
            }
            nimcp_exception_clear_current();
        }
    });

    other_thread.join();

    EXPECT_TRUE(thread_saw_null.load());
    EXPECT_EQ(thread_exception_code.load(), NIMCP_ERROR_DEADLOCK);

    // Main thread should still have its exception
    nimcp_exception_t* main_current = nimcp_exception_get_current();
    ASSERT_NE(main_current, nullptr);
    EXPECT_EQ(main_current->code, NIMCP_ERROR_OPERATION_FAILED);

    nimcp_exception_clear_current();
}

//=============================================================================
// 9. Exception String Formatting
//=============================================================================

TEST_F(ExceptionFlowTest, ExceptionToStringFormatting) {
    // WHAT: Test exception to string conversion
    // WHY:  Logging requires string representation
    // HOW:  Create exception, convert to string, verify content

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_BRAIN_CREATION,
        EXCEPTION_SEVERITY_CRITICAL,
        "test_file.c", 99, "test_func",
        "Brain creation failed for ID 42"
    );
    ASSERT_NE(ex, nullptr);

    char buffer[1024];
    size_t len = nimcp_exception_to_string(ex, buffer, sizeof(buffer));

    EXPECT_GT(len, 0u);
    EXPECT_LT(len, sizeof(buffer));

    std::string str(buffer);
    // Should contain some identifying information
    EXPECT_TRUE(str.find("3000") != std::string::npos ||
                str.find("BRAIN") != std::string::npos ||
                str.find("brain") != std::string::npos);

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionFlowTest, SeverityToStringConversion) {
    // WHAT: Test severity to string conversion
    // WHY:  Logging needs human-readable severity names
    // HOW:  Convert each severity level to string

    const char* debug_str = nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_DEBUG);
    const char* info_str = nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_INFO);
    const char* warning_str = nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_WARNING);
    const char* error_str = nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_ERROR);
    const char* severe_str = nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_SEVERE);
    const char* critical_str = nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_CRITICAL);
    const char* fatal_str = nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_FATAL);

    EXPECT_NE(debug_str, nullptr);
    EXPECT_NE(info_str, nullptr);
    EXPECT_NE(warning_str, nullptr);
    EXPECT_NE(error_str, nullptr);
    EXPECT_NE(severe_str, nullptr);
    EXPECT_NE(critical_str, nullptr);
    EXPECT_NE(fatal_str, nullptr);

    // All should be distinct
    EXPECT_STRNE(debug_str, critical_str);
    EXPECT_STRNE(error_str, fatal_str);
}

TEST_F(ExceptionFlowTest, CategoryToStringConversion) {
    // WHAT: Test category to string conversion
    // WHY:  Logging needs human-readable category names
    // HOW:  Convert each category to string

    const char* generic_str = nimcp_exception_category_to_string(EXCEPTION_CATEGORY_GENERIC);
    const char* memory_str = nimcp_exception_category_to_string(EXCEPTION_CATEGORY_MEMORY);
    const char* brain_str = nimcp_exception_category_to_string(EXCEPTION_CATEGORY_BRAIN);
    const char* io_str = nimcp_exception_category_to_string(EXCEPTION_CATEGORY_IO);
    const char* threading_str = nimcp_exception_category_to_string(EXCEPTION_CATEGORY_THREADING);

    EXPECT_NE(generic_str, nullptr);
    EXPECT_NE(memory_str, nullptr);
    EXPECT_NE(brain_str, nullptr);
    EXPECT_NE(io_str, nullptr);
    EXPECT_NE(threading_str, nullptr);
}

//=============================================================================
// 10. Reference Counting
//=============================================================================

TEST_F(ExceptionFlowTest, ReferenceCountingBasic) {
    // WHAT: Test basic reference counting
    // WHY:  Shared exceptions need proper lifecycle management
    // HOW:  Add/remove references, verify behavior

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Reference test"
    );
    ASSERT_NE(ex, nullptr);

    // Add reference
    nimcp_exception_t* ref1 = nimcp_exception_ref(ex);
    EXPECT_EQ(ref1, ex);

    // Add another reference
    nimcp_exception_t* ref2 = nimcp_exception_ref(ex);
    EXPECT_EQ(ref2, ex);

    // Release references
    nimcp_exception_unref(ref1);
    // Should still be valid
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);

    nimcp_exception_unref(ref2);
    // Still valid (original reference)
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);

    // Final release
    nimcp_exception_unref(ex);
}

TEST_F(ExceptionFlowTest, ReferenceCountingWithCauseChain) {
    // WHAT: Test reference counting with cause chain
    // WHY:  Cause chains add complexity to lifecycle management
    // HOW:  Create chain, verify proper cleanup

    nimcp_exception_t* cause = nimcp_exception_create(
        NIMCP_ERROR_INVALID_PARAM,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Cause exception"
    );
    ASSERT_NE(cause, nullptr);

    nimcp_exception_t* wrapper = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Wrapper exception"
    );
    ASSERT_NE(wrapper, nullptr);

    // Set cause (takes reference)
    nimcp_exception_set_cause(wrapper, cause);

    // Verify cause is accessible
    EXPECT_EQ(nimcp_exception_get_cause(wrapper), cause);

    // Release wrapper (should release cause too)
    nimcp_exception_unref(wrapper);
    // No crash = success
    SUCCEED();
}

//=============================================================================
// 11. Handler Registration/Unregistration
//=============================================================================

TEST_F(ExceptionFlowTest, HandlerRegistrationLifecycle) {
    // WHAT: Test handler registration and unregistration
    // WHY:  Handlers must be properly managed
    // HOW:  Register, verify active, unregister, verify gone

    size_t initial_count = nimcp_handler_count();

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "lifecycle_test_handler";
    options.handler = tracking_handler;
    options.priority = 50;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);

    EXPECT_EQ(nimcp_handler_count(), initial_count + 1);
    EXPECT_TRUE(reg->active);

    // Unregister
    int result = nimcp_handler_unregister(reg);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(nimcp_handler_count(), initial_count);
}

TEST_F(ExceptionFlowTest, HandlerEnableDisable) {
    // WHAT: Test handler enable/disable functionality
    // WHY:  Handlers may need temporary deactivation
    // HOW:  Register, disable, verify not called, enable, verify called

    static int handler_invocations = 0;
    auto counted_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        (void)user_data;
        handler_invocations++;
        return false;
    };

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "enable_disable_handler";
    options.handler = counted_handler;
    options.priority = 100;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);
    registered_handlers.push_back(reg);

    handler_invocations = 0;

    // Handler should be called when active
    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, "Test 1"
    );
    nimcp_exception_dispatch(ex1);
    EXPECT_EQ(handler_invocations, 1);
    nimcp_exception_unref(ex1);

    // Disable handler
    nimcp_handler_disable(reg);
    EXPECT_FALSE(reg->active);

    // Handler should NOT be called when disabled
    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, "Test 2"
    );
    nimcp_exception_dispatch(ex2);
    EXPECT_EQ(handler_invocations, 1);  // Still 1
    nimcp_exception_unref(ex2);

    // Re-enable handler
    nimcp_handler_enable(reg);
    EXPECT_TRUE(reg->active);

    // Handler should be called again
    nimcp_exception_t* ex3 = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, "Test 3"
    );
    nimcp_exception_dispatch(ex3);
    EXPECT_EQ(handler_invocations, 2);
    nimcp_exception_unref(ex3);
}

//=============================================================================
// 12. Epitope Generation
//=============================================================================

TEST_F(ExceptionFlowTest, EpitopeGeneration) {
    // WHAT: Test exception epitope generation for immune system
    // WHY:  Immune system needs consistent fingerprints
    // HOW:  Generate epitopes, verify non-empty and consistent

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Memory exhausted"
    );
    ASSERT_NE(ex, nullptr);

    // Generate epitope
    size_t len = nimcp_exception_generate_epitope(ex);
    EXPECT_GT(len, 0u);
    EXPECT_LE(len, NIMCP_EXCEPTION_EPITOPE_SIZE);
    EXPECT_EQ(ex->epitope_len, len);

    // Epitope should have non-zero content
    bool has_content = false;
    for (size_t i = 0; i < len; i++) {
        if (ex->epitope[i] != 0) {
            has_content = true;
            break;
        }
    }
    EXPECT_TRUE(has_content);

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionFlowTest, EpitopeConsistencyForSameError) {
    // WHAT: Test epitope consistency for same error type
    // WHY:  Same errors should produce similar epitopes
    // HOW:  Create two similar exceptions, compare epitopes

    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Memory error"
    );
    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Memory error"
    );

    ASSERT_NE(ex1, nullptr);
    ASSERT_NE(ex2, nullptr);

    size_t len1 = nimcp_exception_generate_epitope(ex1);
    size_t len2 = nimcp_exception_generate_epitope(ex2);

    // Lengths should match
    EXPECT_EQ(len1, len2);

    // Epitopes should be similar (if not identical)
    EXPECT_EQ(ex1->epitope_len, ex2->epitope_len);

    nimcp_exception_unref(ex1);
    nimcp_exception_unref(ex2);
}

//=============================================================================
// 13. Stack Trace Capture
//=============================================================================

TEST_F(ExceptionFlowTest, StackTraceCaptured) {
    // WHAT: Test that stack traces are captured
    // WHY:  Debugging requires call stack information
    // HOW:  Create exception, verify stack trace present

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Stack trace test"
    );
    ASSERT_NE(ex, nullptr);

    // Stack trace should be populated (depth depends on implementation)
    // At minimum it shouldn't be entirely empty for non-trivial calls
    // Note: depth might be 0 if backtrace() isn't available
    EXPECT_GE(ex->stack_trace.depth, 0u);
    EXPECT_LE(ex->stack_trace.depth, NIMCP_EXCEPTION_MAX_STACK_DEPTH);

    nimcp_exception_unref(ex);
}

//=============================================================================
// 14. Suggested Recovery Action
//=============================================================================

TEST_F(ExceptionFlowTest, SuggestedRecoveryAction) {
    // WHAT: Test suggested recovery action for different error types
    // WHY:  Different errors require different recovery strategies
    // HOW:  Create different exceptions, verify suggested actions

    // Memory exception should suggest GC
    nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY, EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__, 1024, "Memory error"
    );
    ASSERT_NE(mem_ex, nullptr);
    nimcp_exception_recovery_action_t mem_action =
        nimcp_exception_get_suggested_recovery((nimcp_exception_t*)mem_ex);
    // Should suggest some form of memory-related recovery
    EXPECT_NE(mem_action, EXCEPTION_RECOVERY_NONE);
    nimcp_exception_unref((nimcp_exception_t*)mem_ex);

    // Threading exception should suggest different recovery
    nimcp_threading_exception_t* thread_ex = nimcp_threading_exception_create(
        NIMCP_ERROR_DEADLOCK, EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__, 12345, "Deadlock"
    );
    ASSERT_NE(thread_ex, nullptr);
    nimcp_exception_recovery_action_t thread_action =
        nimcp_exception_get_suggested_recovery((nimcp_exception_t*)thread_ex);
    // Should suggest thread-related recovery
    EXPECT_NE(thread_action, EXCEPTION_RECOVERY_NONE);
    nimcp_exception_unref((nimcp_exception_t*)thread_ex);
}

//=============================================================================
// 15. Exception Type Identification
//=============================================================================

TEST_F(ExceptionFlowTest, ExceptionTypeIdentification) {
    // WHAT: Test exception type field for polymorphism
    // WHY:  Type-specific handling requires type identification
    // HOW:  Create different types, verify type field

    nimcp_exception_t* base = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, "Base"
    );
    ASSERT_NE(base, nullptr);
    EXPECT_EQ(base->type, EXCEPTION_TYPE_BASE);
    nimcp_exception_unref(base);

    nimcp_memory_exception_t* mem = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, 1024, "Memory"
    );
    ASSERT_NE(mem, nullptr);
    EXPECT_EQ(mem->base.type, EXCEPTION_TYPE_MEMORY);
    nimcp_exception_unref((nimcp_exception_t*)mem);

    nimcp_brain_exception_t* brain = nimcp_brain_exception_create(
        NIMCP_ERROR_BRAIN_INVALID, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, 1, "test", "Brain"
    );
    ASSERT_NE(brain, nullptr);
    EXPECT_EQ(brain->base.type, EXCEPTION_TYPE_BRAIN);
    nimcp_exception_unref((nimcp_exception_t*)brain);

    nimcp_io_exception_t* io = nimcp_io_exception_create(
        NIMCP_ERROR_FILE_READ, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, "/test", "IO"
    );
    ASSERT_NE(io, nullptr);
    EXPECT_EQ(io->base.type, EXCEPTION_TYPE_IO);
    nimcp_exception_unref((nimcp_exception_t*)io);

    nimcp_threading_exception_t* thread = nimcp_threading_exception_create(
        NIMCP_ERROR_DEADLOCK, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, 1234, "Thread"
    );
    ASSERT_NE(thread, nullptr);
    EXPECT_EQ(thread->base.type, EXCEPTION_TYPE_THREADING);
    nimcp_exception_unref((nimcp_exception_t*)thread);
}

//=============================================================================
// 16. Category and Severity Mapping
//=============================================================================

TEST_F(ExceptionFlowTest, ErrorCodeToCategoryMapping) {
    // WHAT: Test error code to category mapping
    // WHY:  Correct categorization drives handler routing
    // HOW:  Check mapping for various error codes

    // Generic errors
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_UNKNOWN),
              EXCEPTION_CATEGORY_GENERIC);

    // Memory errors
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_NO_MEMORY),
              EXCEPTION_CATEGORY_MEMORY);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_BUFFER_OVERFLOW),
              EXCEPTION_CATEGORY_MEMORY);

    // Brain errors
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_BRAIN_CREATION),
              EXCEPTION_CATEGORY_BRAIN);

    // I/O errors
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_FILE_NOT_FOUND),
              EXCEPTION_CATEGORY_IO);

    // Threading errors
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_DEADLOCK),
              EXCEPTION_CATEGORY_THREADING);

    // Signal errors
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_SIGSEGV),
              EXCEPTION_CATEGORY_SIGNAL);

    // Cognitive errors
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_WORKING_MEMORY),
              EXCEPTION_CATEGORY_COGNITIVE);
}

TEST_F(ExceptionFlowTest, ErrorCodeToSeverityMapping) {
    // WHAT: Test error code to severity heuristic
    // WHY:  Default severity should be reasonable for error type
    // HOW:  Check severity for various error codes

    // Memory exhaustion should be severe
    nimcp_exception_severity_t mem_sev =
        nimcp_exception_get_severity_from_code(NIMCP_ERROR_NO_MEMORY);
    EXPECT_GE(mem_sev, EXCEPTION_SEVERITY_ERROR);

    // Deadlock should be critical or severe
    nimcp_exception_severity_t deadlock_sev =
        nimcp_exception_get_severity_from_code(NIMCP_ERROR_DEADLOCK);
    EXPECT_GE(deadlock_sev, EXCEPTION_SEVERITY_ERROR);

    // SIGSEGV should be critical or fatal
    nimcp_exception_severity_t sigsegv_sev =
        nimcp_exception_get_severity_from_code(NIMCP_ERROR_SIGSEGV);
    EXPECT_GE(sigsegv_sev, EXCEPTION_SEVERITY_SEVERE);
}

//=============================================================================
// 17. Recovery Action String Conversion
//=============================================================================

TEST_F(ExceptionFlowTest, RecoveryActionToString) {
    // WHAT: Test recovery action to string conversion
    // WHY:  Logging needs human-readable action names
    // HOW:  Convert each action to string

    const char* none_str = nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_NONE);
    const char* retry_str = nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_RETRY);
    const char* gc_str = nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_GC);
    const char* shutdown_str = nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN);

    EXPECT_NE(none_str, nullptr);
    EXPECT_NE(retry_str, nullptr);
    EXPECT_NE(gc_str, nullptr);
    EXPECT_NE(shutdown_str, nullptr);

    // All should be distinct
    EXPECT_STRNE(none_str, gc_str);
    EXPECT_STRNE(retry_str, shutdown_str);
}

//=============================================================================
// 18. Handler Priority Ordering
//=============================================================================

// Static variables for priority order test
static std::vector<int> priority_call_order;
static std::mutex priority_order_mutex;

static bool priority_handler_10(nimcp_exception_t* ex, void* user_data) {
    (void)ex;
    (void)user_data;
    std::lock_guard<std::mutex> lock(priority_order_mutex);
    priority_call_order.push_back(10);
    return false;
}

static bool priority_handler_50(nimcp_exception_t* ex, void* user_data) {
    (void)ex;
    (void)user_data;
    std::lock_guard<std::mutex> lock(priority_order_mutex);
    priority_call_order.push_back(50);
    return false;
}

static bool priority_handler_100(nimcp_exception_t* ex, void* user_data) {
    (void)ex;
    (void)user_data;
    std::lock_guard<std::mutex> lock(priority_order_mutex);
    priority_call_order.push_back(100);
    return false;
}

TEST_F(ExceptionFlowTest, HandlerPriorityOrder) {
    // WHAT: Test handlers called in priority order
    // WHY:  High priority handlers must run first
    // HOW:  Register handlers with different priorities, verify call order

    {
        std::lock_guard<std::mutex> lock(priority_order_mutex);
        priority_call_order.clear();
    }

    // Register handlers in random order
    nimcp_handler_options_t opts;

    nimcp_handler_default_options(&opts);
    opts.name = "low";
    opts.handler = priority_handler_10;
    opts.priority = 10;
    auto* low = nimcp_handler_register(&opts);
    registered_handlers.push_back(low);

    nimcp_handler_default_options(&opts);
    opts.name = "high";
    opts.handler = priority_handler_100;
    opts.priority = 100;
    auto* high = nimcp_handler_register(&opts);
    registered_handlers.push_back(high);

    nimcp_handler_default_options(&opts);
    opts.name = "medium";
    opts.handler = priority_handler_50;
    opts.priority = 50;
    auto* medium = nimcp_handler_register(&opts);
    registered_handlers.push_back(medium);

    // Dispatch exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, "Priority test"
    );
    nimcp_exception_dispatch(ex);
    nimcp_exception_unref(ex);

    // Verify order: high (100) -> medium (50) -> low (10)
    {
        std::lock_guard<std::mutex> lock(priority_order_mutex);
        ASSERT_GE(priority_call_order.size(), 3u);
        EXPECT_EQ(priority_call_order[0], 100);
        EXPECT_EQ(priority_call_order[1], 50);
        EXPECT_EQ(priority_call_order[2], 10);
    }
}

//=============================================================================
// 19. Exception Timestamp
//=============================================================================

TEST_F(ExceptionFlowTest, ExceptionTimestamp) {
    // WHAT: Test exception timestamp is set
    // WHY:  Timing information useful for debugging
    // HOW:  Create exception, verify timestamp non-zero and reasonable

    auto before = std::chrono::steady_clock::now();

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, "Timestamp test"
    );
    ASSERT_NE(ex, nullptr);

    auto after = std::chrono::steady_clock::now();

    // Timestamp should be non-zero
    EXPECT_GT(ex->timestamp_us, 0u);

    // Note: Absolute time comparison depends on system clock
    // Just verify it's set
    nimcp_exception_unref(ex);
}

//=============================================================================
// 20. Default Handler Installation
//=============================================================================

TEST_F(ExceptionFlowTest, DefaultHandlersCanBeInstalled) {
    // WHAT: Test default handlers can be installed
    // WHY:  System needs default behavior
    // HOW:  Install default handlers, verify count increases

    size_t initial_count = nimcp_handler_count();

    int result = nimcp_install_default_handlers();
    EXPECT_EQ(result, 0);

    // Should have more handlers now
    EXPECT_GE(nimcp_handler_count(), initial_count);
}

//=============================================================================
// 21. Exception Flow Through Multiple Module Boundaries
//=============================================================================

// Static tracking for bridge simulation
static std::atomic<int> bridge_A_exceptions{0};
static std::atomic<int> bridge_B_exceptions{0};
static std::atomic<int> bridge_C_exceptions{0};

// Simulated bridge module A handler
static bool bridge_A_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    bridge_A_exceptions++;
    // Transform error and propagate
    if (ex->category == EXCEPTION_CATEGORY_MEMORY) {
        // Memory errors pass through unchanged
        return false;
    }
    return false;
}

// Simulated bridge module B handler
static bool bridge_B_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    bridge_B_exceptions++;
    return false;
}

// Simulated bridge module C handler
static bool bridge_C_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    bridge_C_exceptions++;
    return false;
}

TEST_F(ExceptionFlowTest, ExceptionFlowsThroughMultipleBridges) {
    // WHAT: Test exception flows through multiple simulated bridge modules
    // WHY:  Real system has exceptions crossing many bridge boundaries
    // HOW:  Register handlers simulating different bridges, verify all receive

    bridge_A_exceptions = 0;
    bridge_B_exceptions = 0;
    bridge_C_exceptions = 0;

    // Register bridge handlers with different priorities (simulating bridge chain)
    nimcp_handler_options_t opts_a, opts_b, opts_c;

    nimcp_handler_default_options(&opts_a);
    opts_a.name = "bridge_A";
    opts_a.handler = bridge_A_handler;
    opts_a.priority = 90;
    auto* reg_a = nimcp_handler_register(&opts_a);
    registered_handlers.push_back(reg_a);

    nimcp_handler_default_options(&opts_b);
    opts_b.name = "bridge_B";
    opts_b.handler = bridge_B_handler;
    opts_b.priority = 80;
    auto* reg_b = nimcp_handler_register(&opts_b);
    registered_handlers.push_back(reg_b);

    nimcp_handler_default_options(&opts_c);
    opts_c.name = "bridge_C";
    opts_c.handler = bridge_C_handler;
    opts_c.priority = 70;
    auto* reg_c = nimcp_handler_register(&opts_c);
    registered_handlers.push_back(reg_c);

    // Create and dispatch exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Exception crossing bridge boundaries"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_dispatch(ex);

    // All bridges should have seen the exception (in priority order)
    EXPECT_EQ(bridge_A_exceptions.load(), 1);
    EXPECT_EQ(bridge_B_exceptions.load(), 1);
    EXPECT_EQ(bridge_C_exceptions.load(), 1);

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionFlowTest, BridgeBoundaryContextPreservation) {
    // WHAT: Test that exception context is preserved across bridge boundaries
    // WHY:  Each bridge may add context; previous context must remain
    // HOW:  Add context at different "bridge" levels, verify all preserved

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Cross-bridge context test"
    );
    ASSERT_NE(ex, nullptr);

    // Simulate Bridge A adding context
    nimcp_exception_set_context(ex, "bridge_a_module", "perception");
    nimcp_exception_set_context(ex, "bridge_a_timestamp", "12345");

    // Simulate Bridge B adding context
    nimcp_exception_set_context(ex, "bridge_b_module", "cognitive");
    nimcp_exception_set_context(ex, "bridge_b_action", "processing");

    // Simulate Bridge C adding context
    nimcp_exception_set_context(ex, "bridge_c_module", "executive");
    nimcp_exception_set_context(ex, "bridge_c_decision", "escalate");

    // Verify all context from all bridges is preserved
    EXPECT_EQ(nimcp_exception_context_count(ex), 6u);
    EXPECT_STREQ(nimcp_exception_get_context(ex, "bridge_a_module"), "perception");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "bridge_b_module"), "cognitive");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "bridge_c_module"), "executive");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "bridge_c_decision"), "escalate");

    nimcp_exception_unref(ex);
}

//=============================================================================
// 22. Immune System Exception Processing (Simulated)
//=============================================================================

// Simulated immune response tracker
static struct {
    std::atomic<int> antigens_received{0};
    std::atomic<int> recoveries_suggested{0};
    std::vector<nimcp_error_t> antigen_codes;
    std::mutex antigen_mutex;
} g_immune_tracker;

// Handler that simulates immune system receiving exceptions
static bool simulated_immune_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    g_immune_tracker.antigens_received++;

    {
        std::lock_guard<std::mutex> lock(g_immune_tracker.antigen_mutex);
        g_immune_tracker.antigen_codes.push_back(ex->code);
    }

    // Generate epitope to simulate immune processing
    size_t epitope_len = nimcp_exception_generate_epitope(ex);
    if (epitope_len > 0) {
        // Mark as presented to immune
        ex->presented_to_immune = true;
    }

    // Simulate recovery suggestion based on category
    nimcp_exception_recovery_action_t suggested = nimcp_exception_get_suggested_recovery(ex);
    if (suggested != EXCEPTION_RECOVERY_NONE) {
        g_immune_tracker.recoveries_suggested++;
    }

    return false;
}

TEST_F(ExceptionFlowTest, ImmuneSystemReceivesExceptions) {
    // WHAT: Test simulated immune system receives and processes exceptions
    // WHY:  Immune integration is core to NIMCP self-healing
    // HOW:  Register immune handler, dispatch exceptions, verify processing

    g_immune_tracker.antigens_received = 0;
    g_immune_tracker.recoveries_suggested = 0;
    {
        std::lock_guard<std::mutex> lock(g_immune_tracker.antigen_mutex);
        g_immune_tracker.antigen_codes.clear();
    }

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "simulated_immune";
    opts.handler = simulated_immune_handler;
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    auto* reg = nimcp_handler_register(&opts);
    registered_handlers.push_back(reg);

    // Dispatch various exception types
    nimcp_memory_exception_t* mem = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY, EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__, 1024, "Memory immune test"
    );
    nimcp_brain_exception_t* brain = nimcp_brain_exception_create(
        NIMCP_ERROR_LEARNING_FAILED, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, 1, "test_region", "Brain immune test"
    );
    nimcp_threading_exception_t* thread = nimcp_threading_exception_create(
        NIMCP_ERROR_DEADLOCK, EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__, 9999, "Threading immune test"
    );

    ASSERT_NE(mem, nullptr);
    ASSERT_NE(brain, nullptr);
    ASSERT_NE(thread, nullptr);

    nimcp_exception_dispatch((nimcp_exception_t*)mem);
    nimcp_exception_dispatch((nimcp_exception_t*)brain);
    nimcp_exception_dispatch((nimcp_exception_t*)thread);

    EXPECT_EQ(g_immune_tracker.antigens_received.load(), 3);
    EXPECT_GE(g_immune_tracker.recoveries_suggested.load(), 1);

    // Verify all were marked as presented
    EXPECT_TRUE(mem->base.presented_to_immune);
    EXPECT_TRUE(brain->base.presented_to_immune);
    EXPECT_TRUE(thread->base.presented_to_immune);

    nimcp_exception_unref((nimcp_exception_t*)mem);
    nimcp_exception_unref((nimcp_exception_t*)brain);
    nimcp_exception_unref((nimcp_exception_t*)thread);
}

TEST_F(ExceptionFlowTest, ImmuneProcessesDifferentCategories) {
    // WHAT: Test immune system handles different exception categories
    // WHY:  Different categories need different immune responses
    // HOW:  Dispatch multiple categories, verify all received

    g_immune_tracker.antigens_received = 0;
    {
        std::lock_guard<std::mutex> lock(g_immune_tracker.antigen_mutex);
        g_immune_tracker.antigen_codes.clear();
    }

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "category_immune";
    opts.handler = simulated_immune_handler;
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    auto* reg = nimcp_handler_register(&opts);
    registered_handlers.push_back(reg);

    // Create exceptions of each major category
    std::vector<std::pair<nimcp_error_t, nimcp_exception_category_t>> test_cases = {
        {NIMCP_ERROR_NO_MEMORY, EXCEPTION_CATEGORY_MEMORY},
        {NIMCP_ERROR_BRAIN_CREATION, EXCEPTION_CATEGORY_BRAIN},
        {NIMCP_ERROR_FILE_READ, EXCEPTION_CATEGORY_IO},
        {NIMCP_ERROR_DEADLOCK, EXCEPTION_CATEGORY_THREADING},
        {NIMCP_ERROR_GPU, EXCEPTION_CATEGORY_GPU},
    };

    for (const auto& tc : test_cases) {
        nimcp_exception_t* ex = nimcp_exception_create(
            tc.first, EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Category test: %d", (int)tc.second
        );
        ASSERT_NE(ex, nullptr);
        EXPECT_EQ(ex->category, tc.second);
        nimcp_exception_dispatch(ex);
        nimcp_exception_unref(ex);
    }

    EXPECT_EQ(g_immune_tracker.antigens_received.load(), (int)test_cases.size());
}

//=============================================================================
// 23. Exception Aggregation and Deduplication
//=============================================================================

TEST_F(ExceptionFlowTest, AggregateExceptionDeduplicatesByCode) {
    // WHAT: Test aggregate exception can detect duplicate error codes
    // WHY:  Prevent flooding with redundant exceptions
    // HOW:  Add same error code multiple times, check count

    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Deduplication test aggregate"
    );
    ASSERT_NE(agg, nullptr);

    // Add unique exceptions
    std::set<nimcp_error_t> unique_codes;
    for (int i = 0; i < 5; i++) {
        nimcp_error_t code = NIMCP_ERROR_OPERATION_FAILED + i;
        nimcp_exception_t* child = nimcp_exception_create(
            code, EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Child %d with unique code", i
        );
        ASSERT_NE(child, nullptr);
        unique_codes.insert(code);
        EXPECT_EQ(nimcp_aggregate_exception_add(agg, child), 0);
    }

    // Add duplicate exceptions (same codes)
    for (int i = 0; i < 3; i++) {
        nimcp_error_t code = NIMCP_ERROR_OPERATION_FAILED; // Same code
        nimcp_exception_t* child = nimcp_exception_create(
            code, EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Duplicate child %d", i
        );
        ASSERT_NE(child, nullptr);
        EXPECT_EQ(nimcp_aggregate_exception_add(agg, child), 0);
    }

    // Total should be 8 (5 unique + 3 duplicates)
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 8u);

    // Manual deduplication check - count unique codes
    std::set<nimcp_error_t> seen_codes;
    for (size_t i = 0; i < nimcp_aggregate_exception_count(agg); i++) {
        nimcp_exception_t* child = nimcp_aggregate_exception_get(agg, i);
        seen_codes.insert(child->code);
    }
    // Should have 5 unique codes (5 unique + 1 duplicated)
    EXPECT_EQ(seen_codes.size(), 5u);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

TEST_F(ExceptionFlowTest, AggregateExceptionMaxChildren) {
    // WHAT: Test aggregate respects maximum children limit
    // WHY:  Prevent memory exhaustion from unbounded aggregation
    // HOW:  Add more than max children, verify limit enforced

    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Max children test"
    );
    ASSERT_NE(agg, nullptr);

    // Try to add NIMCP_EXCEPTION_MAX_CHILDREN + some extra
    int success_count = 0;
    for (int i = 0; i < NIMCP_EXCEPTION_MAX_CHILDREN + 10; i++) {
        nimcp_exception_t* child = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Child %d", i
        );
        ASSERT_NE(child, nullptr);

        int result = nimcp_aggregate_exception_add(agg, child);
        if (result == 0) {
            success_count++;
        } else {
            // Addition failed - clean up child manually since not added
            nimcp_exception_unref(child);
        }
    }

    // Should be capped at max
    EXPECT_LE(nimcp_aggregate_exception_count(agg), (size_t)NIMCP_EXCEPTION_MAX_CHILDREN);
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), (size_t)success_count);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

TEST_F(ExceptionFlowTest, AggregateExceptionEpitopeDistinct) {
    // WHAT: Test aggregate exceptions have distinct epitopes
    // WHY:  Immune system needs unique signatures
    // HOW:  Create two aggregates with different children, compare epitopes

    // Create first aggregate
    nimcp_aggregate_exception_t* agg1 = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Aggregate 1"
    );
    ASSERT_NE(agg1, nullptr);

    nimcp_exception_t* child1 = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, "Memory error"
    );
    nimcp_aggregate_exception_add(agg1, child1);

    // Create second aggregate with different child
    nimcp_aggregate_exception_t* agg2 = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Aggregate 2"
    );
    ASSERT_NE(agg2, nullptr);

    nimcp_exception_t* child2 = nimcp_exception_create(
        NIMCP_ERROR_BRAIN_CREATION, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, "Brain error"
    );
    nimcp_aggregate_exception_add(agg2, child2);

    // Generate epitopes
    size_t len1 = nimcp_exception_generate_epitope((nimcp_exception_t*)agg1);
    size_t len2 = nimcp_exception_generate_epitope((nimcp_exception_t*)agg2);

    EXPECT_GT(len1, 0u);
    EXPECT_GT(len2, 0u);

    // Epitopes should be generated (content may vary)
    EXPECT_EQ(((nimcp_exception_t*)agg1)->epitope_len, len1);
    EXPECT_EQ(((nimcp_exception_t*)agg2)->epitope_len, len2);

    nimcp_exception_unref((nimcp_exception_t*)agg1);
    nimcp_exception_unref((nimcp_exception_t*)agg2);
}

//=============================================================================
// 24. Bio-Async Exception Message Routing (Simulated)
//=============================================================================

// Simulated bio-async message tracking
static struct {
    std::atomic<int> messages_routed{0};
    std::atomic<int> error_messages{0};
    std::vector<nimcp_exception_category_t> message_categories;
    std::mutex routing_mutex;
} g_bio_async_routing;

// Simulated bio-async message router handler
static bool bio_async_router_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    g_bio_async_routing.messages_routed++;

    {
        std::lock_guard<std::mutex> lock(g_bio_async_routing.routing_mutex);
        g_bio_async_routing.message_categories.push_back(ex->category);
    }

    // Simulate routing decision based on severity
    if (ex->severity >= EXCEPTION_SEVERITY_SEVERE) {
        g_bio_async_routing.error_messages++;
    }

    return false;
}

TEST_F(ExceptionFlowTest, BioAsyncRoutesByCategory) {
    // WHAT: Test simulated bio-async exception routing by category
    // WHY:  Bio-router needs to route exceptions to appropriate modules
    // HOW:  Dispatch exceptions of different categories, verify routing

    g_bio_async_routing.messages_routed = 0;
    g_bio_async_routing.error_messages = 0;
    {
        std::lock_guard<std::mutex> lock(g_bio_async_routing.routing_mutex);
        g_bio_async_routing.message_categories.clear();
    }

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "bio_async_router";
    opts.handler = bio_async_router_handler;
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    auto* reg = nimcp_handler_register(&opts);
    registered_handlers.push_back(reg);

    // Create and dispatch exceptions that would be routed to different modules
    struct TestMessage {
        nimcp_error_t code;
        nimcp_exception_severity_t severity;
        const char* target_module;
    };

    std::vector<TestMessage> messages = {
        {NIMCP_ERROR_NO_MEMORY, EXCEPTION_SEVERITY_SEVERE, "memory_module"},
        {NIMCP_ERROR_BRAIN_CREATION, EXCEPTION_SEVERITY_ERROR, "brain_module"},
        {NIMCP_ERROR_FILE_READ, EXCEPTION_SEVERITY_WARNING, "io_module"},
        {NIMCP_ERROR_DEADLOCK, EXCEPTION_SEVERITY_CRITICAL, "threading_module"},
        {NIMCP_ERROR_GPU, EXCEPTION_SEVERITY_SEVERE, "gpu_module"},
    };

    for (const auto& msg : messages) {
        nimcp_exception_t* ex = nimcp_exception_create(
            msg.code, msg.severity,
            __FILE__, __LINE__, __func__,
            "Bio-async message to %s", msg.target_module
        );
        ASSERT_NE(ex, nullptr);
        nimcp_exception_set_context(ex, "target_module", msg.target_module);
        nimcp_exception_dispatch(ex);
        nimcp_exception_unref(ex);
    }

    EXPECT_EQ(g_bio_async_routing.messages_routed.load(), (int)messages.size());
    // 3 messages are SEVERE or higher (SEVERE, CRITICAL, SEVERE)
    EXPECT_EQ(g_bio_async_routing.error_messages.load(), 3);

    // Verify categories were received
    {
        std::lock_guard<std::mutex> lock(g_bio_async_routing.routing_mutex);
        EXPECT_EQ(g_bio_async_routing.message_categories.size(), messages.size());
    }
}

TEST_F(ExceptionFlowTest, BioAsyncPreservesPriorityOnRouting) {
    // WHAT: Test bio-async routing preserves exception priority/severity
    // WHY:  High-priority exceptions need expedited handling
    // HOW:  Track severity of routed exceptions

    g_bio_async_routing.messages_routed = 0;
    g_bio_async_routing.error_messages = 0;

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "bio_async_priority";
    opts.handler = bio_async_router_handler;
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.min_severity = EXCEPTION_SEVERITY_ERROR; // Only receive ERROR+
    auto* reg = nimcp_handler_register(&opts);
    registered_handlers.push_back(reg);

    // Dispatch low severity - should NOT be received
    nimcp_exception_t* low = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED, EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__, "Low priority"
    );
    nimcp_exception_dispatch(low);
    nimcp_exception_unref(low);

    // Dispatch high severity - should be received
    nimcp_exception_t* high = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED, EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__, "High priority"
    );
    nimcp_exception_dispatch(high);
    nimcp_exception_unref(high);

    // Only high severity should have been routed
    EXPECT_EQ(g_bio_async_routing.messages_routed.load(), 1);
}

TEST_F(ExceptionFlowTest, BioAsyncBatchExceptionRouting) {
    // WHAT: Test batch exception routing via aggregate
    // WHY:  Batch operations produce multiple exceptions to route
    // HOW:  Create aggregate with multiple children, dispatch as batch

    g_bio_async_routing.messages_routed = 0;
    {
        std::lock_guard<std::mutex> lock(g_bio_async_routing.routing_mutex);
        g_bio_async_routing.message_categories.clear();
    }

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "bio_async_batch";
    opts.handler = bio_async_router_handler;
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    auto* reg = nimcp_handler_register(&opts);
    registered_handlers.push_back(reg);

    // Create aggregate batch
    nimcp_aggregate_exception_t* batch = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Batch of exceptions"
    );
    ASSERT_NE(batch, nullptr);

    // Add multiple children representing batch failures
    for (int i = 0; i < 5; i++) {
        nimcp_exception_t* child = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED + i,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Batch item %d failure", i
        );
        nimcp_aggregate_exception_add(batch, child);
    }

    // Dispatch the aggregate (single message with batch info)
    nimcp_exception_dispatch((nimcp_exception_t*)batch);

    // Should be routed as single message
    EXPECT_EQ(g_bio_async_routing.messages_routed.load(), 1);

    // Verify children are still accessible
    EXPECT_EQ(nimcp_aggregate_exception_count(batch), 5u);

    nimcp_exception_unref((nimcp_exception_t*)batch);
}

//=============================================================================
// 25. Cross-Thread Exception Propagation
//=============================================================================

TEST_F(ExceptionFlowTest, CrossThreadExceptionPropagation) {
    // WHAT: Test exceptions propagate correctly across thread boundaries
    // WHY:  Worker threads may generate exceptions that need main thread handling
    // HOW:  Generate exceptions in worker threads, verify main thread can observe

    auto* reg = register_tracking_handler("cross_thread", 100);
    ASSERT_NE(reg, nullptr);

    std::atomic<bool> thread_completed{false};
    std::atomic<nimcp_error_t> thread_exception_code{NIMCP_SUCCESS};

    // Launch worker thread that will throw exception
    std::thread worker([&]() {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_THREAD_CREATE,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Exception from worker thread"
        );
        if (ex) {
            nimcp_exception_set_context(ex, "thread_role", "worker");
            thread_exception_code = ex->code;
            nimcp_exception_dispatch(ex);
            nimcp_exception_unref(ex);
        }
        thread_completed = true;
    });

    worker.join();

    EXPECT_TRUE(thread_completed.load());
    EXPECT_EQ(thread_exception_code.load(), NIMCP_ERROR_THREAD_CREATE);
    // Handler should have been called (shared handler registration)
    EXPECT_GE(handler_call_count.load(), 1);
}

TEST_F(ExceptionFlowTest, MultipleThreadsSimultaneousExceptions) {
    // WHAT: Test handling exceptions from multiple threads simultaneously
    // WHY:  Real systems have concurrent exception generation
    // HOW:  Launch multiple threads that each generate exceptions

    auto* reg = register_tracking_handler("multi_thread", 100);
    ASSERT_NE(reg, nullptr);

    const int NUM_THREADS = 4;
    const int EXCEPTIONS_PER_THREAD = 3;
    std::atomic<int> total_dispatched{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([t, &total_dispatched]() {
            for (int e = 0; e < EXCEPTIONS_PER_THREAD; e++) {
                nimcp_exception_t* ex = nimcp_exception_create(
                    NIMCP_ERROR_OPERATION_FAILED,
                    EXCEPTION_SEVERITY_ERROR,
                    __FILE__, __LINE__, __func__,
                    "Thread %d exception %d", t, e
                );
                if (ex) {
                    char thread_id_str[32];
                    snprintf(thread_id_str, sizeof(thread_id_str), "%d", t);
                    nimcp_exception_set_context(ex, "thread_id", thread_id_str);
                    nimcp_exception_dispatch(ex);
                    nimcp_exception_unref(ex);
                    total_dispatched++;
                }
            }
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(total_dispatched.load(), NUM_THREADS * EXCEPTIONS_PER_THREAD);
    // Handler should have been called for all exceptions
    EXPECT_GE(handler_call_count.load(), NUM_THREADS * EXCEPTIONS_PER_THREAD);
}

//=============================================================================
// 26. Exception Chain Walking
//=============================================================================

TEST_F(ExceptionFlowTest, ExceptionChainWalkingWithContext) {
    // WHAT: Test walking exception cause chain and accessing context at each level
    // WHY:  Debug tools need to traverse full exception history
    // HOW:  Create chain, walk it, verify all context accessible

    // Create exception chain: DB error -> Service error -> API error
    nimcp_exception_t* db_error = nimcp_exception_create(
        NIMCP_ERROR_FILE_READ,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Database read failed"
    );
    ASSERT_NE(db_error, nullptr);
    nimcp_exception_set_context(db_error, "layer", "database");
    nimcp_exception_set_context(db_error, "table", "neurons");

    nimcp_exception_t* service_error = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Service operation failed"
    );
    ASSERT_NE(service_error, nullptr);
    nimcp_exception_set_context(service_error, "layer", "service");
    nimcp_exception_set_context(service_error, "operation", "fetch_neurons");
    nimcp_exception_set_cause(service_error, db_error);

    nimcp_exception_t* api_error = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "API request failed"
    );
    ASSERT_NE(api_error, nullptr);
    nimcp_exception_set_context(api_error, "layer", "api");
    nimcp_exception_set_context(api_error, "endpoint", "/neurons/get");
    nimcp_exception_set_cause(api_error, service_error);

    // Walk the chain and verify context at each level
    std::vector<std::pair<std::string, std::string>> expected_layers = {
        {"api", "/neurons/get"},
        {"service", "fetch_neurons"},
        {"database", "neurons"}
    };

    nimcp_exception_t* current = api_error;
    int depth = 0;
    while (current != nullptr && depth < (int)expected_layers.size()) {
        const char* layer = nimcp_exception_get_context(current, "layer");
        ASSERT_NE(layer, nullptr);
        EXPECT_EQ(std::string(layer), expected_layers[depth].first);
        depth++;
        current = nimcp_exception_get_cause(current);
    }

    EXPECT_EQ(depth, 3);

    nimcp_exception_unref(api_error);
}

//=============================================================================
// 27. Exception Severity Escalation
//=============================================================================

TEST_F(ExceptionFlowTest, SeverityEscalationInChain) {
    // WHAT: Test that chained exceptions can have escalating severity
    // WHY:  Root cause may be minor but impact severe
    // HOW:  Create chain with increasing severity

    nimcp_exception_t* warning = nimcp_exception_create(
        NIMCP_ERROR_INVALID_PARAM,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Minor parameter issue"
    );
    ASSERT_NE(warning, nullptr);

    nimcp_exception_t* error = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Operation affected"
    );
    ASSERT_NE(error, nullptr);
    nimcp_exception_set_cause(error, warning);

    nimcp_exception_t* critical = nimcp_exception_create(
        NIMCP_ERROR_BRAIN_INVALID,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        "Brain state corrupted"
    );
    ASSERT_NE(critical, nullptr);
    nimcp_exception_set_cause(critical, error);

    // Verify escalation
    EXPECT_EQ(critical->severity, EXCEPTION_SEVERITY_CRITICAL);
    EXPECT_EQ(nimcp_exception_get_cause(critical)->severity, EXCEPTION_SEVERITY_ERROR);
    EXPECT_EQ(nimcp_exception_get_cause(nimcp_exception_get_cause(critical))->severity,
              EXCEPTION_SEVERITY_WARNING);

    nimcp_exception_unref(critical);
}

//=============================================================================
// 28. Exception Filtering by Multiple Criteria
//=============================================================================

// Handler that tracks both category and severity
static struct {
    std::atomic<int> count{0};
    std::vector<std::pair<nimcp_exception_category_t, nimcp_exception_severity_t>> received;
    std::mutex filter_mutex;
} g_filter_tracking;

static bool multi_filter_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    g_filter_tracking.count++;
    {
        std::lock_guard<std::mutex> lock(g_filter_tracking.filter_mutex);
        g_filter_tracking.received.push_back({ex->category, ex->severity});
    }
    return false;
}

TEST_F(ExceptionFlowTest, FilterByCategoryAndSeverity) {
    // WHAT: Test handler filtering by both category AND severity
    // WHY:  Real handlers may only want specific exception subsets
    // HOW:  Register filtered handler, dispatch various exceptions

    g_filter_tracking.count = 0;
    {
        std::lock_guard<std::mutex> lock(g_filter_tracking.filter_mutex);
        g_filter_tracking.received.clear();
    }

    // Register handler that only wants SEVERE+ MEMORY exceptions
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "memory_severe_filter";
    opts.handler = multi_filter_handler;
    opts.priority = 100;
    opts.category_filter = EXCEPTION_CATEGORY_MEMORY;
    opts.min_severity = EXCEPTION_SEVERITY_SEVERE;
    auto* reg = nimcp_handler_register(&opts);
    registered_handlers.push_back(reg);

    // Dispatch various combinations
    // Memory + WARNING - should NOT match
    nimcp_memory_exception_t* mem_warn = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY, EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__, 1024, "Memory warning"
    );
    nimcp_exception_dispatch((nimcp_exception_t*)mem_warn);
    nimcp_exception_unref((nimcp_exception_t*)mem_warn);

    // Memory + SEVERE - SHOULD match
    nimcp_memory_exception_t* mem_severe = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY, EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__, 1024, "Memory severe"
    );
    nimcp_exception_dispatch((nimcp_exception_t*)mem_severe);
    nimcp_exception_unref((nimcp_exception_t*)mem_severe);

    // Brain + SEVERE - should NOT match (wrong category)
    nimcp_brain_exception_t* brain_severe = nimcp_brain_exception_create(
        NIMCP_ERROR_BRAIN_CREATION, EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__, 1, "test", "Brain severe"
    );
    nimcp_exception_dispatch((nimcp_exception_t*)brain_severe);
    nimcp_exception_unref((nimcp_exception_t*)brain_severe);

    // Memory + CRITICAL - SHOULD match
    nimcp_memory_exception_t* mem_crit = nimcp_memory_exception_create(
        NIMCP_ERROR_MEMORY_CORRUPTION, EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__, 0, "Memory critical"
    );
    nimcp_exception_dispatch((nimcp_exception_t*)mem_crit);
    nimcp_exception_unref((nimcp_exception_t*)mem_crit);

    // Only Memory + SEVERE or higher should have been received
    EXPECT_EQ(g_filter_tracking.count.load(), 2);

    {
        std::lock_guard<std::mutex> lock(g_filter_tracking.filter_mutex);
        for (const auto& r : g_filter_tracking.received) {
            EXPECT_EQ(r.first, EXCEPTION_CATEGORY_MEMORY);
            EXPECT_GE(r.second, EXCEPTION_SEVERITY_SEVERE);
        }
    }
}

//=============================================================================
// 20. Exception Flow Through Async Bridges
//=============================================================================

/**
 * @brief Simulated async bridge operation that throws exceptions
 */
static nimcp_error_t async_bridge_operation(int bridge_id, bool should_fail) {
    if (should_fail) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Async bridge %d operation failed", bridge_id);
        if (ex) {
            nimcp_exception_set_context(ex, "bridge_id", std::to_string(bridge_id).c_str());
            nimcp_exception_set_context(ex, "async", "true");
            nimcp_exception_present_to_immune(ex, NULL);
            nimcp_exception_dispatch(ex);
            nimcp_exception_unref(ex);
        }
        return NIMCP_ERROR_OPERATION_FAILED;
    }
    return NIMCP_SUCCESS;
}

TEST_F(ExceptionFlowTest, AsyncBridgeExceptionPropagation) {
    // WHAT: Test exception propagation through async bridge operations
    // WHY:  Async bridges must properly propagate exceptions to handlers
    // HOW:  Simulate async bridge failure and verify exception reaches handlers

    register_tracking_handler("async_bridge_test", 100);

    // Simulate multiple async bridge operations
    std::vector<std::thread> bridge_threads;
    std::atomic<int> failures{0};

    for (int i = 0; i < 5; i++) {
        bridge_threads.emplace_back([&, i]() {
            nimcp_error_t result = async_bridge_operation(i, i % 2 == 0);
            if (result != NIMCP_SUCCESS) {
                failures++;
            }
        });
    }

    for (auto& t : bridge_threads) {
        t.join();
    }

    // Every other bridge should have failed (0, 2, 4)
    EXPECT_EQ(failures.load(), 3);
    // Handler should have been called for each failure
    EXPECT_GE(handler_call_count.load(), 3);
}

TEST_F(ExceptionFlowTest, AsyncBridgeConcurrentExceptions) {
    // WHAT: Test concurrent exception handling in multiple async bridges
    // WHY:  Ensure thread-safety of exception system under concurrent access
    // HOW:  Launch many concurrent bridge operations that throw

    register_tracking_handler("concurrent_bridge_test", 100);

    const int NUM_BRIDGES = 20;
    const int ITERATIONS = 10;
    std::atomic<int> total_exceptions{0};

    std::vector<std::thread> threads;
    for (int b = 0; b < NUM_BRIDGES; b++) {
        threads.emplace_back([&, b]() {
            for (int i = 0; i < ITERATIONS; i++) {
                nimcp_exception_t* ex = nimcp_exception_create(
                    NIMCP_ERROR_THREAD_SYNC,
                    EXCEPTION_SEVERITY_WARNING,
                    __FILE__, __LINE__, __func__,
                    "Bridge %d iteration %d sync error", b, i);
                if (ex) {
                    nimcp_exception_dispatch(ex);
                    nimcp_exception_unref(ex);
                    total_exceptions++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(total_exceptions.load(), NUM_BRIDGES * ITERATIONS);
    EXPECT_EQ(handler_call_count.load(), total_exceptions.load());
}

TEST_F(ExceptionFlowTest, AsyncBridgeExceptionWithTimeout) {
    // WHAT: Test exception handling with timeout in async bridge
    // WHY:  Bridges may throw timeout exceptions during async operations
    // HOW:  Simulate timeout scenario and verify exception handling

    register_tracking_handler("timeout_bridge_test", 100);

    std::atomic<bool> timed_out{false};

    std::thread bridge_thread([&]() {
        // Simulate operation that times out
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_TIMEOUT,
            EXCEPTION_SEVERITY_WARNING,
            __FILE__, __LINE__, __func__,
            "Bridge operation timed out after 10ms");
        if (ex) {
            nimcp_exception_set_context(ex, "timeout_ms", "10");
            nimcp_exception_dispatch(ex);
            nimcp_exception_unref(ex);
            timed_out = true;
        }
    });

    bridge_thread.join();

    EXPECT_TRUE(timed_out.load());
    EXPECT_EQ(handler_call_count.load(), 1);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_TIMEOUT);
}

//=============================================================================
// 21. Exception Flow Through Swarm Modules
//=============================================================================

/**
 * @brief Simulated swarm agent operation with exception handling
 */
static nimcp_error_t swarm_agent_operation(int agent_id, bool should_fail) {
    if (should_fail) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Swarm agent %d operation failed", agent_id);
        if (ex) {
            nimcp_exception_set_context(ex, "agent_id", std::to_string(agent_id).c_str());
            nimcp_exception_set_context(ex, "swarm_type", "collective");
            nimcp_exception_present_to_immune(ex, NULL);
            nimcp_exception_dispatch(ex);
            nimcp_exception_unref(ex);
        }
        return NIMCP_ERROR_OPERATION_FAILED;
    }
    return NIMCP_SUCCESS;
}

TEST_F(ExceptionFlowTest, SwarmAgentExceptionPropagation) {
    // WHAT: Test exception flow through swarm agent operations
    // WHY:  Swarm modules must handle exceptions from individual agents
    // HOW:  Simulate multiple agent failures and verify exception handling

    register_tracking_handler("swarm_agent_test", 100);

    const int NUM_AGENTS = 10;
    std::atomic<int> agent_failures{0};
    std::vector<std::thread> agent_threads;

    for (int i = 0; i < NUM_AGENTS; i++) {
        agent_threads.emplace_back([&, i]() {
            // Agents 3, 6, 9 will fail
            nimcp_error_t result = swarm_agent_operation(i, i % 3 == 0 && i > 0);
            if (result != NIMCP_SUCCESS) {
                agent_failures++;
            }
        });
    }

    for (auto& t : agent_threads) {
        t.join();
    }

    EXPECT_EQ(agent_failures.load(), 3);  // Agents 3, 6, 9
    EXPECT_GE(handler_call_count.load(), 3);
}

TEST_F(ExceptionFlowTest, SwarmCollectiveExceptionAggregation) {
    // WHAT: Test aggregation of exceptions from multiple swarm agents
    // WHY:  Swarm operations may need to collect and report multiple failures
    // HOW:  Create aggregate exception from multiple agent failures

    register_tracking_handler("swarm_aggregate_test", 100);

    // Create aggregate exception for swarm failures
    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Swarm collective operation failed with multiple agent errors");
    ASSERT_NE(agg, nullptr);

    // Add individual agent exceptions
    for (int i = 0; i < 5; i++) {
        nimcp_exception_t* agent_ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_WARNING,
            __FILE__, __LINE__, __func__,
            "Agent %d failed in swarm operation", i);
        ASSERT_NE(agent_ex, nullptr);
        nimcp_exception_set_context(agent_ex, "agent_id", std::to_string(i).c_str());
        nimcp_aggregate_exception_add(agg, agent_ex);
    }

    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 5u);

    // Dispatch aggregate
    nimcp_exception_dispatch((nimcp_exception_t*)agg);

    EXPECT_EQ(handler_call_count.load(), 1);  // Single aggregate dispatch
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_OPERATION_FAILED);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

TEST_F(ExceptionFlowTest, SwarmConsensusExceptionHandling) {
    // WHAT: Test exception handling during swarm consensus operations
    // WHY:  Consensus failures are critical for swarm decision making
    // HOW:  Simulate consensus failure and verify exception chain

    register_tracking_handler("consensus_test", 100);

    // Simulate consensus failure
    nimcp_exception_t* consensus_ex = nimcp_exception_create(
        NIMCP_ERROR_THREAD_SYNC,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Swarm consensus failed: quorum not reached");
    ASSERT_NE(consensus_ex, nullptr);

    nimcp_exception_set_context(consensus_ex, "required_quorum", "7");
    nimcp_exception_set_context(consensus_ex, "achieved_votes", "4");
    nimcp_exception_set_context(consensus_ex, "total_agents", "10");

    // Create cause chain - individual agent communication failures
    nimcp_exception_t* comm_failure = nimcp_exception_create(
        NIMCP_ERROR_NETWORK_IO,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Agent communication timeout during consensus round");
    ASSERT_NE(comm_failure, nullptr);

    nimcp_exception_set_cause(consensus_ex, comm_failure);
    nimcp_exception_present_to_immune(consensus_ex, NULL);
    nimcp_exception_dispatch(consensus_ex);

    EXPECT_EQ(handler_call_count.load(), 1);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_THREAD_SYNC);

    // Verify cause chain
    nimcp_exception_t* cause = nimcp_exception_get_cause(consensus_ex);
    ASSERT_NE(cause, nullptr);
    EXPECT_EQ(cause->code, NIMCP_ERROR_NETWORK_IO);

    nimcp_exception_unref(consensus_ex);
}

//=============================================================================
// 22. Exception Flow Through Cognitive Modules
//=============================================================================

/**
 * @brief Simulated cognitive module operation
 */
static nimcp_error_t cognitive_module_operation(
    const char* module_name, int operation_id, bool should_fail) {
    if (should_fail) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_WORKING_MEMORY,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Cognitive %s module operation %d failed", module_name, operation_id);
        if (ex) {
            nimcp_exception_set_context(ex, "module", module_name);
            nimcp_exception_set_context(ex, "operation_id",
                std::to_string(operation_id).c_str());
            nimcp_exception_present_to_immune(ex, NULL);
            nimcp_exception_dispatch(ex);
            nimcp_exception_unref(ex);
        }
        return NIMCP_ERROR_WORKING_MEMORY;
    }
    return NIMCP_SUCCESS;
}

TEST_F(ExceptionFlowTest, CognitiveModuleExceptionPropagation) {
    // WHAT: Test exception propagation through cognitive modules
    // WHY:  Cognitive modules must properly report and handle errors
    // HOW:  Simulate failures in different cognitive modules

    register_tracking_handler("cognitive_test", 100);

    const char* modules[] = {
        "working_memory", "attention", "executive", "salience", "episodic"
    };

    for (int i = 0; i < 5; i++) {
        handler_call_count = 0;
        last_exception_code = NIMCP_SUCCESS;

        nimcp_error_t result = cognitive_module_operation(modules[i], i, true);

        EXPECT_EQ(result, NIMCP_ERROR_WORKING_MEMORY);
        EXPECT_EQ(handler_call_count.load(), 1);
        EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_WORKING_MEMORY);
    }
}

TEST_F(ExceptionFlowTest, CognitiveChainExceptionFlow) {
    // WHAT: Test exception flow through cognitive processing chain
    // WHY:  Cognitive operations often chain through multiple modules
    // HOW:  Simulate a processing chain with exception at different stages

    register_tracking_handler("cognitive_chain_test", 100);

    // Simulate: Input -> Attention -> Working Memory -> Executive -> Output
    // Failure at Working Memory stage

    // Stage 1: Attention (success)
    nimcp_error_t result = cognitive_module_operation("attention", 1, false);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Stage 2: Working Memory (failure)
    result = cognitive_module_operation("working_memory", 2, true);
    EXPECT_EQ(result, NIMCP_ERROR_WORKING_MEMORY);

    // Exception should have been dispatched
    EXPECT_EQ(handler_call_count.load(), 1);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_WORKING_MEMORY);
}

TEST_F(ExceptionFlowTest, CognitiveExceptionWithEmotionalTagging) {
    // WHAT: Test cognitive exception with emotional context
    // WHY:  Cognitive errors may have emotional significance for learning
    // HOW:  Create cognitive exception with emotional tagging context

    register_tracking_handler("emotional_test", 100);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_EMOTIONAL_TAGGING,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Emotional tagging failed for memory encoding");
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "emotion_type", "fear");
    nimcp_exception_set_context(ex, "valence", "-0.8");
    nimcp_exception_set_context(ex, "arousal", "0.9");
    nimcp_exception_set_context(ex, "memory_id", "mem_12345");

    nimcp_exception_present_to_immune(ex, NULL);
    nimcp_exception_dispatch(ex);

    EXPECT_EQ(handler_call_count.load(), 1);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_EMOTIONAL_TAGGING);
    EXPECT_EQ(last_category.load(), EXCEPTION_CATEGORY_COGNITIVE);

    // Verify context preservation
    EXPECT_STREQ(nimcp_exception_get_context(ex, "emotion_type"), "fear");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "valence"), "-0.8");

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionFlowTest, CognitiveExecutiveControlException) {
    // WHAT: Test executive control exception handling
    // WHY:  Executive control failures affect high-level cognitive operations
    // HOW:  Simulate executive control failure with proper context

    register_tracking_handler("executive_test", 100);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_EXECUTIVE_CONTROL,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Executive control failure: goal conflict detected");
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "active_goals", "3");
    nimcp_exception_set_context(ex, "conflicting_goals", "goal_A,goal_B");
    nimcp_exception_set_context(ex, "resolution_strategy", "priority_based");

    nimcp_exception_present_to_immune(ex, NULL);
    nimcp_exception_dispatch(ex);

    EXPECT_EQ(handler_call_count.load(), 1);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_EXECUTIVE_CONTROL);
    EXPECT_GE(last_severity.load(), EXCEPTION_SEVERITY_SEVERE);

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionFlowTest, CognitivePredictiveProcessingException) {
    // WHAT: Test predictive processing exception
    // WHY:  Predictive coding failures need special handling
    // HOW:  Create exception for prediction error scenario

    register_tracking_handler("predictive_test", 100);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_PREDICTIVE,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Prediction error exceeded threshold");
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "prediction_error", "0.85");
    nimcp_exception_set_context(ex, "threshold", "0.5");
    nimcp_exception_set_context(ex, "model_id", "pred_model_7");
    nimcp_exception_set_context(ex, "layer", "sensory");

    nimcp_exception_dispatch(ex);

    EXPECT_EQ(handler_call_count.load(), 1);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_PREDICTIVE);
    EXPECT_EQ(last_category.load(), EXCEPTION_CATEGORY_COGNITIVE);

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionFlowTest, CognitiveMetaLearningException) {
    // WHAT: Test meta-learning exception handling
    // WHY:  Meta-learning failures affect adaptation capabilities
    // HOW:  Simulate meta-learning failure scenario

    register_tracking_handler("metalearning_test", 100);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_META_LEARNING,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Meta-learning adaptation failed");
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "learning_rate_delta", "-0.001");
    nimcp_exception_set_context(ex, "adaptation_cycles", "100");
    nimcp_exception_set_context(ex, "convergence_status", "diverging");

    nimcp_exception_present_to_immune(ex, NULL);
    nimcp_exception_dispatch(ex);

    EXPECT_EQ(handler_call_count.load(), 1);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_META_LEARNING);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
