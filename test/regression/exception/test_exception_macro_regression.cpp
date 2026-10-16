/**
 * @file test_exception_macro_regression.cpp
 * @brief Regression tests for exception macro API stability
 *
 * WHAT: Verify exception macro API contracts remain stable
 * WHY:  Prevent breaking changes to public exception macro interfaces
 * HOW:  Test exact macro behavior, parameter passing, and return codes
 *
 * REGRESSION CATEGORIES:
 * 1. Macro Interface Stability - Macro signatures must not change
 * 2. Return Code Consistency - Macros must produce consistent error codes
 * 3. Exception Creation - Macros must create valid exceptions
 * 4. Immune Integration - Immune-related macros must work correctly
 * 5. Memory Safety - Exception cleanup must work properly
 * 6. Thread Safety - Thread-local state must be handled correctly
 *
 * HEADER FILES READ:
 * - nimcp_exception_macros.h: Defines NIMCP_THROW, NIMCP_THROW_IF,
 *   NIMCP_CHECK_THROW, NIMCP_THROW_TO_IMMUNE, typed exception macros
 * - nimcp_exception.h: Defines exception types and creation functions
 * - nimcp_exception_handlers.h: Defines handler registration and dispatch
 *
 * @author NIMCP Development Team
 * @date 2026-01-21
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdint>
#include <atomic>
#include <thread>
#include <vector>

extern "C" {
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class ExceptionMacroRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_exception_system_init();
        nimcp_exception_clear_current();
    }

    void TearDown() override {
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }
};

//=============================================================================
// KG Wiring Constants Regression Tests
// REGRESSION: KG wiring constants must remain stable for message routing
//=============================================================================

TEST_F(ExceptionMacroRegressionTest, KGWiringConstantsNeverChange) {
    // REGRESSION: KG message type constants are used for inter-module communication

    EXPECT_STREQ(KG_MSG_EXCEPTION_RAISED, "EXCEPTION_RAISED")
        << "KG_MSG_EXCEPTION_RAISED must equal 'EXCEPTION_RAISED'";

    EXPECT_STREQ(KG_MSG_ANTIGEN_PRESENTED, "ANTIGEN_PRESENTED")
        << "KG_MSG_ANTIGEN_PRESENTED must equal 'ANTIGEN_PRESENTED'";

    EXPECT_STREQ(KG_MSG_RECOVERY_REQUEST, "RECOVERY_REQUEST")
        << "KG_MSG_RECOVERY_REQUEST must equal 'RECOVERY_REQUEST'";

    EXPECT_STREQ(KG_MSG_RECOVERY_RESULT, "RECOVERY_RESULT")
        << "KG_MSG_RECOVERY_RESULT must equal 'RECOVERY_RESULT'";

    EXPECT_STREQ(KG_MSG_ERROR_REPORT, "ERROR_REPORT")
        << "KG_MSG_ERROR_REPORT must equal 'ERROR_REPORT'";

    EXPECT_STREQ(KG_MSG_CRASH_SIGNAL, "CRASH_SIGNAL")
        << "KG_MSG_CRASH_SIGNAL must equal 'CRASH_SIGNAL'";

    EXPECT_STREQ(KG_EXCEPTION_MODULE_NAME, "exception_handler")
        << "KG_EXCEPTION_MODULE_NAME must equal 'exception_handler'";

    EXPECT_STREQ(KG_EXCEPTION_MODULE_TYPE, "FAULT_TOLERANCE")
        << "KG_EXCEPTION_MODULE_TYPE must equal 'FAULT_TOLERANCE'";
}

//=============================================================================
// NIMCP_THROW Macro Regression Tests
// REGRESSION: Basic throw macro must work consistently
//=============================================================================

TEST_F(ExceptionMacroRegressionTest, ThrowMacroCreatesValidException) {
    // REGRESSION: NIMCP_THROW must create exception with correct error code

    // We need to capture the exception via handler
    static nimcp_exception_t* captured_ex = nullptr;

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "test_handler";
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        captured_ex = nimcp_exception_ref(ex);
        return true;  // Mark as handled
    };

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Use NIMCP_THROW macro
    NIMCP_THROW(NIMCP_ERROR_UNKNOWN, "Test throw macro: value=%d", 42);

    // Verify exception was captured
    ASSERT_NE(captured_ex, nullptr) << "NIMCP_THROW must create and dispatch exception";
    EXPECT_EQ(captured_ex->code, NIMCP_ERROR_UNKNOWN)
        << "Exception code must match the code passed to NIMCP_THROW";
    EXPECT_NE(strstr(captured_ex->message, "Test throw macro"), nullptr)
        << "Exception message must contain formatted text";
    EXPECT_NE(strstr(captured_ex->message, "42"), nullptr)
        << "Exception message must contain formatted value";

    // Cleanup
    nimcp_exception_unref(captured_ex);
    captured_ex = nullptr;
    nimcp_handler_unregister(reg);
}

TEST_F(ExceptionMacroRegressionTest, ThrowMacroSetsSourceLocation) {
    // REGRESSION: NIMCP_THROW must capture __FILE__, __LINE__, __func__

    static nimcp_exception_t* captured_ex = nullptr;

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "location_test_handler";
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        captured_ex = nimcp_exception_ref(ex);
        return true;
    };

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    int expected_line = __LINE__ + 1;
    NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Location test");

    ASSERT_NE(captured_ex, nullptr);
    EXPECT_NE(captured_ex->file, nullptr) << "File must be set";
    EXPECT_NE(strstr(captured_ex->file, "test_exception_macro_regression"), nullptr)
        << "File must contain test file name";
    EXPECT_EQ(captured_ex->line, expected_line)
        << "Line number must match the NIMCP_THROW call line";
    EXPECT_NE(captured_ex->function, nullptr) << "Function must be set";

    nimcp_exception_unref(captured_ex);
    captured_ex = nullptr;
    nimcp_handler_unregister(reg);
}

//=============================================================================
// NIMCP_THROW_IF Macro Regression Tests
// REGRESSION: Conditional throw must only throw when condition is false
//=============================================================================

TEST_F(ExceptionMacroRegressionTest, ThrowIfOnlyThrowsWhenConditionTrue) {
    // REGRESSION: NIMCP_THROW_IF(cond, ...) throws only when cond is true
    // This matches standard semantics: "throw IF (the condition is met)"

    static bool handler_called = false;

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "throw_if_test_handler";
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        handler_called = true;
        return true;
    };

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Test with false condition - should NOT throw
    handler_called = false;
    NIMCP_THROW_IF(false, NIMCP_ERROR_UNKNOWN, "Should not throw");
    EXPECT_FALSE(handler_called)
        << "NIMCP_THROW_IF must NOT throw when condition is false";

    // Test with true condition - should throw
    handler_called = false;
    NIMCP_THROW_IF(true, NIMCP_ERROR_UNKNOWN, "Should throw");
    EXPECT_TRUE(handler_called)
        << "NIMCP_THROW_IF must throw when condition is true";

    nimcp_handler_unregister(reg);
}

//=============================================================================
// NIMCP_CHECK_THROW Macro Regression Tests
// REGRESSION: Check-throw must return error code when condition fails
//=============================================================================

// Helper function that uses NIMCP_CHECK_THROW
static nimcp_error_t check_throw_helper(bool condition) {
    NIMCP_CHECK_THROW(condition, NIMCP_ERROR_NULL_POINTER, "Check failed");
    return NIMCP_SUCCESS;
}

TEST_F(ExceptionMacroRegressionTest, CheckThrowReturnsErrorCodeOnFailure) {
    // REGRESSION: NIMCP_CHECK_THROW must return the error code when condition fails

    // Install a handler to prevent unhandled exception warnings
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "check_throw_handler";
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        return true;  // Handle silently
    };

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Test with true condition - should return NIMCP_SUCCESS
    nimcp_error_t result = check_throw_helper(true);
    EXPECT_EQ(result, NIMCP_SUCCESS)
        << "NIMCP_CHECK_THROW must allow continuation when condition is true";

    // Test with false condition - should return the error code
    result = check_throw_helper(false);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER)
        << "NIMCP_CHECK_THROW must return error code when condition is false";

    nimcp_handler_unregister(reg);
}

//=============================================================================
// NIMCP_THROW_TO_IMMUNE Macro Regression Tests
// REGRESSION: Immune throw must present to immune system
//=============================================================================

TEST_F(ExceptionMacroRegressionTest, ThrowToImmunePresentsException) {
    // REGRESSION: NIMCP_THROW_TO_IMMUNE must set presented_to_immune flag

    static nimcp_exception_t* captured_ex = nullptr;

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "immune_test_handler";
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        captured_ex = nimcp_exception_ref(ex);
        return true;
    };

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Use NIMCP_THROW_TO_IMMUNE macro
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Memory test: size=%zu", (size_t)1024);

    ASSERT_NE(captured_ex, nullptr);
    EXPECT_TRUE(captured_ex->presented_to_immune)
        << "NIMCP_THROW_TO_IMMUNE must set presented_to_immune to true";
    EXPECT_EQ(captured_ex->code, NIMCP_ERROR_NO_MEMORY)
        << "Exception code must match";

    nimcp_exception_unref(captured_ex);
    captured_ex = nullptr;
    nimcp_handler_unregister(reg);
}

//=============================================================================
// NIMCP_THROW_TO_IMMUNE_IF Macro Regression Tests
// REGRESSION: Conditional immune throw must only present when condition is false
//=============================================================================

TEST_F(ExceptionMacroRegressionTest, ThrowToImmuneIfConditional) {
    // REGRESSION: NIMCP_THROW_TO_IMMUNE_IF must only throw when condition is false

    static bool handler_called = false;

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "immune_if_test_handler";
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        handler_called = true;
        return true;
    };

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Test with true condition - should NOT throw
    handler_called = false;
    NIMCP_THROW_TO_IMMUNE_IF(true, NIMCP_ERROR_NO_MEMORY, "Should not throw");
    EXPECT_FALSE(handler_called)
        << "NIMCP_THROW_TO_IMMUNE_IF must NOT throw when condition is true";

    // Test with false condition - should throw
    handler_called = false;
    NIMCP_THROW_TO_IMMUNE_IF(false, NIMCP_ERROR_NO_MEMORY, "Should throw");
    EXPECT_TRUE(handler_called)
        << "NIMCP_THROW_TO_IMMUNE_IF must throw when condition is false";

    nimcp_handler_unregister(reg);
}

//=============================================================================
// NIMCP_CHECK_THROW_IMMUNE Macro Regression Tests
//=============================================================================

static nimcp_error_t check_throw_immune_helper(bool condition) {
    NIMCP_CHECK_THROW_IMMUNE(condition, NIMCP_ERROR_NO_MEMORY, "Check immune failed");
    return NIMCP_SUCCESS;
}

TEST_F(ExceptionMacroRegressionTest, CheckThrowImmuneReturnsAndPresents) {
    // REGRESSION: NIMCP_CHECK_THROW_IMMUNE must return error and present to immune

    static nimcp_exception_t* captured_ex = nullptr;

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "check_immune_handler";
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        captured_ex = nimcp_exception_ref(ex);
        return true;
    };

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Test with false condition
    nimcp_error_t result = check_throw_immune_helper(false);
    EXPECT_EQ(result, NIMCP_ERROR_NO_MEMORY)
        << "NIMCP_CHECK_THROW_IMMUNE must return error code";

    ASSERT_NE(captured_ex, nullptr);
    EXPECT_TRUE(captured_ex->presented_to_immune)
        << "NIMCP_CHECK_THROW_IMMUNE must set presented_to_immune";

    nimcp_exception_unref(captured_ex);
    captured_ex = nullptr;
    nimcp_handler_unregister(reg);
}

//=============================================================================
// NIMCP_THROW_SEVERITY Macro Regression Tests
// REGRESSION: Explicit severity must be respected
//=============================================================================

TEST_F(ExceptionMacroRegressionTest, ThrowSeverityUsesExplicitSeverity) {
    // REGRESSION: NIMCP_THROW_SEVERITY must use provided severity level

    static nimcp_exception_t* captured_ex = nullptr;

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "severity_test_handler";
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        captured_ex = nimcp_exception_ref(ex);
        return true;
    };

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Test with explicit severity
    NIMCP_THROW_SEVERITY(NIMCP_ERROR_UNKNOWN, EXCEPTION_SEVERITY_WARNING, "Warning test");

    ASSERT_NE(captured_ex, nullptr);
    EXPECT_EQ(captured_ex->severity, EXCEPTION_SEVERITY_WARNING)
        << "NIMCP_THROW_SEVERITY must use explicit severity";

    nimcp_exception_unref(captured_ex);
    captured_ex = nullptr;

    // Test NIMCP_THROW_CRITICAL
    NIMCP_THROW_CRITICAL(NIMCP_ERROR_UNKNOWN, "Critical test");

    ASSERT_NE(captured_ex, nullptr);
    EXPECT_EQ(captured_ex->severity, EXCEPTION_SEVERITY_CRITICAL)
        << "NIMCP_THROW_CRITICAL must set CRITICAL severity";

    nimcp_exception_unref(captured_ex);
    captured_ex = nullptr;

    // Test NIMCP_THROW_FATAL
    NIMCP_THROW_FATAL(NIMCP_ERROR_UNKNOWN, "Fatal test");

    ASSERT_NE(captured_ex, nullptr);
    EXPECT_EQ(captured_ex->severity, EXCEPTION_SEVERITY_FATAL)
        << "NIMCP_THROW_FATAL must set FATAL severity";

    nimcp_exception_unref(captured_ex);
    captured_ex = nullptr;
    nimcp_handler_unregister(reg);
}

//=============================================================================
// NIMCP_THROW_AND_RECOVER Macro Regression Tests
// REGRESSION: Throw and recover must attempt recovery
//=============================================================================

TEST_F(ExceptionMacroRegressionTest, ThrowAndRecoverAttemptsRecovery) {
    // REGRESSION: NIMCP_THROW_AND_RECOVER must execute recovery action

    static bool recovery_called = false;
    static nimcp_exception_recovery_action_t recovery_action_received;

    // Register recovery callback
    nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_GC,
        [](nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) -> int {
            recovery_called = true;
            recovery_action_received = action;
            return 0;
        },
        nullptr
    );

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "recovery_test_handler";
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        return true;  // Handle silently
    };

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Use NIMCP_THROW_AND_RECOVER
    recovery_called = false;
    NIMCP_THROW_AND_RECOVER(NIMCP_ERROR_NO_MEMORY, EXCEPTION_RECOVERY_GC, "Recovery test");

    EXPECT_TRUE(recovery_called)
        << "NIMCP_THROW_AND_RECOVER must call recovery callback";
    EXPECT_EQ(recovery_action_received, EXCEPTION_RECOVERY_GC)
        << "Recovery action must match the requested action";

    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_GC);
    nimcp_handler_unregister(reg);
}

//=============================================================================
// NIMCP_SET_ERROR_EX Macro Regression Tests
// REGRESSION: Legacy compatibility macro must work correctly
//=============================================================================

TEST_F(ExceptionMacroRegressionTest, SetErrorExSetsErrorAndThrows) {
    // REGRESSION: NIMCP_SET_ERROR_EX must set error context AND throw exception

    static bool handler_called = false;

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "set_error_handler";
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        handler_called = true;
        return true;
    };

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Use NIMCP_SET_ERROR_EX
    handler_called = false;
    NIMCP_SET_ERROR_EX(NIMCP_ERROR_INVALID_PARAM, "Set error test");

    EXPECT_TRUE(handler_called)
        << "NIMCP_SET_ERROR_EX must dispatch exception";

    // Verify error context was also set
    nimcp_error_t last_error = nimcp_get_last_error();
    EXPECT_EQ(last_error, NIMCP_ERROR_INVALID_PARAM)
        << "NIMCP_SET_ERROR_EX must set thread-local error context";

    nimcp_handler_unregister(reg);
}

//=============================================================================
// Message Format Consistency Tests
// REGRESSION: Message formatting must work correctly with printf-style args
//=============================================================================

TEST_F(ExceptionMacroRegressionTest, MessageFormatConsistency) {
    // REGRESSION: Printf-style formatting must produce correct messages

    static nimcp_exception_t* captured_ex = nullptr;

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "format_test_handler";
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        captured_ex = nimcp_exception_ref(ex);
        return true;
    };

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Test with multiple format specifiers
    int int_val = 42;
    const char* str_val = "test_string";
    float float_val = 3.14f;

    NIMCP_THROW(NIMCP_ERROR_UNKNOWN, "int=%d str=%s float=%.2f", int_val, str_val, float_val);

    ASSERT_NE(captured_ex, nullptr);
    EXPECT_NE(strstr(captured_ex->message, "int=42"), nullptr)
        << "Integer formatting must work";
    EXPECT_NE(strstr(captured_ex->message, "str=test_string"), nullptr)
        << "String formatting must work";
    EXPECT_NE(strstr(captured_ex->message, "float=3.14"), nullptr)
        << "Float formatting must work";

    nimcp_exception_unref(captured_ex);
    captured_ex = nullptr;
    nimcp_handler_unregister(reg);
}

TEST_F(ExceptionMacroRegressionTest, MessageTruncationSafety) {
    // REGRESSION: Long messages must be safely truncated

    static nimcp_exception_t* captured_ex = nullptr;

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "truncation_test_handler";
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        captured_ex = nimcp_exception_ref(ex);
        return true;
    };

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Create a very long message (longer than NIMCP_EXCEPTION_MAX_MESSAGE = 256)
    char long_msg[512];
    memset(long_msg, 'A', sizeof(long_msg) - 1);
    long_msg[sizeof(long_msg) - 1] = '\0';

    NIMCP_THROW(NIMCP_ERROR_UNKNOWN, "%s", long_msg);

    ASSERT_NE(captured_ex, nullptr);

    // Message should be truncated but not crash
    size_t msg_len = strlen(captured_ex->message);
    EXPECT_LT(msg_len, NIMCP_EXCEPTION_MAX_MESSAGE)
        << "Message must be truncated to fit buffer";
    EXPECT_GT(msg_len, 0u)
        << "Message must not be empty";

    nimcp_exception_unref(captured_ex);
    captured_ex = nullptr;
    nimcp_handler_unregister(reg);
}

//=============================================================================
// Memory Leak Prevention Tests
// REGRESSION: Exceptions must be properly cleaned up
//=============================================================================

TEST_F(ExceptionMacroRegressionTest, ExceptionUnrefPreventsLeak) {
    // REGRESSION: nimcp_exception_unref must properly clean up exceptions

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Memory test"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->ref_count, 1) << "Initial ref count must be 1";

    // Add reference
    nimcp_exception_ref(ex);
    EXPECT_EQ(ex->ref_count, 2) << "Ref count must increase";

    // Remove reference
    nimcp_exception_unref(ex);
    EXPECT_EQ(ex->ref_count, 1) << "Ref count must decrease";

    // Final unref - should free (no crash = success)
    nimcp_exception_unref(ex);
}

TEST_F(ExceptionMacroRegressionTest, MacroDoesNotLeakOnHandledExceptions) {
    // REGRESSION: Exceptions dispatched via macros must be properly freed

    static int exception_count = 0;

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "leak_test_handler";
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        exception_count++;
        return true;  // Handler returns true, exception should be freed internally
    };

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Throw multiple exceptions
    for (int i = 0; i < 100; i++) {
        NIMCP_THROW(NIMCP_ERROR_UNKNOWN, "Leak test iteration %d", i);
    }

    EXPECT_EQ(exception_count, 100)
        << "All 100 exceptions must be dispatched";

    // If there's a memory leak, this test would show issues under memory sanitizers

    nimcp_handler_unregister(reg);
}

//=============================================================================
// Thread Safety Tests
// REGRESSION: Exception macros must be thread-safe
//=============================================================================

TEST_F(ExceptionMacroRegressionTest, ThreadLocalCurrentExceptionIsolation) {
    // REGRESSION: Thread-local current exception must be isolated per thread

    std::atomic<bool> thread1_done{false};
    std::atomic<bool> thread2_done{false};
    nimcp_exception_t* thread1_ex = nullptr;
    nimcp_exception_t* thread2_ex = nullptr;

    std::thread t1([&]() {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_UNKNOWN,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Thread 1 exception"
        );
        nimcp_exception_set_current(ex);

        // Wait for thread 2 to set its exception
        while (!thread2_done.load()) {
            std::this_thread::yield();
        }

        // Our current should still be our exception
        thread1_ex = nimcp_exception_get_current();
        nimcp_exception_clear_current();
        nimcp_exception_unref(ex);
        thread1_done.store(true);
    });

    std::thread t2([&]() {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_NO_MEMORY,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Thread 2 exception"
        );
        nimcp_exception_set_current(ex);
        thread2_ex = nimcp_exception_get_current();
        thread2_done.store(true);

        // Wait for thread 1 to complete
        while (!thread1_done.load()) {
            std::this_thread::yield();
        }

        nimcp_exception_clear_current();
        nimcp_exception_unref(ex);
    });

    t1.join();
    t2.join();

    // Each thread should have seen its own exception
    ASSERT_NE(thread1_ex, nullptr);
    ASSERT_NE(thread2_ex, nullptr);
    EXPECT_NE(thread1_ex, thread2_ex)
        << "Each thread must have its own current exception";
}

TEST_F(ExceptionMacroRegressionTest, ConcurrentExceptionThrowSafety) {
    // REGRESSION: Multiple threads can throw exceptions concurrently

    std::atomic<int> total_exceptions{0};

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "concurrent_test_handler";
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        auto* counter = static_cast<std::atomic<int>*>(user_data);
        (*counter)++;
        return true;
    };
    opts.user_data = &total_exceptions;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    const int num_threads = 4;
    const int exceptions_per_thread = 25;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([t, exceptions_per_thread]() {
            for (int i = 0; i < exceptions_per_thread; i++) {
                NIMCP_THROW(NIMCP_ERROR_UNKNOWN, "Thread %d, exception %d", t, i);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(total_exceptions.load(), num_threads * exceptions_per_thread)
        << "All exceptions from all threads must be handled";

    nimcp_handler_unregister(reg);
}

//=============================================================================
// Debug Assert Macro Tests
// REGRESSION: NIMCP_ASSERT_THROW behavior depends on NDEBUG
//=============================================================================

#ifndef NDEBUG
TEST_F(ExceptionMacroRegressionTest, AssertThrowInDebugMode) {
    // REGRESSION: NIMCP_ASSERT_THROW must throw in debug builds when condition fails

    static bool handler_called = false;

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "assert_test_handler";
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        handler_called = true;
        return true;
    };

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Should not throw when condition is true
    handler_called = false;
    NIMCP_ASSERT_THROW(true, NIMCP_ERROR_UNKNOWN, "Should not throw");
    EXPECT_FALSE(handler_called)
        << "NIMCP_ASSERT_THROW must NOT throw when condition is true (debug)";

    // Should throw when condition is false
    handler_called = false;
    NIMCP_ASSERT_THROW(false, NIMCP_ERROR_UNKNOWN, "Should throw");
    EXPECT_TRUE(handler_called)
        << "NIMCP_ASSERT_THROW must throw when condition is false (debug)";

    nimcp_handler_unregister(reg);
}
#else
TEST_F(ExceptionMacroRegressionTest, AssertThrowInReleaseMode) {
    // REGRESSION: NIMCP_ASSERT_THROW must be no-op in release builds

    static bool handler_called = false;

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "assert_test_handler";
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        handler_called = true;
        return true;
    };

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Should be no-op even with false condition in release
    handler_called = false;
    NIMCP_ASSERT_THROW(false, NIMCP_ERROR_UNKNOWN, "Should not throw in release");
    EXPECT_FALSE(handler_called)
        << "NIMCP_ASSERT_THROW must be no-op in release builds";

    nimcp_handler_unregister(reg);
}
#endif

//=============================================================================
// NIMCP_RECOVER Macro Tests
// REGRESSION: Recovery macro must work with current exception
//=============================================================================

TEST_F(ExceptionMacroRegressionTest, RecoverMacroUsesCurrentException) {
    // REGRESSION: NIMCP_RECOVER must operate on current thread-local exception

    static bool recovery_called = false;
    static nimcp_error_t recovered_code = NIMCP_SUCCESS;

    nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_RETRY,
        [](nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) -> int {
            recovery_called = true;
            if (ex) {
                recovered_code = ex->code;
            }
            return 0;
        },
        nullptr
    );

    // Create and set current exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_FILE_READ,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "File read test"
    );
    nimcp_exception_set_current(ex);

    // Use NIMCP_RECOVER
    recovery_called = false;
    NIMCP_RECOVER(EXCEPTION_RECOVERY_RETRY);

    EXPECT_TRUE(recovery_called)
        << "NIMCP_RECOVER must call recovery callback";
    EXPECT_EQ(recovered_code, NIMCP_ERROR_FILE_READ)
        << "Recovery must receive the current exception";

    nimcp_exception_clear_current();
    nimcp_exception_unref(ex);
    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_RETRY);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
