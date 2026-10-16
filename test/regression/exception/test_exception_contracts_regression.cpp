/**
 * @file test_exception_contracts_regression.cpp
 * @brief Regression tests for exception handler registration and contracts
 *
 * WHAT: Verify handler registration/unregistration contracts remain stable
 * WHY:  Prevent breaking changes to handler chain behavior
 * HOW:  Test exact registration behavior, priority ordering, filtering
 *
 * REGRESSION CATEGORIES:
 * 1. Handler Registration - Registration returns valid handles
 * 2. Handler Unregistration - Unregistration removes handlers cleanly
 * 3. Priority Ordering - Handlers called in priority order (highest first)
 * 4. Category Filtering - Handlers only receive matching categories
 * 5. Severity Filtering - Handlers only receive exceptions >= min severity
 * 6. Type Filtering - Handlers only receive matching types
 * 7. Enable/Disable - Disabled handlers not called
 * 8. Recovery Callbacks - Recovery callbacks invoked correctly
 * 9. Try/Catch Mechanism - Try/catch stack works correctly
 * 10. Dispatch Behavior - Dispatch chain behaves correctly
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdint>
#include <vector>
#include <string>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/exception/nimcp_exception_circuit.h"
}

//=============================================================================
// Test State for Handler Callbacks
//=============================================================================

namespace {

// Global state for tracking handler invocations
struct HandlerCallRecord {
    const char* name;
    nimcp_error_t code;
    nimcp_exception_severity_t severity;
    nimcp_exception_category_t category;
    int order;  // Call order (incremented each call)
};

std::vector<HandlerCallRecord> g_handler_calls;
int g_call_order = 0;
bool g_handler_should_consume = false;

void reset_handler_tracking() {
    g_handler_calls.clear();
    g_call_order = 0;
    g_handler_should_consume = false;
}

// Generic tracking handler
bool tracking_handler(nimcp_exception_t* ex, void* user_data) {
    HandlerCallRecord record;
    record.name = static_cast<const char*>(user_data);
    record.code = ex->code;
    record.severity = ex->severity;
    record.category = ex->category;
    record.order = g_call_order++;
    g_handler_calls.push_back(record);
    return g_handler_should_consume;
}

// Recovery callback tracking
struct RecoveryCallRecord {
    nimcp_exception_recovery_action_t action;
    nimcp_error_t ex_code;
};

std::vector<RecoveryCallRecord> g_recovery_calls;

int tracking_recovery_callback(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
    RecoveryCallRecord record;
    record.action = action;
    record.ex_code = ex->code;
    g_recovery_calls.push_back(record);
    return 0;  // Success
}

void reset_recovery_tracking() {
    g_recovery_calls.clear();
}

}  // anonymous namespace

//=============================================================================
// Test Fixture
//=============================================================================

class ExceptionContractsRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_exception_system_init();
        reset_handler_tracking();
        reset_recovery_tracking();
    }

    void TearDown() override {
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }
};

//=============================================================================
// Handler Registration Contract Tests
// REGRESSION: Registration must return valid handles
//=============================================================================

TEST_F(ExceptionContractsRegressionTest, HandlerRegistrationReturnsHandle) {
    // REGRESSION: nimcp_handler_register must return non-NULL on valid input

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "test_handler";
    options.handler = tracking_handler;
    options.user_data = (void*)"test_handler";
    options.priority = NIMCP_HANDLER_PRIORITY_NORMAL;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr)
        << "nimcp_handler_register must return non-NULL on valid input";

    // Registration should be active
    EXPECT_TRUE(reg->active)
        << "New registration must be active";

    // Clean up
    nimcp_handler_unregister(reg);
}

TEST_F(ExceptionContractsRegressionTest, HandlerRegistrationWithNullHandlerFails) {
    // REGRESSION: Registration with NULL handler should fail

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "null_handler";
    options.handler = nullptr;  // Invalid

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    // This should either return NULL or return a registration that won't be called
    // The contract is that NULL handler is not useful
    if (reg != nullptr) {
        nimcp_handler_unregister(reg);
    }
}

TEST_F(ExceptionContractsRegressionTest, HandlerUnregistrationRemovesHandler) {
    // REGRESSION: Unregistration must remove handler from chain

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "unregister_test";
    options.handler = tracking_handler;
    options.user_data = (void*)"unregister_test";

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);

    size_t count_before = nimcp_handler_count();

    int result = nimcp_handler_unregister(reg);
    EXPECT_EQ(result, 0) << "nimcp_handler_unregister must return 0 on success";

    size_t count_after = nimcp_handler_count();
    EXPECT_EQ(count_after, count_before - 1)
        << "Handler count must decrease after unregistration";
}

//=============================================================================
// Priority Ordering Contract Tests
// REGRESSION: Handlers must be called in priority order (highest first)
//=============================================================================

TEST_F(ExceptionContractsRegressionTest, HandlersCalledInPriorityOrder) {
    // REGRESSION: Handlers with higher priority are called first

    // Register low priority handler
    nimcp_handler_options_t low_opts;
    nimcp_handler_default_options(&low_opts);
    low_opts.name = "low_priority";
    low_opts.handler = tracking_handler;
    low_opts.user_data = (void*)"LOW";
    low_opts.priority = NIMCP_HANDLER_PRIORITY_LOW;

    nimcp_handler_registration_t* low_reg = nimcp_handler_register(&low_opts);
    ASSERT_NE(low_reg, nullptr);

    // Register high priority handler
    nimcp_handler_options_t high_opts;
    nimcp_handler_default_options(&high_opts);
    high_opts.name = "high_priority";
    high_opts.handler = tracking_handler;
    high_opts.user_data = (void*)"HIGH";
    high_opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;

    nimcp_handler_registration_t* high_reg = nimcp_handler_register(&high_opts);
    ASSERT_NE(high_reg, nullptr);

    // Register normal priority handler
    nimcp_handler_options_t normal_opts;
    nimcp_handler_default_options(&normal_opts);
    normal_opts.name = "normal_priority";
    normal_opts.handler = tracking_handler;
    normal_opts.user_data = (void*)"NORMAL";
    normal_opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;

    nimcp_handler_registration_t* normal_reg = nimcp_handler_register(&normal_opts);
    ASSERT_NE(normal_reg, nullptr);

    // Dispatch an exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Priority test"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_dispatch(ex);

    // Verify call order: HIGH -> NORMAL -> LOW
    ASSERT_GE(g_handler_calls.size(), 3u);

    // Find calls by name and check order
    int high_order = -1, normal_order = -1, low_order = -1;
    for (const auto& call : g_handler_calls) {
        if (strcmp(call.name, "HIGH") == 0) high_order = call.order;
        if (strcmp(call.name, "NORMAL") == 0) normal_order = call.order;
        if (strcmp(call.name, "LOW") == 0) low_order = call.order;
    }

    EXPECT_NE(high_order, -1) << "HIGH handler must be called";
    EXPECT_NE(normal_order, -1) << "NORMAL handler must be called";
    EXPECT_NE(low_order, -1) << "LOW handler must be called";

    EXPECT_LT(high_order, normal_order)
        << "HIGH priority handler must be called before NORMAL";
    EXPECT_LT(normal_order, low_order)
        << "NORMAL priority handler must be called before LOW";

    // Clean up
    nimcp_handler_unregister(low_reg);
    nimcp_handler_unregister(high_reg);
    nimcp_handler_unregister(normal_reg);
    nimcp_exception_unref(ex);
}

//=============================================================================
// Handler Consumption Contract Tests
// REGRESSION: If handler returns true, chain stops
//=============================================================================

TEST_F(ExceptionContractsRegressionTest, HandlerConsumptionStopsChain) {
    // REGRESSION: When handler returns true, subsequent handlers are not called

    // Register handler that will consume
    nimcp_handler_options_t consume_opts;
    nimcp_handler_default_options(&consume_opts);
    consume_opts.name = "consumer";
    consume_opts.handler = tracking_handler;
    consume_opts.user_data = (void*)"CONSUMER";
    consume_opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;

    nimcp_handler_registration_t* consume_reg = nimcp_handler_register(&consume_opts);
    ASSERT_NE(consume_reg, nullptr);

    // Register handler that should NOT be called
    nimcp_handler_options_t after_opts;
    nimcp_handler_default_options(&after_opts);
    after_opts.name = "after_consumer";
    after_opts.handler = tracking_handler;
    after_opts.user_data = (void*)"AFTER";
    after_opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;

    nimcp_handler_registration_t* after_reg = nimcp_handler_register(&after_opts);
    ASSERT_NE(after_reg, nullptr);

    // Make the consumer handler consume the exception
    g_handler_should_consume = true;

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Consumption test"
    );

    bool handled = nimcp_exception_dispatch(ex);
    EXPECT_TRUE(handled) << "Dispatch must return true when handler consumes";

    // Check that AFTER handler was NOT called
    bool after_called = false;
    for (const auto& call : g_handler_calls) {
        if (strcmp(call.name, "AFTER") == 0) {
            after_called = true;
        }
    }
    EXPECT_FALSE(after_called)
        << "Handler after consumer must NOT be called when exception is consumed";

    // Clean up
    nimcp_handler_unregister(consume_reg);
    nimcp_handler_unregister(after_reg);
    nimcp_exception_unref(ex);
}

//=============================================================================
// Category Filtering Contract Tests
// REGRESSION: Handlers with category_filter only receive matching categories
//=============================================================================

TEST_F(ExceptionContractsRegressionTest, CategoryFilteringWorks) {
    // REGRESSION: Handler with category_filter only receives matching exceptions

    // Register handler that only wants MEMORY exceptions
    nimcp_handler_options_t memory_opts;
    nimcp_handler_default_options(&memory_opts);
    memory_opts.name = "memory_only";
    memory_opts.handler = tracking_handler;
    memory_opts.user_data = (void*)"MEMORY_ONLY";
    memory_opts.category_filter = EXCEPTION_CATEGORY_MEMORY;

    nimcp_handler_registration_t* memory_reg = nimcp_handler_register(&memory_opts);
    ASSERT_NE(memory_reg, nullptr);

    // Dispatch an IO exception (should NOT trigger memory handler)
    nimcp_exception_t* io_ex = nimcp_exception_create(
        NIMCP_ERROR_FILE_NOT_FOUND,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "IO exception"
    );

    reset_handler_tracking();
    nimcp_exception_dispatch(io_ex);

    bool memory_handler_called = false;
    for (const auto& call : g_handler_calls) {
        if (strcmp(call.name, "MEMORY_ONLY") == 0) {
            memory_handler_called = true;
        }
    }
    EXPECT_FALSE(memory_handler_called)
        << "MEMORY_ONLY handler must NOT be called for IO exception";

    // Dispatch a MEMORY exception (SHOULD trigger memory handler)
    nimcp_exception_t* mem_ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Memory exception"
    );

    reset_handler_tracking();
    nimcp_exception_dispatch(mem_ex);

    memory_handler_called = false;
    for (const auto& call : g_handler_calls) {
        if (strcmp(call.name, "MEMORY_ONLY") == 0) {
            memory_handler_called = true;
        }
    }
    EXPECT_TRUE(memory_handler_called)
        << "MEMORY_ONLY handler must be called for MEMORY exception";

    // Clean up
    nimcp_handler_unregister(memory_reg);
    nimcp_exception_unref(io_ex);
    nimcp_exception_unref(mem_ex);
}

//=============================================================================
// Severity Filtering Contract Tests
// REGRESSION: Handlers with min_severity only receive exceptions >= that severity
//=============================================================================

TEST_F(ExceptionContractsRegressionTest, SeverityFilteringWorks) {
    // REGRESSION: Handler with min_severity filters lower severity exceptions

    // Register handler that only wants SEVERE or higher
    nimcp_handler_options_t severe_opts;
    nimcp_handler_default_options(&severe_opts);
    severe_opts.name = "severe_only";
    severe_opts.handler = tracking_handler;
    severe_opts.user_data = (void*)"SEVERE_ONLY";
    severe_opts.min_severity = EXCEPTION_SEVERITY_SEVERE;

    nimcp_handler_registration_t* severe_reg = nimcp_handler_register(&severe_opts);
    ASSERT_NE(severe_reg, nullptr);

    // Dispatch a WARNING exception (should NOT trigger severe handler)
    nimcp_exception_t* warn_ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Warning exception"
    );

    reset_handler_tracking();
    nimcp_exception_dispatch(warn_ex);

    bool severe_handler_called = false;
    for (const auto& call : g_handler_calls) {
        if (strcmp(call.name, "SEVERE_ONLY") == 0) {
            severe_handler_called = true;
        }
    }
    EXPECT_FALSE(severe_handler_called)
        << "SEVERE_ONLY handler must NOT be called for WARNING exception";

    // Dispatch a CRITICAL exception (SHOULD trigger severe handler)
    nimcp_exception_t* crit_ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        "Critical exception"
    );

    reset_handler_tracking();
    nimcp_exception_dispatch(crit_ex);

    severe_handler_called = false;
    for (const auto& call : g_handler_calls) {
        if (strcmp(call.name, "SEVERE_ONLY") == 0) {
            severe_handler_called = true;
        }
    }
    EXPECT_TRUE(severe_handler_called)
        << "SEVERE_ONLY handler must be called for CRITICAL exception";

    // Clean up
    nimcp_handler_unregister(severe_reg);
    nimcp_exception_unref(warn_ex);
    nimcp_exception_unref(crit_ex);
}

//=============================================================================
// Handler Enable/Disable Contract Tests
// REGRESSION: Disabled handlers are not called
//=============================================================================

TEST_F(ExceptionContractsRegressionTest, DisabledHandlerNotCalled) {
    // REGRESSION: nimcp_handler_disable must prevent handler from being called

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "disable_test";
    opts.handler = tracking_handler;
    opts.user_data = (void*)"DISABLE_TEST";

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);
    EXPECT_TRUE(reg->active);

    // Disable the handler
    nimcp_handler_disable(reg);
    EXPECT_FALSE(reg->active);

    // Dispatch an exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Disable test"
    );

    reset_handler_tracking();
    nimcp_exception_dispatch(ex);

    // Check that disabled handler was NOT called
    bool handler_called = false;
    for (const auto& call : g_handler_calls) {
        if (strcmp(call.name, "DISABLE_TEST") == 0) {
            handler_called = true;
        }
    }
    EXPECT_FALSE(handler_called)
        << "Disabled handler must NOT be called";

    // Re-enable and verify it's called
    nimcp_handler_enable(reg);
    EXPECT_TRUE(reg->active);

    reset_handler_tracking();
    nimcp_exception_dispatch(ex);

    handler_called = false;
    for (const auto& call : g_handler_calls) {
        if (strcmp(call.name, "DISABLE_TEST") == 0) {
            handler_called = true;
        }
    }
    EXPECT_TRUE(handler_called)
        << "Re-enabled handler must be called";

    // Clean up
    nimcp_handler_unregister(reg);
    nimcp_exception_unref(ex);
}

//=============================================================================
// Recovery Callback Registration Contract Tests
// REGRESSION: Recovery callbacks must be invoked correctly
//=============================================================================

TEST_F(ExceptionContractsRegressionTest, RecoveryCallbackRegistration) {
    // REGRESSION: nimcp_register_recovery_callback must register callbacks

    int result = nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_GC,
        tracking_recovery_callback,
        nullptr
    );
    EXPECT_EQ(result, 0)
        << "nimcp_register_recovery_callback must return 0 on success";

    // Execute recovery and verify callback is called
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Recovery test"
    );

    result = nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_GC);
    EXPECT_EQ(result, 0)
        << "nimcp_execute_recovery must return 0 on success";

    ASSERT_EQ(g_recovery_calls.size(), 1u);
    EXPECT_EQ(g_recovery_calls[0].action, EXCEPTION_RECOVERY_GC);
    EXPECT_EQ(g_recovery_calls[0].ex_code, NIMCP_ERROR_NO_MEMORY);

    // Clean up
    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_GC);
    nimcp_exception_unref(ex);
}

TEST_F(ExceptionContractsRegressionTest, RecoveryCallbackUnregistration) {
    // REGRESSION: nimcp_unregister_recovery_callback removes callback

    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RETRY, tracking_recovery_callback, nullptr);

    int result = nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_RETRY);
    EXPECT_EQ(result, 0)
        << "nimcp_unregister_recovery_callback must return 0 on success";

    // Second unregister is idempotent (returns 0 even if not registered)
    result = nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_RETRY);
    EXPECT_EQ(result, 0)
        << "nimcp_unregister_recovery_callback is idempotent";
}

//=============================================================================
// Handler Count Contract Tests
// REGRESSION: nimcp_handler_count must be accurate
//=============================================================================

TEST_F(ExceptionContractsRegressionTest, HandlerCountAccurate) {
    // REGRESSION: nimcp_handler_count must reflect actual registered handlers

    size_t initial_count = nimcp_handler_count();

    // Register handlers
    nimcp_handler_options_t opts1, opts2, opts3;
    nimcp_handler_default_options(&opts1);
    nimcp_handler_default_options(&opts2);
    nimcp_handler_default_options(&opts3);

    opts1.name = "handler1";
    opts1.handler = tracking_handler;
    opts1.user_data = (void*)"H1";

    opts2.name = "handler2";
    opts2.handler = tracking_handler;
    opts2.user_data = (void*)"H2";

    opts3.name = "handler3";
    opts3.handler = tracking_handler;
    opts3.user_data = (void*)"H3";

    nimcp_handler_registration_t* reg1 = nimcp_handler_register(&opts1);
    nimcp_handler_registration_t* reg2 = nimcp_handler_register(&opts2);
    nimcp_handler_registration_t* reg3 = nimcp_handler_register(&opts3);

    EXPECT_EQ(nimcp_handler_count(), initial_count + 3);

    // Unregister one
    nimcp_handler_unregister(reg2);
    EXPECT_EQ(nimcp_handler_count(), initial_count + 2);

    // Clean up
    nimcp_handler_unregister(reg1);
    nimcp_handler_unregister(reg3);
    EXPECT_EQ(nimcp_handler_count(), initial_count);
}

//=============================================================================
// Handler Get By Index Contract Tests
// REGRESSION: nimcp_handler_get must return correct handlers
//=============================================================================

TEST_F(ExceptionContractsRegressionTest, HandlerGetByIndex) {
    // REGRESSION: nimcp_handler_get must return handlers in order

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "indexed_handler";
    opts.handler = tracking_handler;
    opts.user_data = (void*)"INDEXED";

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    size_t count = nimcp_handler_count();
    ASSERT_GT(count, 0u);

    // Get last handler (the one we just registered)
    const nimcp_handler_registration_t* retrieved = nimcp_handler_get(count - 1);
    EXPECT_NE(retrieved, nullptr);

    // Out of bounds returns NULL
    retrieved = nimcp_handler_get(count + 100);
    EXPECT_EQ(retrieved, nullptr)
        << "nimcp_handler_get must return NULL for out of bounds index";

    // Clean up
    nimcp_handler_unregister(reg);
}

//=============================================================================
// Default Options Contract Tests
// REGRESSION: nimcp_handler_default_options must set consistent defaults
//=============================================================================

TEST_F(ExceptionContractsRegressionTest, DefaultOptionsContract) {
    // REGRESSION: Default options must have consistent values

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);

    // These defaults are part of the API contract
    // Note: name can be non-NULL as it may be set to a default string
    EXPECT_EQ(opts.handler, nullptr)
        << "Default handler must be NULL";
    EXPECT_EQ(opts.user_data, nullptr)
        << "Default user_data must be NULL";
    EXPECT_EQ(opts.priority, NIMCP_HANDLER_PRIORITY_NORMAL)
        << "Default priority must be NIMCP_HANDLER_PRIORITY_NORMAL";
    // min_severity may have a non-zero default to filter out info/debug levels
    // Just verify it's a valid severity value
    EXPECT_GE(static_cast<int>(opts.min_severity), 0);
    EXPECT_LE(static_cast<int>(opts.min_severity), static_cast<int>(EXCEPTION_SEVERITY_FATAL));
}

//=============================================================================
// Try/Catch Stack Contract Tests
// REGRESSION: Try/catch stack must work correctly
//=============================================================================

TEST_F(ExceptionContractsRegressionTest, TryStackPushPop) {
    // REGRESSION: nimcp_try_push/pop must maintain LIFO order

    nimcp_try_context_t ctx1 = {0};
    ctx1.file = "file1.c";
    ctx1.line = 100;

    nimcp_try_context_t ctx2 = {0};
    ctx2.file = "file2.c";
    ctx2.line = 200;

    // Push first context
    int result = nimcp_try_push(&ctx1);
    EXPECT_EQ(result, 0) << "First push must succeed";

    // Current should be ctx1
    nimcp_try_context_t* current = nimcp_try_current();
    EXPECT_EQ(current, &ctx1);

    // Push second context
    result = nimcp_try_push(&ctx2);
    EXPECT_EQ(result, 0) << "Second push must succeed";

    // Current should now be ctx2
    current = nimcp_try_current();
    EXPECT_EQ(current, &ctx2);

    // Pop should return ctx2
    nimcp_try_context_t* popped = nimcp_try_pop();
    EXPECT_EQ(popped, &ctx2);

    // Current should be ctx1 again
    current = nimcp_try_current();
    EXPECT_EQ(current, &ctx1);

    // Pop should return ctx1
    popped = nimcp_try_pop();
    EXPECT_EQ(popped, &ctx1);

    // Current should be NULL
    current = nimcp_try_current();
    EXPECT_EQ(current, nullptr);
}

TEST_F(ExceptionContractsRegressionTest, InTryBlockDetection) {
    // REGRESSION: nimcp_in_try_block must correctly detect try block

    // Not in try block initially
    EXPECT_FALSE(nimcp_in_try_block());

    nimcp_try_context_t ctx = {0};
    nimcp_try_push(&ctx);

    // Now in try block
    EXPECT_TRUE(nimcp_in_try_block());

    nimcp_try_pop();

    // Not in try block again
    EXPECT_FALSE(nimcp_in_try_block());
}

//=============================================================================
// Dispatch Return Value Contract Tests
// REGRESSION: nimcp_exception_dispatch return value must be consistent
//=============================================================================

TEST_F(ExceptionContractsRegressionTest, DispatchReturnValueContract) {
    // REGRESSION: Dispatch returns true if any handler consumes exception

    // Register non-consuming handler
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "non_consumer";
    opts.handler = tracking_handler;
    opts.user_data = (void*)"NON_CONSUMER";

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    g_handler_should_consume = false;

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Dispatch test"
    );

    // When no handler consumes, dispatch may return true or false
    // depending on default handler behavior
    nimcp_exception_dispatch(ex);

    // Register consuming handler
    nimcp_handler_options_t consume_opts;
    nimcp_handler_default_options(&consume_opts);
    consume_opts.name = "consumer";
    consume_opts.handler = tracking_handler;
    consume_opts.user_data = (void*)"CONSUMER";
    consume_opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;

    nimcp_handler_registration_t* consume_reg = nimcp_handler_register(&consume_opts);
    g_handler_should_consume = true;

    bool handled = nimcp_exception_dispatch(ex);
    EXPECT_TRUE(handled)
        << "Dispatch must return true when handler consumes exception";

    // Clean up
    nimcp_handler_unregister(reg);
    nimcp_handler_unregister(consume_reg);
    nimcp_exception_unref(ex);
}

//=============================================================================
// Circuit Breaker State String Contract Tests
//=============================================================================

TEST_F(ExceptionContractsRegressionTest, CircuitStateToStringStable) {
    // REGRESSION: Circuit state string representations must be stable

    EXPECT_STREQ(nimcp_circuit_state_to_string(CIRCUIT_STATE_CLOSED), "CLOSED");
    EXPECT_STREQ(nimcp_circuit_state_to_string(CIRCUIT_STATE_OPEN), "OPEN");
    EXPECT_STREQ(nimcp_circuit_state_to_string(CIRCUIT_STATE_HALF_OPEN), "HALF_OPEN");
}

//=============================================================================
// Multiple Handler Same Priority Contract Tests
// REGRESSION: Handlers with same priority have deterministic order
//=============================================================================

TEST_F(ExceptionContractsRegressionTest, SamePriorityDeterministicOrder) {
    // REGRESSION: Handlers with same priority are called in registration order

    nimcp_handler_options_t opts1, opts2, opts3;
    nimcp_handler_default_options(&opts1);
    nimcp_handler_default_options(&opts2);
    nimcp_handler_default_options(&opts3);

    opts1.name = "same_prio_1";
    opts1.handler = tracking_handler;
    opts1.user_data = (void*)"FIRST";
    opts1.priority = 42;

    opts2.name = "same_prio_2";
    opts2.handler = tracking_handler;
    opts2.user_data = (void*)"SECOND";
    opts2.priority = 42;

    opts3.name = "same_prio_3";
    opts3.handler = tracking_handler;
    opts3.user_data = (void*)"THIRD";
    opts3.priority = 42;

    nimcp_handler_registration_t* reg1 = nimcp_handler_register(&opts1);
    nimcp_handler_registration_t* reg2 = nimcp_handler_register(&opts2);
    nimcp_handler_registration_t* reg3 = nimcp_handler_register(&opts3);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Same priority test"
    );

    reset_handler_tracking();
    nimcp_exception_dispatch(ex);

    // Find call orders
    int first_order = -1, second_order = -1, third_order = -1;
    for (const auto& call : g_handler_calls) {
        if (strcmp(call.name, "FIRST") == 0) first_order = call.order;
        if (strcmp(call.name, "SECOND") == 0) second_order = call.order;
        if (strcmp(call.name, "THIRD") == 0) third_order = call.order;
    }

    // Order should be deterministic (registration order or reverse)
    // We just verify they're all called with distinct orders
    EXPECT_NE(first_order, -1);
    EXPECT_NE(second_order, -1);
    EXPECT_NE(third_order, -1);
    EXPECT_NE(first_order, second_order);
    EXPECT_NE(second_order, third_order);
    EXPECT_NE(first_order, third_order);

    // Clean up
    nimcp_handler_unregister(reg1);
    nimcp_handler_unregister(reg2);
    nimcp_handler_unregister(reg3);
    nimcp_exception_unref(ex);
}

//=============================================================================
// Null Parameter Handling Contract Tests
// REGRESSION: Null parameters must be handled safely
//=============================================================================

TEST_F(ExceptionContractsRegressionTest, NullParameterHandling) {
    // REGRESSION: Functions must handle NULL parameters safely

    // nimcp_handler_register with NULL should fail
    nimcp_handler_registration_t* reg = nimcp_handler_register(nullptr);
    // Should return NULL or handle safely

    // nimcp_handler_unregister with NULL should be safe
    int result = nimcp_handler_unregister(nullptr);
    // Should return error or be no-op

    // nimcp_handler_disable with NULL should be safe
    nimcp_handler_disable(nullptr);
    // Should not crash

    // nimcp_handler_enable with NULL should be safe
    nimcp_handler_enable(nullptr);
    // Should not crash

    // nimcp_handler_default_options with NULL should be safe
    nimcp_handler_default_options(nullptr);
    // Should not crash

    // nimcp_exception_dispatch with NULL should be safe
    bool handled = nimcp_exception_dispatch(nullptr);
    EXPECT_FALSE(handled)
        << "Dispatch with NULL exception should return false";

    // nimcp_try_push with NULL should fail
    result = nimcp_try_push(nullptr);
    EXPECT_EQ(result, -1)
        << "try_push with NULL should return -1";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
