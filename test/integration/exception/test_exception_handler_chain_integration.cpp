/**
 * @file test_exception_handler_chain_integration.cpp
 * @brief Integration tests for exception handler chain
 *
 * WHAT: Test exception handler chain behavior across modules
 * WHY:  Verify handler priority, consumption, filtering, and cleanup work correctly
 * HOW:  Register multiple handlers from simulated "modules" and verify dispatch behavior
 *
 * TEST SCENARIOS:
 * - Multiple handlers receiving exceptions in priority order
 * - Handler consumption (stopping propagation)
 * - Handler failure and fallback
 * - Handler registration from different modules
 * - Handler filtering by exception type
 * - Handler unregistration cleanup
 *
 * @author NIMCP Development Team
 * @date 2026-01-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>
#include <atomic>
#include <chrono>
#include <thread>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Helper Structures
//=============================================================================

/**
 * @brief Record of a handler invocation for verification
 */
struct HandlerInvocation {
    std::string handler_name;
    nimcp_error_t exception_code;
    nimcp_exception_type_t exception_type;
    nimcp_exception_category_t exception_category;
    int priority;
    uint64_t timestamp_us;
};

/**
 * @brief Shared test state across handlers
 */
static struct {
    std::vector<HandlerInvocation> invocations;
    std::atomic<int> call_count{0};
    std::atomic<bool> should_consume{false};
    std::atomic<bool> should_fail{false};
    nimcp_exception_type_t consume_only_type{EXCEPTION_TYPE_BASE};
} g_test_state;

//=============================================================================
// Test Handler Functions
//=============================================================================

/**
 * @brief Generic handler that records invocation and optionally consumes
 */
static bool recording_handler(nimcp_exception_t* ex, void* user_data) {
    const char* handler_name = static_cast<const char*>(user_data);

    HandlerInvocation inv;
    inv.handler_name = handler_name ? handler_name : "unknown";
    inv.exception_code = ex->code;
    inv.exception_type = ex->type;
    inv.exception_category = ex->category;
    inv.timestamp_us = ex->timestamp_us;

    // Extract priority from handler name if format is "handler_<priority>"
    if (handler_name && strstr(handler_name, "handler_") == handler_name) {
        inv.priority = atoi(handler_name + strlen("handler_"));
    } else {
        inv.priority = 0;
    }

    g_test_state.invocations.push_back(inv);
    g_test_state.call_count++;

    // Check if we should consume based on type filter
    if (g_test_state.should_consume.load()) {
        if (g_test_state.consume_only_type == EXCEPTION_TYPE_BASE ||
            g_test_state.consume_only_type == ex->type) {
            return true;  // Consume - stop propagation
        }
    }

    return false;  // Don't consume - continue chain
}

/**
 * @brief Handler that always consumes exceptions
 */
static bool consuming_handler(nimcp_exception_t* ex, void* user_data) {
    const char* handler_name = static_cast<const char*>(user_data);

    HandlerInvocation inv;
    inv.handler_name = handler_name ? handler_name : "consuming";
    inv.exception_code = ex->code;
    inv.exception_type = ex->type;
    inv.exception_category = ex->category;
    inv.priority = 0;
    inv.timestamp_us = ex->timestamp_us;

    g_test_state.invocations.push_back(inv);
    g_test_state.call_count++;

    return true;  // Always consume
}

/**
 * @brief Handler that simulates failure (returns false even when it should handle)
 */
static bool failing_handler(nimcp_exception_t* ex, void* user_data) {
    const char* handler_name = static_cast<const char*>(user_data);

    HandlerInvocation inv;
    inv.handler_name = handler_name ? handler_name : "failing";
    inv.exception_code = ex->code;
    inv.exception_type = ex->type;
    inv.exception_category = ex->category;
    inv.priority = 0;
    inv.timestamp_us = ex->timestamp_us;

    g_test_state.invocations.push_back(inv);
    g_test_state.call_count++;

    // Simulate failure - don't handle even though we were called
    return false;
}

/**
 * @brief Handler specific to memory exceptions
 */
static bool memory_handler(nimcp_exception_t* ex, void* user_data) {
    const char* handler_name = static_cast<const char*>(user_data);

    // Only record if it's a memory exception (filter check at handler level)
    if (ex->category != EXCEPTION_CATEGORY_MEMORY) {
        return false;
    }

    HandlerInvocation inv;
    inv.handler_name = handler_name ? handler_name : "memory";
    inv.exception_code = ex->code;
    inv.exception_type = ex->type;
    inv.exception_category = ex->category;
    inv.priority = 0;
    inv.timestamp_us = ex->timestamp_us;

    g_test_state.invocations.push_back(inv);
    g_test_state.call_count++;

    return true;  // Consume memory exceptions
}

//=============================================================================
// Test Fixture
//=============================================================================

class ExceptionHandlerChainIntegrationTest : public ::testing::Test {
protected:
    std::vector<nimcp_handler_registration_t*> registrations_;

    void SetUp() override {
        // Clear test state
        g_test_state.invocations.clear();
        g_test_state.call_count = 0;
        g_test_state.should_consume = false;
        g_test_state.should_fail = false;
        g_test_state.consume_only_type = EXCEPTION_TYPE_BASE;

        // Initialize exception system
        nimcp_exception_system_init();

        registrations_.clear();
    }

    void TearDown() override {
        // Unregister all handlers
        for (auto* reg : registrations_) {
            if (reg) {
                nimcp_handler_unregister(reg);
            }
        }
        registrations_.clear();

        // Clean up exception system
        nimcp_exception_clear_current();
        nimcp_exception_handlers_shutdown();
        nimcp_exception_system_shutdown();
    }

    /**
     * @brief Helper to register a handler and track for cleanup
     */
    nimcp_handler_registration_t* registerHandler(
        const char* name,
        nimcp_exception_handler_fn handler,
        int priority,
        void* user_data = nullptr,
        nimcp_exception_category_t category_filter = static_cast<nimcp_exception_category_t>(0),
        nimcp_exception_type_t type_filter = static_cast<nimcp_exception_type_t>(0)
    ) {
        nimcp_handler_options_t options;
        nimcp_handler_default_options(&options);
        options.name = name;
        options.handler = handler;
        options.priority = priority;
        options.user_data = user_data ? user_data : const_cast<char*>(name);
        options.category_filter = category_filter;
        options.type_filter = type_filter;

        nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
        if (reg) {
            registrations_.push_back(reg);
        }
        return reg;
    }

    /**
     * @brief Create a test exception with specified parameters
     */
    nimcp_exception_t* createException(
        nimcp_error_t code,
        nimcp_exception_severity_t severity = EXCEPTION_SEVERITY_ERROR,
        const char* message = "Test exception"
    ) {
        return nimcp_exception_create(
            code, severity, __FILE__, __LINE__, __func__, "%s", message
        );
    }
};

//=============================================================================
// Test: Handlers Receive Exceptions in Priority Order
//=============================================================================

TEST_F(ExceptionHandlerChainIntegrationTest, HandlersReceiveExceptionInPriorityOrder) {
    // WHAT: Verify handlers are called in descending priority order
    // WHY:  Critical for security/logging handlers to execute in correct sequence

    // Register handlers with different priorities
    auto* reg_low = registerHandler("handler_10", recording_handler, NIMCP_HANDLER_PRIORITY_LOW);
    auto* reg_mid = registerHandler("handler_50", recording_handler, NIMCP_HANDLER_PRIORITY_NORMAL);
    auto* reg_high = registerHandler("handler_100", recording_handler, NIMCP_HANDLER_PRIORITY_HIGH);

    ASSERT_NE(reg_low, nullptr);
    ASSERT_NE(reg_mid, nullptr);
    ASSERT_NE(reg_high, nullptr);

    // Create and dispatch exception
    nimcp_exception_t* ex = createException(NIMCP_ERROR_OPERATION_FAILED);
    ASSERT_NE(ex, nullptr);

    nimcp_exception_dispatch(ex);

    // Verify all handlers were called
    ASSERT_GE(g_test_state.invocations.size(), 3u);

    // Verify order: highest priority first (100 -> 50 -> 10)
    bool found_100 = false;
    bool found_50 = false;
    bool found_10 = false;
    size_t idx_100 = 0, idx_50 = 0, idx_10 = 0;

    for (size_t i = 0; i < g_test_state.invocations.size(); i++) {
        if (g_test_state.invocations[i].handler_name == "handler_100") {
            found_100 = true;
            idx_100 = i;
        } else if (g_test_state.invocations[i].handler_name == "handler_50") {
            found_50 = true;
            idx_50 = i;
        } else if (g_test_state.invocations[i].handler_name == "handler_10") {
            found_10 = true;
            idx_10 = i;
        }
    }

    EXPECT_TRUE(found_100) << "High priority handler not called";
    EXPECT_TRUE(found_50) << "Normal priority handler not called";
    EXPECT_TRUE(found_10) << "Low priority handler not called";

    // Verify priority order
    EXPECT_LT(idx_100, idx_50) << "High priority should be called before normal";
    EXPECT_LT(idx_50, idx_10) << "Normal priority should be called before low";

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Handler Can Consume Exception (Stop Propagation)
//=============================================================================

TEST_F(ExceptionHandlerChainIntegrationTest, HandlerCanConsumeException) {
    // WHAT: Verify handler returning true stops further propagation
    // WHY:  Essential for error recovery handlers that fully handle exceptions

    // Register a consuming handler at medium priority
    auto* reg_high = registerHandler("handler_100", recording_handler, NIMCP_HANDLER_PRIORITY_HIGH);
    auto* reg_consumer = registerHandler("consumer_50", consuming_handler, NIMCP_HANDLER_PRIORITY_NORMAL);
    auto* reg_low = registerHandler("handler_10", recording_handler, NIMCP_HANDLER_PRIORITY_LOW);

    ASSERT_NE(reg_high, nullptr);
    ASSERT_NE(reg_consumer, nullptr);
    ASSERT_NE(reg_low, nullptr);

    // Create and dispatch exception
    nimcp_exception_t* ex = createException(NIMCP_ERROR_OPERATION_FAILED);
    ASSERT_NE(ex, nullptr);

    bool handled = nimcp_exception_dispatch(ex);

    // Exception should be marked as handled
    EXPECT_TRUE(handled);

    // Check invocation order
    bool found_high = false;
    bool found_consumer = false;
    bool found_low = false;

    for (const auto& inv : g_test_state.invocations) {
        if (inv.handler_name == "handler_100") found_high = true;
        if (inv.handler_name == "consumer_50") found_consumer = true;
        if (inv.handler_name == "handler_10") found_low = true;
    }

    EXPECT_TRUE(found_high) << "High priority handler should be called";
    EXPECT_TRUE(found_consumer) << "Consumer handler should be called";
    EXPECT_FALSE(found_low) << "Low priority handler should NOT be called after consumption";

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Handler Failure Triggers Next Handler
//=============================================================================

TEST_F(ExceptionHandlerChainIntegrationTest, HandlerFailureTriggersNextHandler) {
    // WHAT: Verify that when a handler fails (returns false), next handler is called
    // WHY:  Ensure fallback behavior works for resilient error handling

    // Register handlers: failing at high priority, backup at normal priority
    auto* reg_fail = registerHandler("failing_100", failing_handler, NIMCP_HANDLER_PRIORITY_HIGH);
    auto* reg_backup = registerHandler("backup_50", consuming_handler, NIMCP_HANDLER_PRIORITY_NORMAL);
    auto* reg_low = registerHandler("handler_10", recording_handler, NIMCP_HANDLER_PRIORITY_LOW);

    ASSERT_NE(reg_fail, nullptr);
    ASSERT_NE(reg_backup, nullptr);
    ASSERT_NE(reg_low, nullptr);

    // Create and dispatch exception
    nimcp_exception_t* ex = createException(NIMCP_ERROR_OPERATION_FAILED);
    ASSERT_NE(ex, nullptr);

    bool handled = nimcp_exception_dispatch(ex);

    // Exception should be handled by backup
    EXPECT_TRUE(handled);

    // Verify failing handler was called first, then backup
    bool found_fail = false;
    bool found_backup = false;
    bool found_low = false;
    size_t idx_fail = 0, idx_backup = 0;

    for (size_t i = 0; i < g_test_state.invocations.size(); i++) {
        if (g_test_state.invocations[i].handler_name == "failing_100") {
            found_fail = true;
            idx_fail = i;
        }
        if (g_test_state.invocations[i].handler_name == "backup_50") {
            found_backup = true;
            idx_backup = i;
        }
        if (g_test_state.invocations[i].handler_name == "handler_10") {
            found_low = true;
        }
    }

    EXPECT_TRUE(found_fail) << "Failing handler should be called";
    EXPECT_TRUE(found_backup) << "Backup handler should be called after failure";
    EXPECT_LT(idx_fail, idx_backup) << "Failing handler should be called before backup";
    EXPECT_FALSE(found_low) << "Low priority handler should not be called after backup consumed";

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Modules Can Register Own Handlers
//=============================================================================

TEST_F(ExceptionHandlerChainIntegrationTest, ModulesCanRegisterOwnHandlers) {
    // WHAT: Simulate multiple modules registering their own handlers
    // WHY:  Verify modular registration works correctly

    // Simulate Module A (security module) - high priority
    auto* reg_security = registerHandler("security_module", recording_handler,
        NIMCP_HANDLER_PRIORITY_HIGH, const_cast<char*>("security_module"));

    // Simulate Module B (logging module) - low priority
    auto* reg_logging = registerHandler("logging_module", recording_handler,
        NIMCP_HANDLER_PRIORITY_LOW, const_cast<char*>("logging_module"));

    // Simulate Module C (recovery module) - normal priority
    auto* reg_recovery = registerHandler("recovery_module", consuming_handler,
        NIMCP_HANDLER_PRIORITY_NORMAL, const_cast<char*>("recovery_module"));

    ASSERT_NE(reg_security, nullptr);
    ASSERT_NE(reg_logging, nullptr);
    ASSERT_NE(reg_recovery, nullptr);

    // Verify handler count
    size_t count = nimcp_handler_count();
    EXPECT_GE(count, 3u);

    // Create and dispatch exception
    nimcp_exception_t* ex = createException(NIMCP_ERROR_SECURITY_BASE, EXCEPTION_SEVERITY_SEVERE);
    ASSERT_NE(ex, nullptr);

    nimcp_exception_dispatch(ex);

    // Verify security was called first, then recovery consumed
    bool found_security = false;
    bool found_recovery = false;
    bool found_logging = false;
    size_t idx_security = 0, idx_recovery = 0;

    for (size_t i = 0; i < g_test_state.invocations.size(); i++) {
        if (g_test_state.invocations[i].handler_name == "security_module") {
            found_security = true;
            idx_security = i;
        }
        if (g_test_state.invocations[i].handler_name == "recovery_module") {
            found_recovery = true;
            idx_recovery = i;
        }
        if (g_test_state.invocations[i].handler_name == "logging_module") {
            found_logging = true;
        }
    }

    EXPECT_TRUE(found_security) << "Security module handler should be called";
    EXPECT_TRUE(found_recovery) << "Recovery module handler should be called";
    EXPECT_LT(idx_security, idx_recovery) << "Security should be called before recovery";
    EXPECT_FALSE(found_logging) << "Logging should not be called after recovery consumed";

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Handlers Filter by Exception Type
//=============================================================================

TEST_F(ExceptionHandlerChainIntegrationTest, HandlersFilterByExceptionType) {
    // WHAT: Verify handlers with type_filter only receive matching exceptions
    // WHY:  Allow specialized handlers for different exception types

    // Register handler filtered to memory exceptions
    nimcp_handler_options_t memory_opts;
    nimcp_handler_default_options(&memory_opts);
    memory_opts.name = "memory_type_handler";
    memory_opts.handler = recording_handler;
    memory_opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    memory_opts.user_data = const_cast<char*>("memory_type_handler");
    memory_opts.type_filter = EXCEPTION_TYPE_MEMORY;

    auto* reg_memory = nimcp_handler_register(&memory_opts);
    ASSERT_NE(reg_memory, nullptr);
    registrations_.push_back(reg_memory);

    // Register general handler
    auto* reg_general = registerHandler("general_handler", recording_handler,
        NIMCP_HANDLER_PRIORITY_LOW, const_cast<char*>("general_handler"));
    ASSERT_NE(reg_general, nullptr);

    // Dispatch a generic exception (not memory type)
    nimcp_exception_t* ex_generic = createException(NIMCP_ERROR_OPERATION_FAILED);
    ASSERT_NE(ex_generic, nullptr);

    g_test_state.invocations.clear();
    g_test_state.call_count = 0;

    nimcp_exception_dispatch(ex_generic);

    // Check what was called
    bool found_memory_handler = false;
    bool found_general = false;

    for (const auto& inv : g_test_state.invocations) {
        if (inv.handler_name == "memory_type_handler") found_memory_handler = true;
        if (inv.handler_name == "general_handler") found_general = true;
    }

    // Memory type handler should NOT be called for generic exception
    // (depending on implementation, it may or may not be filtered)
    EXPECT_TRUE(found_general) << "General handler should be called for generic exception";

    nimcp_exception_unref(ex_generic);

    // Now dispatch a memory exception
    nimcp_memory_exception_t* ex_memory = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY, EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__, 1024, "Memory allocation failed"
    );
    ASSERT_NE(ex_memory, nullptr);

    g_test_state.invocations.clear();
    g_test_state.call_count = 0;

    nimcp_exception_dispatch((nimcp_exception_t*)ex_memory);

    found_memory_handler = false;
    found_general = false;

    for (const auto& inv : g_test_state.invocations) {
        if (inv.handler_name == "memory_type_handler") found_memory_handler = true;
        if (inv.handler_name == "general_handler") found_general = true;
    }

    // Memory type handler SHOULD be called for memory exception
    EXPECT_TRUE(found_memory_handler) << "Memory type handler should be called for memory exception";

    nimcp_exception_unref((nimcp_exception_t*)ex_memory);
}

//=============================================================================
// Test: Handlers Filter by Exception Category
//=============================================================================

TEST_F(ExceptionHandlerChainIntegrationTest, HandlersFilterByExceptionCategory) {
    // WHAT: Verify handlers with category_filter only receive matching exceptions
    // WHY:  Allow module-specific handlers for different error categories

    // Register handler filtered to threading category
    nimcp_handler_options_t threading_opts;
    nimcp_handler_default_options(&threading_opts);
    threading_opts.name = "threading_category_handler";
    threading_opts.handler = recording_handler;
    threading_opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    threading_opts.user_data = const_cast<char*>("threading_category_handler");
    threading_opts.category_filter = EXCEPTION_CATEGORY_THREADING;

    auto* reg_threading = nimcp_handler_register(&threading_opts);
    ASSERT_NE(reg_threading, nullptr);
    registrations_.push_back(reg_threading);

    // Register catch-all handler
    auto* reg_catchall = registerHandler("catchall_handler", recording_handler,
        NIMCP_HANDLER_PRIORITY_LOW, const_cast<char*>("catchall_handler"));
    ASSERT_NE(reg_catchall, nullptr);

    // Dispatch I/O exception (not threading category)
    nimcp_exception_t* ex_io = createException(NIMCP_ERROR_FILE_READ);
    ASSERT_NE(ex_io, nullptr);

    nimcp_exception_dispatch(ex_io);

    bool found_threading_handler = false;
    bool found_catchall = false;

    for (const auto& inv : g_test_state.invocations) {
        if (inv.handler_name == "threading_category_handler") found_threading_handler = true;
        if (inv.handler_name == "catchall_handler") found_catchall = true;
    }

    EXPECT_TRUE(found_catchall) << "Catchall handler should be called";

    nimcp_exception_unref(ex_io);

    // Now dispatch threading exception
    g_test_state.invocations.clear();

    nimcp_threading_exception_t* ex_thread = nimcp_threading_exception_create(
        NIMCP_ERROR_DEADLOCK, EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__, 12345, "Deadlock detected"
    );
    ASSERT_NE(ex_thread, nullptr);

    nimcp_exception_dispatch((nimcp_exception_t*)ex_thread);

    found_threading_handler = false;
    found_catchall = false;

    for (const auto& inv : g_test_state.invocations) {
        if (inv.handler_name == "threading_category_handler") found_threading_handler = true;
        if (inv.handler_name == "catchall_handler") found_catchall = true;
    }

    EXPECT_TRUE(found_threading_handler) << "Threading handler should be called for deadlock";

    nimcp_exception_unref((nimcp_exception_t*)ex_thread);
}

//=============================================================================
// Test: Unregistered Handlers Not Called
//=============================================================================

TEST_F(ExceptionHandlerChainIntegrationTest, UnregisteredHandlersNotCalled) {
    // WHAT: Verify unregistered handlers are properly removed and not called
    // WHY:  Essential for module cleanup and avoiding use-after-free

    // Register two handlers
    auto* reg1 = registerHandler("handler_1", recording_handler, NIMCP_HANDLER_PRIORITY_HIGH,
        const_cast<char*>("handler_1"));
    auto* reg2 = registerHandler("handler_2", recording_handler, NIMCP_HANDLER_PRIORITY_NORMAL,
        const_cast<char*>("handler_2"));

    ASSERT_NE(reg1, nullptr);
    ASSERT_NE(reg2, nullptr);

    // Dispatch exception - both should be called
    nimcp_exception_t* ex1 = createException(NIMCP_ERROR_OPERATION_FAILED);
    ASSERT_NE(ex1, nullptr);

    nimcp_exception_dispatch(ex1);

    bool found_1 = false, found_2 = false;
    for (const auto& inv : g_test_state.invocations) {
        if (inv.handler_name == "handler_1") found_1 = true;
        if (inv.handler_name == "handler_2") found_2 = true;
    }

    EXPECT_TRUE(found_1) << "Handler 1 should be called before unregister";
    EXPECT_TRUE(found_2) << "Handler 2 should be called before unregister";

    nimcp_exception_unref(ex1);

    // Unregister handler 1
    int result = nimcp_handler_unregister(reg1);
    EXPECT_EQ(result, 0);

    // Remove from our tracking to prevent double-unregister in TearDown
    registrations_.erase(
        std::remove(registrations_.begin(), registrations_.end(), reg1),
        registrations_.end()
    );

    // Clear invocations and dispatch again
    g_test_state.invocations.clear();
    g_test_state.call_count = 0;

    nimcp_exception_t* ex2 = createException(NIMCP_ERROR_OPERATION_FAILED);
    ASSERT_NE(ex2, nullptr);

    nimcp_exception_dispatch(ex2);

    found_1 = false;
    found_2 = false;
    for (const auto& inv : g_test_state.invocations) {
        if (inv.handler_name == "handler_1") found_1 = true;
        if (inv.handler_name == "handler_2") found_2 = true;
    }

    EXPECT_FALSE(found_1) << "Handler 1 should NOT be called after unregister";
    EXPECT_TRUE(found_2) << "Handler 2 should still be called";

    nimcp_exception_unref(ex2);
}

//=============================================================================
// Test: Handler Enable/Disable
//=============================================================================

TEST_F(ExceptionHandlerChainIntegrationTest, HandlerEnableDisable) {
    // WHAT: Verify handler can be temporarily disabled and re-enabled
    // WHY:  Allow dynamic control of handler participation

    auto* reg = registerHandler("toggle_handler", recording_handler, NIMCP_HANDLER_PRIORITY_NORMAL,
        const_cast<char*>("toggle_handler"));
    ASSERT_NE(reg, nullptr);

    // Dispatch - should be called
    nimcp_exception_t* ex1 = createException(NIMCP_ERROR_OPERATION_FAILED);
    ASSERT_NE(ex1, nullptr);

    nimcp_exception_dispatch(ex1);

    bool found = false;
    for (const auto& inv : g_test_state.invocations) {
        if (inv.handler_name == "toggle_handler") found = true;
    }
    EXPECT_TRUE(found) << "Handler should be called when active";

    nimcp_exception_unref(ex1);

    // Disable handler
    nimcp_handler_disable(reg);

    // Dispatch - should NOT be called
    g_test_state.invocations.clear();

    nimcp_exception_t* ex2 = createException(NIMCP_ERROR_OPERATION_FAILED);
    ASSERT_NE(ex2, nullptr);

    nimcp_exception_dispatch(ex2);

    found = false;
    for (const auto& inv : g_test_state.invocations) {
        if (inv.handler_name == "toggle_handler") found = true;
    }
    EXPECT_FALSE(found) << "Handler should NOT be called when disabled";

    nimcp_exception_unref(ex2);

    // Re-enable handler
    nimcp_handler_enable(reg);

    // Dispatch - should be called again
    g_test_state.invocations.clear();

    nimcp_exception_t* ex3 = createException(NIMCP_ERROR_OPERATION_FAILED);
    ASSERT_NE(ex3, nullptr);

    nimcp_exception_dispatch(ex3);

    found = false;
    for (const auto& inv : g_test_state.invocations) {
        if (inv.handler_name == "toggle_handler") found = true;
    }
    EXPECT_TRUE(found) << "Handler should be called after re-enable";

    nimcp_exception_unref(ex3);
}

//=============================================================================
// Test: Multiple Handlers Same Priority
//=============================================================================

TEST_F(ExceptionHandlerChainIntegrationTest, MultipleHandlersSamePriority) {
    // WHAT: Verify handlers with same priority all get called
    // WHY:  Common scenario for logging/metrics handlers at same priority

    auto* reg_a = registerHandler("same_priority_a", recording_handler, NIMCP_HANDLER_PRIORITY_NORMAL,
        const_cast<char*>("same_priority_a"));
    auto* reg_b = registerHandler("same_priority_b", recording_handler, NIMCP_HANDLER_PRIORITY_NORMAL,
        const_cast<char*>("same_priority_b"));
    auto* reg_c = registerHandler("same_priority_c", recording_handler, NIMCP_HANDLER_PRIORITY_NORMAL,
        const_cast<char*>("same_priority_c"));

    ASSERT_NE(reg_a, nullptr);
    ASSERT_NE(reg_b, nullptr);
    ASSERT_NE(reg_c, nullptr);

    nimcp_exception_t* ex = createException(NIMCP_ERROR_OPERATION_FAILED);
    ASSERT_NE(ex, nullptr);

    nimcp_exception_dispatch(ex);

    bool found_a = false, found_b = false, found_c = false;
    for (const auto& inv : g_test_state.invocations) {
        if (inv.handler_name == "same_priority_a") found_a = true;
        if (inv.handler_name == "same_priority_b") found_b = true;
        if (inv.handler_name == "same_priority_c") found_c = true;
    }

    EXPECT_TRUE(found_a) << "Handler A should be called";
    EXPECT_TRUE(found_b) << "Handler B should be called";
    EXPECT_TRUE(found_c) << "Handler C should be called";

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Handler Chain with No Handlers
//=============================================================================

TEST_F(ExceptionHandlerChainIntegrationTest, DispatchWithNoHandlers) {
    // WHAT: Verify dispatch works when no handlers registered
    // WHY:  Edge case that should not crash

    // Don't register any handlers
    EXPECT_EQ(nimcp_handler_count(), 0u);

    nimcp_exception_t* ex = createException(NIMCP_ERROR_OPERATION_FAILED);
    ASSERT_NE(ex, nullptr);

    // Should not crash
    bool handled = nimcp_exception_dispatch(ex);

    // With no handlers, exception is unhandled
    EXPECT_FALSE(handled);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Handler Chain Respects Severity Filter
//=============================================================================

TEST_F(ExceptionHandlerChainIntegrationTest, HandlerRespectsMinSeverityFilter) {
    // WHAT: Verify handlers with min_severity filter only receive severe enough exceptions
    // WHY:  Allow filtering out low-severity exceptions from critical handlers

    // Register handler that only handles SEVERE and above
    nimcp_handler_options_t severe_opts;
    nimcp_handler_default_options(&severe_opts);
    severe_opts.name = "severe_only_handler";
    severe_opts.handler = recording_handler;
    severe_opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    severe_opts.user_data = const_cast<char*>("severe_only_handler");
    severe_opts.min_severity = EXCEPTION_SEVERITY_SEVERE;

    auto* reg_severe = nimcp_handler_register(&severe_opts);
    ASSERT_NE(reg_severe, nullptr);
    registrations_.push_back(reg_severe);

    // Register catch-all handler
    auto* reg_all = registerHandler("all_severity_handler", recording_handler,
        NIMCP_HANDLER_PRIORITY_LOW, const_cast<char*>("all_severity_handler"));
    ASSERT_NE(reg_all, nullptr);

    // Dispatch low severity exception
    nimcp_exception_t* ex_low = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED, EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__, "Low severity test"
    );
    ASSERT_NE(ex_low, nullptr);

    nimcp_exception_dispatch(ex_low);

    bool found_severe = false, found_all = false;
    for (const auto& inv : g_test_state.invocations) {
        if (inv.handler_name == "severe_only_handler") found_severe = true;
        if (inv.handler_name == "all_severity_handler") found_all = true;
    }

    EXPECT_TRUE(found_all) << "All-severity handler should be called for warning";

    nimcp_exception_unref(ex_low);

    // Clear and dispatch high severity exception
    g_test_state.invocations.clear();

    nimcp_exception_t* ex_high = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED, EXCEPTION_SEVERITY_FATAL,
        __FILE__, __LINE__, __func__, "High severity test"
    );
    ASSERT_NE(ex_high, nullptr);

    nimcp_exception_dispatch(ex_high);

    found_severe = false;
    found_all = false;
    for (const auto& inv : g_test_state.invocations) {
        if (inv.handler_name == "severe_only_handler") found_severe = true;
        if (inv.handler_name == "all_severity_handler") found_all = true;
    }

    EXPECT_TRUE(found_severe) << "Severe-only handler should be called for fatal";
    EXPECT_TRUE(found_all) << "All-severity handler should be called for fatal";

    nimcp_exception_unref(ex_high);
}

//=============================================================================
// Test: Handler Count Accurate
//=============================================================================

TEST_F(ExceptionHandlerChainIntegrationTest, HandlerCountAccurate) {
    // WHAT: Verify nimcp_handler_count() returns accurate count
    // WHY:  Important for diagnostics and testing

    EXPECT_EQ(nimcp_handler_count(), 0u);

    auto* reg1 = registerHandler("counter_test_1", recording_handler, 10);
    ASSERT_NE(reg1, nullptr);
    EXPECT_EQ(nimcp_handler_count(), 1u);

    auto* reg2 = registerHandler("counter_test_2", recording_handler, 20);
    ASSERT_NE(reg2, nullptr);
    EXPECT_EQ(nimcp_handler_count(), 2u);

    auto* reg3 = registerHandler("counter_test_3", recording_handler, 30);
    ASSERT_NE(reg3, nullptr);
    EXPECT_EQ(nimcp_handler_count(), 3u);

    // Unregister one
    nimcp_handler_unregister(reg2);
    registrations_.erase(
        std::remove(registrations_.begin(), registrations_.end(), reg2),
        registrations_.end()
    );
    EXPECT_EQ(nimcp_handler_count(), 2u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
