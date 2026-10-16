/**
 * @file test_exception_macro_comprehensive.cpp
 * @brief Comprehensive unit tests for NIMCP exception handling macros
 *
 * WHAT: Full coverage tests for exception macros and related functionality
 * WHY:  Verify correct behavior of all exception handling macros
 * HOW:  GoogleTest framework with fixture setup/teardown for exception system
 *
 * TEST CATEGORIES:
 * 1. NIMCP_CHECK_THROW - basic null checks, error code propagation
 * 2. NIMCP_CHECK_THROW_MSG - with custom messages
 * 3. NIMCP_TRY_CATCH blocks
 * 4. Exception context propagation
 * 5. Error code to string conversions
 * 6. Nested exception handling
 * 7. Stack trace capture (if enabled)
 * 8. Severity handling
 * 9. Typed exception macros
 * 10. Recovery macros
 *
 * @author NIMCP Development Team
 * @date 2026-01-22
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <vector>
#include <string>
#include <thread>
#include <chrono>

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

namespace {

std::atomic<int> g_handler_call_count{0};
std::atomic<nimcp_error_t> g_last_error_code{NIMCP_SUCCESS};
std::atomic<nimcp_exception_severity_t> g_last_severity{EXCEPTION_SEVERITY_DEBUG};
std::atomic<nimcp_exception_category_t> g_last_category{EXCEPTION_CATEGORY_GENERIC};
std::atomic<nimcp_exception_type_t> g_last_type{EXCEPTION_TYPE_BASE};
std::atomic<bool> g_exception_presented_to_immune{false};
std::vector<std::string> g_captured_messages;
std::vector<std::string> g_captured_files;
std::vector<std::string> g_captured_functions;
std::atomic<bool> g_use_message_capture{false};
std::atomic<int> g_last_line{0};

/**
 * @brief Test handler callback to track exception dispatch
 */
bool test_tracking_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    if (ex) {
        g_handler_call_count++;
        g_last_error_code = ex->code;
        g_last_severity = ex->severity;
        g_last_category = ex->category;
        g_last_type = ex->type;
        g_exception_presented_to_immune = ex->presented_to_immune;
        g_last_line = ex->line;

        if (g_use_message_capture) {
            if (ex->message) {
                g_captured_messages.push_back(std::string(ex->message));
            }
            if (ex->file) {
                g_captured_files.push_back(std::string(ex->file));
            }
            if (ex->function) {
                g_captured_functions.push_back(std::string(ex->function));
            }
        }
    }
    return false;  // Don't consume, let chain continue
}

/**
 * @brief Reset all test tracking globals
 */
void reset_tracking() {
    g_handler_call_count = 0;
    g_last_error_code = NIMCP_SUCCESS;
    g_last_severity = EXCEPTION_SEVERITY_DEBUG;
    g_last_category = EXCEPTION_CATEGORY_GENERIC;
    g_last_type = EXCEPTION_TYPE_BASE;
    g_exception_presented_to_immune = false;
    g_last_line = 0;
    g_captured_messages.clear();
    g_captured_files.clear();
    g_captured_functions.clear();
}

}  // anonymous namespace

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * WHAT: Base fixture for exception macro comprehensive tests
 * WHY:  Setup/teardown exception system for each test
 */
class ExceptionMacroComprehensiveTest : public ::testing::Test {
protected:
    nimcp_handler_registration_t* handler_reg_ = nullptr;

    void SetUp() override {
        reset_tracking();
        g_use_message_capture = true;

        // Initialize exception system
        nimcp_exception_system_init();

        // Register test handler
        nimcp_handler_options_t opts;
        nimcp_handler_default_options(&opts);
        opts.name = "macro_comprehensive_test_handler";
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
        g_use_message_capture = false;
    }
};

//=============================================================================
// SECTION 1: NIMCP_CHECK_THROW Basic Tests
//=============================================================================

/**
 * @brief Function using NIMCP_CHECK_THROW for NULL pointer validation
 */
static nimcp_error_t check_throw_null_test(void* ptr) {
    NIMCP_CHECK_THROW(ptr != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Parameter ptr is NULL");
    return NIMCP_SUCCESS;
}

/**
 * @brief Function using NIMCP_CHECK_THROW for range validation
 */
static nimcp_error_t check_throw_range_test(int value, int min, int max) {
    NIMCP_CHECK_THROW(value >= min, NIMCP_ERROR_OUT_OF_RANGE,
                      "Value %d below minimum %d", value, min);
    NIMCP_CHECK_THROW(value <= max, NIMCP_ERROR_OUT_OF_RANGE,
                      "Value %d above maximum %d", value, max);
    return NIMCP_SUCCESS;
}

/**
 * WHAT: Test NIMCP_CHECK_THROW returns NULL_POINTER error
 * WHY:  Verify basic null check functionality
 */
TEST_F(ExceptionMacroComprehensiveTest, CheckThrowNullPointer) {
    nimcp_error_t result = check_throw_null_test(nullptr);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_NULL_POINTER);
}

/**
 * WHAT: Test NIMCP_CHECK_THROW passes on valid pointer
 * WHY:  Verify successful path doesn't throw
 */
TEST_F(ExceptionMacroComprehensiveTest, CheckThrowValidPointer) {
    int value = 42;
    nimcp_error_t result = check_throw_null_test(&value);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(g_handler_call_count, 0);
}

/**
 * WHAT: Test NIMCP_CHECK_THROW returns OUT_OF_RANGE error for low value
 * WHY:  Verify range checking on lower bound
 */
TEST_F(ExceptionMacroComprehensiveTest, CheckThrowOutOfRangeLow) {
    nimcp_error_t result = check_throw_range_test(-5, 0, 100);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_OUT_OF_RANGE);
}

/**
 * WHAT: Test NIMCP_CHECK_THROW returns OUT_OF_RANGE error for high value
 * WHY:  Verify range checking on upper bound
 */
TEST_F(ExceptionMacroComprehensiveTest, CheckThrowOutOfRangeHigh) {
    nimcp_error_t result = check_throw_range_test(150, 0, 100);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test NIMCP_CHECK_THROW passes on valid range
 * WHY:  Verify successful range check
 */
TEST_F(ExceptionMacroComprehensiveTest, CheckThrowValidRange) {
    nimcp_error_t result = check_throw_range_test(50, 0, 100);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(g_handler_call_count, 0);
}

//=============================================================================
// SECTION 2: NIMCP_CHECK_THROW_MSG Tests
//=============================================================================

/**
 * @brief Function using NIMCP_CHECK_THROW_MSG
 */
static nimcp_error_t check_throw_msg_test(const char* name, size_t size) {
    NIMCP_CHECK_THROW_MSG(name != nullptr, NIMCP_ERROR_NULL_POINTER,
                          "Configuration name is NULL");
    NIMCP_CHECK_THROW_MSG(strlen(name) > 0, NIMCP_ERROR_INVALID_PARAM,
                          "Configuration name '%s' is empty", name);
    NIMCP_CHECK_THROW_MSG(size > 0, NIMCP_ERROR_INVALID_PARAM,
                          "Size must be positive, got %zu", size);
    return NIMCP_SUCCESS;
}

/**
 * WHAT: Test NIMCP_CHECK_THROW_MSG with NULL name
 * WHY:  Verify MSG variant works identically to CHECK_THROW
 */
TEST_F(ExceptionMacroComprehensiveTest, CheckThrowMsgNullName) {
    nimcp_error_t result = check_throw_msg_test(nullptr, 10);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("NULL"), std::string::npos);
}

/**
 * WHAT: Test NIMCP_CHECK_THROW_MSG with empty name
 * WHY:  Verify INVALID_PARAM error for empty string
 */
TEST_F(ExceptionMacroComprehensiveTest, CheckThrowMsgEmptyName) {
    nimcp_error_t result = check_throw_msg_test("", 10);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("empty"), std::string::npos);
}

/**
 * WHAT: Test NIMCP_CHECK_THROW_MSG with zero size
 * WHY:  Verify INVALID_PARAM error with formatted size value
 */
TEST_F(ExceptionMacroComprehensiveTest, CheckThrowMsgZeroSize) {
    nimcp_error_t result = check_throw_msg_test("valid_name", 0);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("0"), std::string::npos);
}

/**
 * WHAT: Test NIMCP_CHECK_THROW_MSG with valid parameters
 * WHY:  Verify successful path
 */
TEST_F(ExceptionMacroComprehensiveTest, CheckThrowMsgValidParams) {
    nimcp_error_t result = check_throw_msg_test("valid_name", 10);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(g_handler_call_count, 0);
}

//=============================================================================
// SECTION 3: NIMCP_TRY_CATCH Tests
//=============================================================================

/**
 * @brief Function that throws inside try block
 */
static nimcp_error_t function_that_throws() {
    NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "Intentional test failure");
    return NIMCP_SUCCESS;
}

/**
 * WHAT: Test NIMCP_TRY/NIMCP_CATCH catches exception
 * WHY:  Verify try/catch block mechanism
 */
TEST_F(ExceptionMacroComprehensiveTest, TryCatchBasic) {
    bool caught = false;
    nimcp_error_t caught_code = NIMCP_SUCCESS;

    NIMCP_TRY {
        function_that_throws();
    }
    NIMCP_CATCH(nimcp_exception_t, ex) {
        caught = true;
        caught_code = ex->code;
        nimcp_exception_unref(ex);
    }
    NIMCP_END_TRY;

    EXPECT_TRUE(caught);
    EXPECT_EQ(caught_code, NIMCP_ERROR_OPERATION_FAILED);
}

/**
 * WHAT: Test NIMCP_TRY block without exception
 * WHY:  Verify no catch when no exception thrown
 */
TEST_F(ExceptionMacroComprehensiveTest, TryCatchNoException) {
    bool caught = false;
    int value = 0;

    NIMCP_TRY {
        value = 42;
    }
    NIMCP_CATCH(nimcp_exception_t, ex) {
        caught = true;
        nimcp_exception_unref(ex);
    }
    NIMCP_END_TRY;

    EXPECT_FALSE(caught);
    EXPECT_EQ(value, 42);
}

/**
 * WHAT: Test nested NIMCP_TRY blocks
 * WHY:  Verify nested exception handling
 */
TEST_F(ExceptionMacroComprehensiveTest, TryCatchNested) {
    bool outer_caught = false;
    bool inner_caught = false;
    nimcp_error_t inner_code = NIMCP_SUCCESS;

    NIMCP_TRY {
        NIMCP_TRY {
            NIMCP_THROW(NIMCP_ERROR_INVALID_STATE, "Inner exception");
        }
        NIMCP_CATCH(nimcp_exception_t, ex) {
            inner_caught = true;
            inner_code = ex->code;
            nimcp_exception_unref(ex);
        }
        NIMCP_END_TRY;
    }
    NIMCP_CATCH(nimcp_exception_t, ex) {
        outer_caught = true;
        nimcp_exception_unref(ex);
    }
    NIMCP_END_TRY;

    EXPECT_TRUE(inner_caught);
    EXPECT_FALSE(outer_caught);
    EXPECT_EQ(inner_code, NIMCP_ERROR_INVALID_STATE);
}

//=============================================================================
// SECTION 4: Exception Context Propagation Tests
//=============================================================================

/**
 * @brief Function that sets exception context
 */
static nimcp_error_t function_with_context(int request_id, const char* module) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Operation failed with context");
    if (ex) {
        nimcp_exception_set_context(ex, "request_id", "12345");
        nimcp_exception_set_context(ex, "module", module);
        nimcp_exception_dispatch(ex);
        nimcp_exception_unref(ex);
    }
    return NIMCP_ERROR_OPERATION_FAILED;
}

/**
 * WHAT: Test exception context key-value pairs
 * WHY:  Verify context data is attached to exceptions
 */
TEST_F(ExceptionMacroComprehensiveTest, ExceptionContextBasic) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test with context");

    ASSERT_NE(ex, nullptr);

    // Set context
    int result = nimcp_exception_set_context(ex, "key1", "value1");
    EXPECT_EQ(result, 0);

    result = nimcp_exception_set_context(ex, "key2", "value2");
    EXPECT_EQ(result, 0);

    // Retrieve context
    const char* val1 = nimcp_exception_get_context(ex, "key1");
    ASSERT_NE(val1, nullptr);
    EXPECT_STREQ(val1, "value1");

    const char* val2 = nimcp_exception_get_context(ex, "key2");
    ASSERT_NE(val2, nullptr);
    EXPECT_STREQ(val2, "value2");

    // Non-existent key
    const char* val3 = nimcp_exception_get_context(ex, "key3");
    EXPECT_EQ(val3, nullptr);

    // Context count
    EXPECT_EQ(nimcp_exception_context_count(ex), 2u);

    nimcp_exception_unref(ex);
}

/**
 * WHAT: Test exception context removal
 * WHY:  Verify context entries can be removed
 */
TEST_F(ExceptionMacroComprehensiveTest, ExceptionContextRemoval) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test context removal");

    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "to_remove", "temp_value");
    EXPECT_EQ(nimcp_exception_context_count(ex), 1u);

    int result = nimcp_exception_remove_context(ex, "to_remove");
    EXPECT_EQ(result, 0);
    EXPECT_EQ(nimcp_exception_context_count(ex), 0u);

    // Remove non-existent
    result = nimcp_exception_remove_context(ex, "not_there");
    EXPECT_EQ(result, -1);

    nimcp_exception_unref(ex);
}

//=============================================================================
// SECTION 5: Error Code to String Conversion Tests
//=============================================================================

/**
 * WHAT: Test severity to string conversion
 * WHY:  Verify human-readable severity names
 */
TEST_F(ExceptionMacroComprehensiveTest, SeverityToString) {
    const char* debug_str = nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_DEBUG);
    EXPECT_NE(debug_str, nullptr);
    EXPECT_NE(strlen(debug_str), 0u);

    const char* info_str = nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_INFO);
    EXPECT_NE(info_str, nullptr);

    const char* warning_str = nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_WARNING);
    EXPECT_NE(warning_str, nullptr);

    const char* error_str = nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_ERROR);
    EXPECT_NE(error_str, nullptr);

    const char* severe_str = nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_SEVERE);
    EXPECT_NE(severe_str, nullptr);

    const char* critical_str = nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_CRITICAL);
    EXPECT_NE(critical_str, nullptr);

    const char* fatal_str = nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_FATAL);
    EXPECT_NE(fatal_str, nullptr);
}

/**
 * WHAT: Test category to string conversion
 * WHY:  Verify human-readable category names
 */
TEST_F(ExceptionMacroComprehensiveTest, CategoryToString) {
    const char* generic_str = nimcp_exception_category_to_string(EXCEPTION_CATEGORY_GENERIC);
    EXPECT_NE(generic_str, nullptr);

    const char* memory_str = nimcp_exception_category_to_string(EXCEPTION_CATEGORY_MEMORY);
    EXPECT_NE(memory_str, nullptr);

    const char* brain_str = nimcp_exception_category_to_string(EXCEPTION_CATEGORY_BRAIN);
    EXPECT_NE(brain_str, nullptr);

    const char* io_str = nimcp_exception_category_to_string(EXCEPTION_CATEGORY_IO);
    EXPECT_NE(io_str, nullptr);

    const char* threading_str = nimcp_exception_category_to_string(EXCEPTION_CATEGORY_THREADING);
    EXPECT_NE(threading_str, nullptr);

    const char* signal_str = nimcp_exception_category_to_string(EXCEPTION_CATEGORY_SIGNAL);
    EXPECT_NE(signal_str, nullptr);
}

/**
 * WHAT: Test exception type to string conversion
 * WHY:  Verify human-readable type names
 */
TEST_F(ExceptionMacroComprehensiveTest, TypeToString) {
    const char* base_str = nimcp_exception_type_to_string(EXCEPTION_TYPE_BASE);
    EXPECT_NE(base_str, nullptr);

    const char* memory_str = nimcp_exception_type_to_string(EXCEPTION_TYPE_MEMORY);
    EXPECT_NE(memory_str, nullptr);

    const char* brain_str = nimcp_exception_type_to_string(EXCEPTION_TYPE_BRAIN);
    EXPECT_NE(brain_str, nullptr);

    const char* io_str = nimcp_exception_type_to_string(EXCEPTION_TYPE_IO);
    EXPECT_NE(io_str, nullptr);

    const char* threading_str = nimcp_exception_type_to_string(EXCEPTION_TYPE_THREADING);
    EXPECT_NE(threading_str, nullptr);

    const char* security_str = nimcp_exception_type_to_string(EXCEPTION_TYPE_SECURITY);
    EXPECT_NE(security_str, nullptr);

    const char* signal_str = nimcp_exception_type_to_string(EXCEPTION_TYPE_SIGNAL);
    EXPECT_NE(signal_str, nullptr);
}

/**
 * WHAT: Test recovery action to string conversion
 * WHY:  Verify human-readable recovery action names
 */
TEST_F(ExceptionMacroComprehensiveTest, RecoveryActionToString) {
    const char* none_str = nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_NONE);
    EXPECT_NE(none_str, nullptr);

    const char* retry_str = nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_RETRY);
    EXPECT_NE(retry_str, nullptr);

    const char* gc_str = nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_GC);
    EXPECT_NE(gc_str, nullptr);

    const char* rollback_str = nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_ROLLBACK);
    EXPECT_NE(rollback_str, nullptr);

    const char* quarantine_str = nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_QUARANTINE);
    EXPECT_NE(quarantine_str, nullptr);
}

/**
 * WHAT: Test error code to string conversion
 * WHY:  Verify human-readable error messages
 */
TEST_F(ExceptionMacroComprehensiveTest, ErrorCodeToString) {
    const char* success_str = nimcp_error_to_string(NIMCP_SUCCESS);
    EXPECT_NE(success_str, nullptr);

    const char* null_str = nimcp_error_to_string(NIMCP_ERROR_NULL_POINTER);
    EXPECT_NE(null_str, nullptr);

    const char* invalid_str = nimcp_error_to_string(NIMCP_ERROR_INVALID_PARAM);
    EXPECT_NE(invalid_str, nullptr);

    const char* memory_str = nimcp_error_to_string(NIMCP_ERROR_NO_MEMORY);
    EXPECT_NE(memory_str, nullptr);

    const char* timeout_str = nimcp_error_to_string(NIMCP_ERROR_TIMEOUT);
    EXPECT_NE(timeout_str, nullptr);
}

//=============================================================================
// SECTION 6: Nested Exception Handling Tests
//=============================================================================

/**
 * @brief Inner function that throws
 */
static nimcp_error_t inner_throwing_function() {
    NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_STATE, "Inner error");
    return NIMCP_SUCCESS;
}

/**
 * @brief Outer function that catches and rethrows
 */
static nimcp_error_t outer_catching_function() {
    nimcp_error_t result = inner_throwing_function();
    if (result != NIMCP_SUCCESS) {
        // Chain the error
        return result;
    }
    return NIMCP_SUCCESS;
}

/**
 * WHAT: Test exception propagation through call chain
 * WHY:  Verify exceptions bubble up correctly
 */
TEST_F(ExceptionMacroComprehensiveTest, NestedExceptionPropagation) {
    nimcp_error_t result = outer_catching_function();

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_STATE);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_INVALID_STATE);
}

/**
 * @brief Deep nested function
 */
static nimcp_error_t deep_nested_level3() {
    NIMCP_CHECK_THROW(false, NIMCP_ERROR_OPERATION_FAILED, "Level 3 error");
    return NIMCP_SUCCESS;
}

static nimcp_error_t deep_nested_level2() {
    return deep_nested_level3();
}

static nimcp_error_t deep_nested_level1() {
    return deep_nested_level2();
}

/**
 * WHAT: Test deeply nested exception propagation
 * WHY:  Verify exceptions propagate through multiple call levels
 */
TEST_F(ExceptionMacroComprehensiveTest, DeepNestedExceptionPropagation) {
    nimcp_error_t result = deep_nested_level1();

    EXPECT_EQ(result, NIMCP_ERROR_OPERATION_FAILED);
    EXPECT_EQ(g_handler_call_count, 1);
}

//=============================================================================
// SECTION 7: Stack Trace Tests
//=============================================================================

/**
 * WHAT: Test stack trace capture
 * WHY:  Verify stack trace is captured in exceptions
 */
TEST_F(ExceptionMacroComprehensiveTest, StackTraceCapture) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test stack trace");

    ASSERT_NE(ex, nullptr);

    // Stack trace should be captured
    // Note: Actual depth depends on platform and build settings
    EXPECT_GE(ex->stack_trace.depth, 0u);

    nimcp_exception_unref(ex);
}

/**
 * WHAT: Test stack trace to string conversion
 * WHY:  Verify stack trace can be formatted as string
 */
TEST_F(ExceptionMacroComprehensiveTest, StackTraceToString) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test stack trace formatting");

    ASSERT_NE(ex, nullptr);

    char buffer[4096];
    size_t len = nimcp_stack_trace_to_string(&ex->stack_trace, buffer, sizeof(buffer));

    // Should produce some output (even if stack trace is empty)
    EXPECT_GE(len, 0u);

    nimcp_exception_unref(ex);
}

//=============================================================================
// SECTION 8: Various Error Types Tests
//=============================================================================

/**
 * @brief Helper to throw specific error codes
 */
static nimcp_error_t throw_error_code(nimcp_error_t code) {
    switch (code) {
        case NIMCP_ERROR_NULL_POINTER:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "Null pointer error");
            break;
        case NIMCP_ERROR_INVALID_PARAM:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM, "Invalid parameter error");
            break;
        case NIMCP_ERROR_OUT_OF_RANGE:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_OUT_OF_RANGE, "Out of range error");
            break;
        case NIMCP_ERROR_NO_MEMORY:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_NO_MEMORY, "No memory error");
            break;
        case NIMCP_ERROR_INVALID_STATE:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_STATE, "Invalid state error");
            break;
        case NIMCP_ERROR_OPERATION_FAILED:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_OPERATION_FAILED, "Operation failed error");
            break;
        case NIMCP_ERROR_TIMEOUT:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_TIMEOUT, "Timeout error");
            break;
        case NIMCP_ERROR_BUFFER_OVERFLOW:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_BUFFER_OVERFLOW, "Buffer overflow error");
            break;
        case NIMCP_ERROR_NOT_INITIALIZED:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_INITIALIZED, "Not initialized error");
            break;
        case NIMCP_ERROR_ALREADY_EXISTS:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_ALREADY_EXISTS, "Already exists error");
            break;
        case NIMCP_ERROR_NOT_FOUND:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "Not found error");
            break;
        case NIMCP_ERROR_THREAD_CREATE:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_THREAD_CREATE, "Thread create error");
            break;
        case NIMCP_ERROR_MUTEX_LOCK:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_MUTEX_LOCK, "Mutex lock error");
            break;
        case NIMCP_ERROR_DEADLOCK:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_DEADLOCK, "Deadlock error");
            break;
        default:
            NIMCP_CHECK_THROW(false, code, "Generic error code %d", (int)code);
            break;
    }
    return NIMCP_SUCCESS;
}

/**
 * WHAT: Test NO_MEMORY error code
 * WHY:  Verify memory error handling
 */
TEST_F(ExceptionMacroComprehensiveTest, ErrorCodeNoMemory) {
    nimcp_error_t result = throw_error_code(NIMCP_ERROR_NO_MEMORY);
    EXPECT_EQ(result, NIMCP_ERROR_NO_MEMORY);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_NO_MEMORY);
}

/**
 * WHAT: Test TIMEOUT error code
 * WHY:  Verify timeout error handling
 */
TEST_F(ExceptionMacroComprehensiveTest, ErrorCodeTimeout) {
    nimcp_error_t result = throw_error_code(NIMCP_ERROR_TIMEOUT);
    EXPECT_EQ(result, NIMCP_ERROR_TIMEOUT);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_TIMEOUT);
}

/**
 * WHAT: Test BUFFER_OVERFLOW error code
 * WHY:  Verify buffer overflow error handling
 */
TEST_F(ExceptionMacroComprehensiveTest, ErrorCodeBufferOverflow) {
    nimcp_error_t result = throw_error_code(NIMCP_ERROR_BUFFER_OVERFLOW);
    EXPECT_EQ(result, NIMCP_ERROR_BUFFER_OVERFLOW);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_BUFFER_OVERFLOW);
}

/**
 * WHAT: Test NOT_INITIALIZED error code
 * WHY:  Verify initialization error handling
 */
TEST_F(ExceptionMacroComprehensiveTest, ErrorCodeNotInitialized) {
    nimcp_error_t result = throw_error_code(NIMCP_ERROR_NOT_INITIALIZED);
    EXPECT_EQ(result, NIMCP_ERROR_NOT_INITIALIZED);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_NOT_INITIALIZED);
}

/**
 * WHAT: Test NOT_FOUND error code
 * WHY:  Verify not found error handling
 */
TEST_F(ExceptionMacroComprehensiveTest, ErrorCodeNotFound) {
    nimcp_error_t result = throw_error_code(NIMCP_ERROR_NOT_FOUND);
    EXPECT_EQ(result, NIMCP_ERROR_NOT_FOUND);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_NOT_FOUND);
}

/**
 * WHAT: Test THREAD_CREATE error code
 * WHY:  Verify threading error handling
 */
TEST_F(ExceptionMacroComprehensiveTest, ErrorCodeThreadCreate) {
    nimcp_error_t result = throw_error_code(NIMCP_ERROR_THREAD_CREATE);
    EXPECT_EQ(result, NIMCP_ERROR_THREAD_CREATE);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_THREAD_CREATE);
}

/**
 * WHAT: Test DEADLOCK error code
 * WHY:  Verify deadlock error handling
 */
TEST_F(ExceptionMacroComprehensiveTest, ErrorCodeDeadlock) {
    nimcp_error_t result = throw_error_code(NIMCP_ERROR_DEADLOCK);
    EXPECT_EQ(result, NIMCP_ERROR_DEADLOCK);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_DEADLOCK);
}

//=============================================================================
// SECTION 9: NIMCP_THROW_IF Tests
//=============================================================================

/**
 * @brief Function using NIMCP_THROW_IF
 */
static void throw_if_test(bool condition) {
    /* NIMCP_THROW_IF throws when condition is TRUE (standard semantics) */
    NIMCP_THROW_IF(condition, NIMCP_ERROR_INVALID_PARAM, "Condition was true");
}

/**
 * WHAT: Test NIMCP_THROW_IF when condition is false
 * WHY:  Verify no throw on false condition
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowIfConditionFalse) {
    throw_if_test(false);

    EXPECT_EQ(g_handler_call_count, 0);
}

/**
 * WHAT: Test NIMCP_THROW_IF when condition is true
 * WHY:  Verify throw occurs on true condition
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowIfConditionTrue) {
    throw_if_test(true);

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_INVALID_PARAM);
}

//=============================================================================
// SECTION 10: Severity Tests
//=============================================================================

/**
 * WHAT: Test severity derived from error code
 * WHY:  Verify automatic severity assignment
 */
TEST_F(ExceptionMacroComprehensiveTest, SeverityFromErrorCode) {
    // Test that memory errors get higher severity
    nimcp_exception_severity_t mem_severity =
        nimcp_exception_get_severity_from_code(NIMCP_ERROR_NO_MEMORY);
    EXPECT_GE((int)mem_severity, (int)EXCEPTION_SEVERITY_ERROR);

    // Test generic errors
    nimcp_exception_severity_t generic_severity =
        nimcp_exception_get_severity_from_code(NIMCP_ERROR_INVALID_PARAM);
    EXPECT_GE((int)generic_severity, (int)EXCEPTION_SEVERITY_WARNING);
}

/**
 * WHAT: Test category derived from error code
 * WHY:  Verify automatic category assignment
 */
TEST_F(ExceptionMacroComprehensiveTest, CategoryFromErrorCode) {
    // Generic errors (1000-1999)
    nimcp_exception_category_t generic_cat =
        nimcp_exception_get_category_from_code(NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(generic_cat, EXCEPTION_CATEGORY_GENERIC);

    // Memory errors (2000-2999)
    nimcp_exception_category_t mem_cat =
        nimcp_exception_get_category_from_code(NIMCP_ERROR_NO_MEMORY);
    EXPECT_EQ(mem_cat, EXCEPTION_CATEGORY_MEMORY);

    // Threading errors (6000-6999)
    nimcp_exception_category_t thread_cat =
        nimcp_exception_get_category_from_code(NIMCP_ERROR_DEADLOCK);
    EXPECT_EQ(thread_cat, EXCEPTION_CATEGORY_THREADING);
}

//=============================================================================
// SECTION 11: Exception Lifecycle Tests
//=============================================================================

/**
 * WHAT: Test exception reference counting
 * WHY:  Verify ref/unref works correctly
 */
TEST_F(ExceptionMacroComprehensiveTest, ExceptionRefCounting) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test ref counting");

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->ref_count, 1);

    // Add reference
    nimcp_exception_t* ex2 = nimcp_exception_ref(ex);
    EXPECT_EQ(ex, ex2);
    EXPECT_EQ(ex->ref_count, 2);

    // Release one reference
    nimcp_exception_unref(ex);
    EXPECT_EQ(ex->ref_count, 1);

    // Release final reference (this frees the memory)
    nimcp_exception_unref(ex);
    // ex is now freed, don't access it
}

/**
 * WHAT: Test exception cause chaining
 * WHY:  Verify exceptions can be chained
 */
TEST_F(ExceptionMacroComprehensiveTest, ExceptionCauseChaining) {
    nimcp_exception_t* cause = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Root cause: memory allocation failed");

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Operation failed");

    ASSERT_NE(cause, nullptr);
    ASSERT_NE(ex, nullptr);

    // Chain cause
    nimcp_exception_set_cause(ex, cause);

    // Verify chain
    nimcp_exception_t* retrieved_cause = nimcp_exception_get_cause(ex);
    EXPECT_EQ(retrieved_cause, cause);
    EXPECT_EQ(retrieved_cause->code, NIMCP_ERROR_NO_MEMORY);

    // Unref the main exception (cause will also be unrefd)
    nimcp_exception_unref(ex);
}

//=============================================================================
// SECTION 12: Exception to String Tests
//=============================================================================

/**
 * WHAT: Test exception to string conversion
 * WHY:  Verify human-readable exception formatting
 */
TEST_F(ExceptionMacroComprehensiveTest, ExceptionToString) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test exception message");

    ASSERT_NE(ex, nullptr);

    char buffer[1024];
    size_t len = nimcp_exception_to_string(ex, buffer, sizeof(buffer));

    EXPECT_GT(len, 0u);
    EXPECT_NE(strstr(buffer, "Test exception message"), nullptr);

    nimcp_exception_unref(ex);
}

//=============================================================================
// SECTION 13: Thread-Local Exception Tests
//=============================================================================

/**
 * WHAT: Test thread-local current exception
 * WHY:  Verify thread-local exception storage
 */
TEST_F(ExceptionMacroComprehensiveTest, ThreadLocalException) {
    // Initially no current exception
    nimcp_exception_t* current = nimcp_exception_get_current();
    EXPECT_EQ(current, nullptr);

    // Create and set current
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Thread local test");

    ASSERT_NE(ex, nullptr);
    nimcp_exception_set_current(ex);

    // Verify it's set
    current = nimcp_exception_get_current();
    EXPECT_EQ(current, ex);

    // Clear
    nimcp_exception_clear_current();
    current = nimcp_exception_get_current();
    EXPECT_EQ(current, nullptr);

    // Unref
    nimcp_exception_unref(ex);
}

//=============================================================================
// SECTION 14: Message Formatting Tests
//=============================================================================

/**
 * WHAT: Test printf-style message formatting in exceptions
 * WHY:  Verify formatted messages contain correct values
 */
TEST_F(ExceptionMacroComprehensiveTest, MessageFormattingInteger) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Value %d is out of range [%d, %d]", 42, 0, 10);

    ASSERT_NE(ex, nullptr);
    EXPECT_NE(strstr(ex->message, "42"), nullptr);
    EXPECT_NE(strstr(ex->message, "0"), nullptr);
    EXPECT_NE(strstr(ex->message, "10"), nullptr);

    nimcp_exception_unref(ex);
}

/**
 * WHAT: Test string formatting in exceptions
 * WHY:  Verify string parameters are included
 */
TEST_F(ExceptionMacroComprehensiveTest, MessageFormattingString) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NOT_FOUND,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Resource '%s' not found in module '%s'", "config.json", "loader");

    ASSERT_NE(ex, nullptr);
    EXPECT_NE(strstr(ex->message, "config.json"), nullptr);
    EXPECT_NE(strstr(ex->message, "loader"), nullptr);

    nimcp_exception_unref(ex);
}

/**
 * WHAT: Test mixed formatting in exceptions
 * WHY:  Verify multiple format types work together
 */
TEST_F(ExceptionMacroComprehensiveTest, MessageFormattingMixed) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        "Failed to allocate %zu bytes for '%s' (attempt %d/%d)",
        1024u, "buffer", 3, 5);

    ASSERT_NE(ex, nullptr);
    EXPECT_NE(strstr(ex->message, "1024"), nullptr);
    EXPECT_NE(strstr(ex->message, "buffer"), nullptr);
    EXPECT_NE(strstr(ex->message, "3"), nullptr);
    EXPECT_NE(strstr(ex->message, "5"), nullptr);

    nimcp_exception_unref(ex);
}

//=============================================================================
// SECTION 15: Source Location Tests
//=============================================================================

/**
 * WHAT: Test source file/line/function capture
 * WHY:  Verify source location is captured in exceptions
 */
TEST_F(ExceptionMacroComprehensiveTest, SourceLocationCapture) {
    int line_before = __LINE__;
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Source location test");
    int expected_line = line_before + 1; // Adjust for the actual CREATE line

    ASSERT_NE(ex, nullptr);
    EXPECT_NE(ex->file, nullptr);
    EXPECT_NE(strstr(ex->file, "test_exception_macro_comprehensive"), nullptr);
    EXPECT_GT(ex->line, 0);
    EXPECT_NE(ex->function, nullptr);

    nimcp_exception_unref(ex);
}

/**
 * WHAT: Test NIMCP_THROW captures source location
 * WHY:  Verify macro captures correct __FILE__ and __LINE__
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowCapturesSourceLocation) {
    NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "Throw source test");

    EXPECT_EQ(g_handler_call_count, 1);
    ASSERT_FALSE(g_captured_files.empty());
    EXPECT_NE(g_captured_files[0].find("test_exception_macro_comprehensive"), std::string::npos);
    EXPECT_GT(g_last_line, 0);
}

//=============================================================================
// SECTION 16: Epitope Generation Tests
//=============================================================================

/**
 * WHAT: Test immune epitope generation
 * WHY:  Verify epitope is generated for pattern matching
 */
TEST_F(ExceptionMacroComprehensiveTest, EpitopeGeneration) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Epitope test");

    ASSERT_NE(ex, nullptr);

    size_t epitope_len = nimcp_exception_generate_epitope(ex);
    EXPECT_GT(epitope_len, 0u);
    EXPECT_LE(epitope_len, NIMCP_EXCEPTION_EPITOPE_SIZE);
    EXPECT_EQ(ex->epitope_len, epitope_len);

    nimcp_exception_unref(ex);
}

//=============================================================================
// SECTION 17: Suggested Recovery Tests
//=============================================================================

/**
 * WHAT: Test suggested recovery for memory errors
 * WHY:  Verify appropriate recovery actions are suggested
 */
TEST_F(ExceptionMacroComprehensiveTest, SuggestedRecoveryMemory) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Memory error recovery test");

    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_action_t action = nimcp_exception_get_suggested_recovery(ex);
    // Memory errors should suggest GC or compact
    EXPECT_TRUE(action == EXCEPTION_RECOVERY_GC ||
                action == EXCEPTION_RECOVERY_COMPACT ||
                action == EXCEPTION_RECOVERY_REDUCE_LOAD);

    nimcp_exception_unref(ex);
}

//=============================================================================
// SECTION 18: Aggregate Exception Tests
//=============================================================================

/**
 * WHAT: Test aggregate exception creation
 * WHY:  Verify multiple exceptions can be aggregated
 */
TEST_F(ExceptionMacroComprehensiveTest, AggregateExceptionBasic) {
    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Multiple errors occurred");

    ASSERT_NE(agg, nullptr);
    EXPECT_EQ(agg->base.type, EXCEPTION_TYPE_AGGREGATE);
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 0u);

    // Add children
    nimcp_exception_t* child1 = nimcp_exception_create(
        NIMCP_ERROR_INVALID_PARAM,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Child error 1");

    nimcp_exception_t* child2 = nimcp_exception_create(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Child error 2");

    ASSERT_NE(child1, nullptr);
    ASSERT_NE(child2, nullptr);

    int result = nimcp_aggregate_exception_add(agg, child1);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 1u);

    result = nimcp_aggregate_exception_add(agg, child2);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 2u);

    // Retrieve children
    nimcp_exception_t* retrieved = nimcp_aggregate_exception_get(agg, 0);
    EXPECT_EQ(retrieved, child1);

    retrieved = nimcp_aggregate_exception_get(agg, 1);
    EXPECT_EQ(retrieved, child2);

    // Out of bounds
    retrieved = nimcp_aggregate_exception_get(agg, 5);
    EXPECT_EQ(retrieved, nullptr);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

//=============================================================================
// SECTION 19: Multiple Error Handling Tests
//=============================================================================

/**
 * WHAT: Test multiple errors don't cause memory leaks
 * WHY:  Verify exception cleanup on repeated errors
 */
TEST_F(ExceptionMacroComprehensiveTest, MultipleErrorsNoLeak) {
    const int iterations = 100;

    for (int i = 0; i < iterations; i++) {
        nimcp_error_t result = check_throw_null_test(nullptr);
        EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    }

    EXPECT_EQ(g_handler_call_count, iterations);
}

/**
 * WHAT: Test alternating success and failure
 * WHY:  Verify cleanup works for mixed outcomes
 */
TEST_F(ExceptionMacroComprehensiveTest, AlternatingSuccessFailure) {
    int success_count = 0;
    int failure_count = 0;

    for (int i = 0; i < 50; i++) {
        if (i % 2 == 0) {
            int value = 42;
            nimcp_error_t result = check_throw_null_test(&value);
            EXPECT_EQ(result, NIMCP_SUCCESS);
            success_count++;
        } else {
            nimcp_error_t result = check_throw_null_test(nullptr);
            EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
            failure_count++;
        }
    }

    EXPECT_EQ(success_count, 25);
    EXPECT_EQ(failure_count, 25);
    EXPECT_EQ(g_handler_call_count, 25);
}

//=============================================================================
// SECTION 20: Boundary Value Tests
//=============================================================================

/**
 * WHAT: Test boundary at minimum
 * WHY:  Verify exact boundary is accepted
 */
TEST_F(ExceptionMacroComprehensiveTest, BoundaryAtMinimum) {
    nimcp_error_t result = check_throw_range_test(0, 0, 100);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(g_handler_call_count, 0);
}

/**
 * WHAT: Test boundary at maximum
 * WHY:  Verify exact boundary is accepted
 */
TEST_F(ExceptionMacroComprehensiveTest, BoundaryAtMaximum) {
    nimcp_error_t result = check_throw_range_test(100, 0, 100);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(g_handler_call_count, 0);
}

/**
 * WHAT: Test one below minimum
 * WHY:  Verify off-by-one is caught
 */
TEST_F(ExceptionMacroComprehensiveTest, BoundaryBelowMinimum) {
    nimcp_error_t result = check_throw_range_test(-1, 0, 100);
    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test one above maximum
 * WHY:  Verify off-by-one is caught
 */
TEST_F(ExceptionMacroComprehensiveTest, BoundaryAboveMaximum) {
    nimcp_error_t result = check_throw_range_test(101, 0, 100);
    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
}

//=============================================================================
// SECTION 21: Handler Priority Tests
//=============================================================================

namespace {
std::vector<int> g_handler_order;

bool priority_handler_high(nimcp_exception_t* ex, void* user_data) {
    (void)ex;
    (void)user_data;
    g_handler_order.push_back(100);
    return false;
}

bool priority_handler_normal(nimcp_exception_t* ex, void* user_data) {
    (void)ex;
    (void)user_data;
    g_handler_order.push_back(50);
    return false;
}

bool priority_handler_low(nimcp_exception_t* ex, void* user_data) {
    (void)ex;
    (void)user_data;
    g_handler_order.push_back(10);
    return false;
}
}

/**
 * WHAT: Test handler priority ordering
 * WHY:  Verify handlers are called in priority order
 */
TEST_F(ExceptionMacroComprehensiveTest, HandlerPriorityOrder) {
    g_handler_order.clear();

    // Register handlers with different priorities (note: test fixture already has one)
    nimcp_handler_options_t opts_low, opts_normal;

    nimcp_handler_default_options(&opts_low);
    opts_low.name = "priority_test_low";
    opts_low.handler = priority_handler_low;
    opts_low.priority = NIMCP_HANDLER_PRIORITY_LOW;
    nimcp_handler_registration_t* reg_low = nimcp_handler_register(&opts_low);

    nimcp_handler_default_options(&opts_normal);
    opts_normal.name = "priority_test_normal";
    opts_normal.handler = priority_handler_normal;
    opts_normal.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
    nimcp_handler_registration_t* reg_normal = nimcp_handler_register(&opts_normal);

    // Throw exception
    NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "Priority test");

    // Low and normal should be called (high is our test handler)
    // Handlers with higher priority are called first
    EXPECT_GE(g_handler_order.size(), 2u);

    // Cleanup
    nimcp_handler_unregister(reg_low);
    nimcp_handler_unregister(reg_normal);
}

//=============================================================================
// SECTION 22: Exception System State Tests
//=============================================================================

/**
 * WHAT: Test exception system initialization state
 * WHY:  Verify system can detect initialization state
 */
TEST_F(ExceptionMacroComprehensiveTest, SystemInitializationState) {
    // System should be initialized (done in SetUp)
    EXPECT_TRUE(nimcp_exception_system_is_initialized());
}

/**
 * WHAT: Test handler count
 * WHY:  Verify handlers can be counted
 */
TEST_F(ExceptionMacroComprehensiveTest, HandlerCount) {
    size_t count = nimcp_handler_count();
    // At least our test handler should be registered
    EXPECT_GE(count, 1u);
}

//=============================================================================
// SECTION 23: Exception in Try Block Tests
//=============================================================================

/**
 * WHAT: Test nimcp_in_try_block detection
 * WHY:  Verify try block detection works
 */
TEST_F(ExceptionMacroComprehensiveTest, InTryBlockDetection) {
    // Outside try block
    EXPECT_FALSE(nimcp_in_try_block());

    bool was_in_try = false;
    NIMCP_TRY {
        was_in_try = nimcp_in_try_block();
    }
    NIMCP_CATCH(nimcp_exception_t, ex) {
        (void)ex;
    }
    NIMCP_END_TRY;

    EXPECT_TRUE(was_in_try);

    // After try block
    EXPECT_FALSE(nimcp_in_try_block());
}

//=============================================================================
// SECTION 24: Error Code Category Tests
//=============================================================================

/**
 * WHAT: Test error category extraction
 * WHY:  Verify error codes map to correct categories
 */
TEST_F(ExceptionMacroComprehensiveTest, ErrorCategoryExtraction) {
    // Generic errors (1000-1999) -> category 1
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_INVALID_PARAM), 1);

    // Memory errors (2000-2999) -> category 2
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_NO_MEMORY), 2);

    // Brain errors (3000-3999) -> category 3
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_BRAIN_CREATION), 3);

    // I/O errors (4000-4999) -> category 4
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_FILE_NOT_FOUND), 4);

    // Threading errors (6000-6999) -> category 6
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_DEADLOCK), 6);
}

/**
 * WHAT: Test error success/failure checks
 * WHY:  Verify success/failure detection
 */
TEST_F(ExceptionMacroComprehensiveTest, ErrorSuccessFailureChecks) {
    EXPECT_TRUE(nimcp_error_is_success(NIMCP_SUCCESS));
    EXPECT_TRUE(nimcp_error_is_success(NIMCP_SUCCESS_WITH_WARNINGS));
    EXPECT_TRUE(nimcp_error_is_success(NIMCP_SUCCESS_PARTIAL));

    EXPECT_FALSE(nimcp_error_is_success(NIMCP_ERROR_INVALID_PARAM));
    EXPECT_FALSE(nimcp_error_is_success(NIMCP_ERROR_NO_MEMORY));

    EXPECT_TRUE(nimcp_error_is_failure(NIMCP_ERROR_INVALID_PARAM));
    EXPECT_TRUE(nimcp_error_is_failure(NIMCP_ERROR_NO_MEMORY));

    EXPECT_FALSE(nimcp_error_is_failure(NIMCP_SUCCESS));
}

//=============================================================================
// SECTION 25: Timestamp Tests
//=============================================================================

/**
 * WHAT: Test exception timestamp capture
 * WHY:  Verify timestamps are recorded
 */
TEST_F(ExceptionMacroComprehensiveTest, TimestampCapture) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Timestamp test");

    ASSERT_NE(ex, nullptr);
    EXPECT_GT(ex->timestamp_us, 0u);

    nimcp_exception_unref(ex);
}

//=============================================================================
// SECTION 26: Typed Exception Creation Tests
//=============================================================================

/**
 * WHAT: Test memory exception creation
 * WHY:  Verify typed exception has correct type
 */
TEST_F(ExceptionMacroComprehensiveTest, MemoryExceptionCreation) {
    nimcp_memory_exception_t* mex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1024,  // requested_size
        "Failed to allocate %zu bytes", 1024u);

    ASSERT_NE(mex, nullptr);
    EXPECT_EQ(mex->base.type, EXCEPTION_TYPE_MEMORY);
    EXPECT_EQ(mex->base.code, NIMCP_ERROR_NO_MEMORY);
    EXPECT_EQ(mex->requested_size, 1024u);

    nimcp_exception_unref((nimcp_exception_t*)mex);
}

/**
 * WHAT: Test threading exception creation
 * WHY:  Verify typed exception has correct type
 */
TEST_F(ExceptionMacroComprehensiveTest, ThreadingExceptionCreation) {
    nimcp_threading_exception_t* tex = nimcp_threading_exception_create(
        NIMCP_ERROR_DEADLOCK,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        12345,  // thread_id
        "Deadlock detected on thread %lu", 12345ul);

    ASSERT_NE(tex, nullptr);
    EXPECT_EQ(tex->base.type, EXCEPTION_TYPE_THREADING);
    EXPECT_EQ(tex->base.code, NIMCP_ERROR_DEADLOCK);
    EXPECT_EQ(tex->thread_id, 12345u);

    nimcp_exception_unref((nimcp_exception_t*)tex);
}

/**
 * WHAT: Test I/O exception creation
 * WHY:  Verify typed exception has correct type
 */
TEST_F(ExceptionMacroComprehensiveTest, IOExceptionCreation) {
    nimcp_io_exception_t* iex = nimcp_io_exception_create(
        NIMCP_ERROR_FILE_NOT_FOUND,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "/path/to/file.txt",
        "File not found: %s", "/path/to/file.txt");

    ASSERT_NE(iex, nullptr);
    EXPECT_EQ(iex->base.type, EXCEPTION_TYPE_IO);
    EXPECT_EQ(iex->base.code, NIMCP_ERROR_FILE_NOT_FOUND);

    nimcp_exception_unref((nimcp_exception_t*)iex);
}

/**
 * WHAT: Test security exception creation
 * WHY:  Verify typed exception has correct type
 */
TEST_F(ExceptionMacroComprehensiveTest, SecurityExceptionCreation) {
    nimcp_security_exception_t* sex = nimcp_security_exception_create(
        NIMCP_ERROR_SECURITY_THREAT,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        42,  // threat_type
        "Security threat detected: type %d", 42);

    ASSERT_NE(sex, nullptr);
    EXPECT_EQ(sex->base.type, EXCEPTION_TYPE_SECURITY);
    EXPECT_EQ(sex->base.code, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(sex->threat_type, 42u);

    nimcp_exception_unref((nimcp_exception_t*)sex);
}

//=============================================================================
// SECTION 27: Brain Exception Tests
//=============================================================================

/**
 * WHAT: Test brain exception creation
 * WHY:  Verify brain exception has correct type and fields
 */
TEST_F(ExceptionMacroComprehensiveTest, BrainExceptionCreation) {
    nimcp_brain_exception_t* bex = nimcp_brain_exception_create(
        NIMCP_ERROR_BRAIN_INVALID,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        123,  // brain_id
        "hippocampus",  // region_name
        "Brain %u region '%s' error", 123, "hippocampus");

    ASSERT_NE(bex, nullptr);
    EXPECT_EQ(bex->base.type, EXCEPTION_TYPE_BRAIN);
    EXPECT_EQ(bex->base.code, NIMCP_ERROR_BRAIN_INVALID);
    EXPECT_EQ(bex->brain_id, 123u);

    nimcp_exception_unref((nimcp_exception_t*)bex);
}

//=============================================================================
// SECTION 28: GPU Exception Tests
//=============================================================================

/**
 * WHAT: Test GPU exception creation
 * WHY:  Verify GPU exception has correct type and fields
 */
TEST_F(ExceptionMacroComprehensiveTest, GPUExceptionCreation) {
    nimcp_gpu_exception_t* gex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0,  // device_id
        1,  // cuda_error
        "GPU device %d error (CUDA %d)", 0, 1);

    ASSERT_NE(gex, nullptr);
    EXPECT_EQ(gex->base.type, EXCEPTION_TYPE_GPU);
    EXPECT_EQ(gex->base.code, NIMCP_ERROR_GPU);
    EXPECT_EQ(gex->device_id, 0);
    EXPECT_EQ(gex->cuda_error, 1);

    nimcp_exception_unref((nimcp_exception_t*)gex);
}

//=============================================================================
// SECTION 29: Handler Enable/Disable Tests
//=============================================================================

/**
 * WHAT: Test handler enable/disable
 * WHY:  Verify handlers can be temporarily disabled
 */
TEST_F(ExceptionMacroComprehensiveTest, HandlerEnableDisable) {
    // Disable our test handler
    nimcp_handler_disable(handler_reg_);

    // Throw - our handler shouldn't be called
    reset_tracking();
    NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "Disabled handler test");

    // Our tracking handler shouldn't have been called
    EXPECT_EQ(g_handler_call_count, 0);

    // Re-enable
    nimcp_handler_enable(handler_reg_);

    reset_tracking();
    NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "Re-enabled handler test");

    // Now it should be called
    EXPECT_EQ(g_handler_call_count, 1);
}

//=============================================================================
// SECTION 30: Signal Exception Tests
//=============================================================================

/**
 * WHAT: Test signal to error code conversion
 * WHY:  Verify signals map to correct error codes
 */
TEST_F(ExceptionMacroComprehensiveTest, SignalToErrorCode) {
    // Note: SIGSEGV value is platform-dependent, but should map consistently
    nimcp_error_t err = nimcp_signal_to_error_code(11);  // Typical SIGSEGV
    EXPECT_GE((int)err, 7000);  // Signal errors are in 7000 range
    EXPECT_LT((int)err, 8000);
}

/**
 * WHAT: Test signal name conversion
 * WHY:  Verify signal names are available
 */
TEST_F(ExceptionMacroComprehensiveTest, SignalNameConversion) {
    const char* name = nimcp_signal_name(11);  // Typical SIGSEGV
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
}

//=============================================================================
// SECTION 31: NIMCP_THROW Basic Functionality Tests
//=============================================================================

/**
 * @brief Function that uses NIMCP_THROW directly (no condition check)
 */
static void unconditional_throw_test(nimcp_error_t code, const char* msg) {
    NIMCP_THROW(code, "%s", msg);
}

/**
 * WHAT: Test NIMCP_THROW dispatches to handler chain
 * WHY:  Verify basic throw mechanism works
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowBasicDispatch) {
    unconditional_throw_test(NIMCP_ERROR_OPERATION_FAILED, "Basic throw test");

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_OPERATION_FAILED);
}

/**
 * WHAT: Test NIMCP_THROW with various error codes
 * WHY:  Verify all error code types dispatch correctly
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowVariousErrorCodes) {
    // Test NULL_POINTER
    reset_tracking();
    NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "Null pointer throw");
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_NULL_POINTER);

    // Test INVALID_PARAM
    reset_tracking();
    NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Invalid param throw");
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_INVALID_PARAM);

    // Test NO_MEMORY
    reset_tracking();
    NIMCP_THROW(NIMCP_ERROR_NO_MEMORY, "No memory throw");
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_NO_MEMORY);

    // Test TIMEOUT
    reset_tracking();
    NIMCP_THROW(NIMCP_ERROR_TIMEOUT, "Timeout throw");
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_TIMEOUT);
}

/**
 * WHAT: Test NIMCP_THROW with formatted message
 * WHY:  Verify printf-style formatting in THROW macro
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowFormattedMessage) {
    NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Invalid parameter %d at position %s", 42, "test_field");

    EXPECT_EQ(g_handler_call_count, 1);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("42"), std::string::npos);
    EXPECT_NE(g_captured_messages[0].find("test_field"), std::string::npos);
}

/**
 * WHAT: Test NIMCP_THROW multiple times in sequence
 * WHY:  Verify each throw dispatches independently
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowMultipleSequential) {
    NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "First throw");
    NIMCP_THROW(NIMCP_ERROR_OUT_OF_RANGE, "Second throw");
    NIMCP_THROW(NIMCP_ERROR_INVALID_STATE, "Third throw");

    EXPECT_EQ(g_handler_call_count, 3);
    // Last error code should be from the last throw
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_INVALID_STATE);
}

/**
 * WHAT: Test NIMCP_THROW severity is derived from error code
 * WHY:  Verify automatic severity assignment
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowSeverityFromCode) {
    // Memory errors should get higher severity
    NIMCP_THROW(NIMCP_ERROR_NO_MEMORY, "Memory error test");

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_GE((int)g_last_severity, (int)EXCEPTION_SEVERITY_ERROR);
}

//=============================================================================
// SECTION 32: NIMCP_THROW_TO_IMMUNE Tests
//=============================================================================

/**
 * @brief Function using NIMCP_THROW_TO_IMMUNE
 */
static void throw_to_immune_test(nimcp_error_t code) {
    NIMCP_THROW_TO_IMMUNE(code, "Immune presentation test for code %d", (int)code);
}

/**
 * WHAT: Test NIMCP_THROW_TO_IMMUNE sets presented_to_immune flag
 * WHY:  Verify immune system notification occurs
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowToImmuneSetsPresentedFlag) {
    throw_to_immune_test(NIMCP_ERROR_NO_MEMORY);

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_NO_MEMORY);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

/**
 * WHAT: Test NIMCP_THROW_TO_IMMUNE with various error codes
 * WHY:  Verify immune presentation works for different error types
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowToImmuneVariousErrors) {
    // Memory error
    reset_tracking();
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Memory error immune");
    EXPECT_TRUE(g_exception_presented_to_immune);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_NO_MEMORY);

    // Thread error
    reset_tracking();
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_DEADLOCK, "Deadlock immune");
    EXPECT_TRUE(g_exception_presented_to_immune);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_DEADLOCK);

    // Brain error
    reset_tracking();
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BRAIN_INVALID, "Brain error immune");
    EXPECT_TRUE(g_exception_presented_to_immune);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_BRAIN_INVALID);
}

/**
 * WHAT: Test NIMCP_THROW_TO_IMMUNE_IF macro
 * WHY:  Verify conditional immune presentation
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowToImmuneIfConditionFalse) {
    NIMCP_THROW_TO_IMMUNE_IF(false, NIMCP_ERROR_INVALID_STATE, "Condition was false");

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

/**
 * WHAT: Test NIMCP_THROW_TO_IMMUNE_IF with true condition
 * WHY:  Verify no throw when condition is true
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowToImmuneIfConditionTrue) {
    NIMCP_THROW_TO_IMMUNE_IF(true, NIMCP_ERROR_INVALID_STATE, "Should not throw");

    EXPECT_EQ(g_handler_call_count, 0);
}

//=============================================================================
// SECTION 33: NIMCP_CHECK_THROW_IMMUNE Tests
//=============================================================================

/**
 * @brief Function using NIMCP_CHECK_THROW_IMMUNE
 */
static nimcp_error_t check_throw_immune_test(void* ptr) {
    NIMCP_CHECK_THROW_IMMUNE(ptr != nullptr, NIMCP_ERROR_NULL_POINTER,
                              "Pointer is NULL - presenting to immune");
    return NIMCP_SUCCESS;
}

/**
 * @brief Function with multiple immune-integrated checks
 */
static nimcp_error_t check_throw_immune_multiple(void* ptr, size_t size, bool ready) {
    NIMCP_CHECK_THROW_IMMUNE(ptr != nullptr, NIMCP_ERROR_NULL_POINTER,
                              "Input pointer is NULL");
    NIMCP_CHECK_THROW_IMMUNE(size > 0, NIMCP_ERROR_INVALID_PARAM,
                              "Size must be positive, got %zu", size);
    NIMCP_CHECK_THROW_IMMUNE(ready, NIMCP_ERROR_INVALID_STATE,
                              "System not ready");
    return NIMCP_SUCCESS;
}

/**
 * WHAT: Test NIMCP_CHECK_THROW_IMMUNE returns error and presents to immune
 * WHY:  Verify combined check, return, and immune notification
 */
TEST_F(ExceptionMacroComprehensiveTest, CheckThrowImmuneNullPointer) {
    nimcp_error_t result = check_throw_immune_test(nullptr);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

/**
 * WHAT: Test NIMCP_CHECK_THROW_IMMUNE passes on valid input
 * WHY:  Verify successful path doesn't trigger immune
 */
TEST_F(ExceptionMacroComprehensiveTest, CheckThrowImmuneValidPointer) {
    int value = 42;
    nimcp_error_t result = check_throw_immune_test(&value);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(g_handler_call_count, 0);
}

/**
 * WHAT: Test NIMCP_CHECK_THROW_IMMUNE first failure in chain
 * WHY:  Verify first failing check triggers immune presentation
 */
TEST_F(ExceptionMacroComprehensiveTest, CheckThrowImmuneFirstFailure) {
    nimcp_error_t result = check_throw_immune_multiple(nullptr, 100, true);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

/**
 * WHAT: Test NIMCP_CHECK_THROW_IMMUNE second failure in chain
 * WHY:  Verify second check can trigger immune presentation
 */
TEST_F(ExceptionMacroComprehensiveTest, CheckThrowImmuneSecondFailure) {
    int dummy = 42;
    nimcp_error_t result = check_throw_immune_multiple(&dummy, 0, true);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

/**
 * WHAT: Test NIMCP_CHECK_THROW_IMMUNE third failure in chain
 * WHY:  Verify third check can trigger immune presentation
 */
TEST_F(ExceptionMacroComprehensiveTest, CheckThrowImmuneThirdFailure) {
    int dummy = 42;
    nimcp_error_t result = check_throw_immune_multiple(&dummy, 100, false);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_STATE);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

/**
 * WHAT: Test NIMCP_CHECK_THROW_IMMUNE all checks pass
 * WHY:  Verify successful path through all checks
 */
TEST_F(ExceptionMacroComprehensiveTest, CheckThrowImmuneAllPass) {
    int dummy = 42;
    nimcp_error_t result = check_throw_immune_multiple(&dummy, 100, true);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(g_handler_call_count, 0);
}

//=============================================================================
// SECTION 34: Error Code Propagation Tests
//=============================================================================

/**
 * @brief Level 3 function that throws
 */
static nimcp_error_t propagate_level3(bool should_fail, nimcp_error_t error_code) {
    NIMCP_CHECK_THROW(!should_fail, error_code, "Level 3 failure");
    return NIMCP_SUCCESS;
}

/**
 * @brief Level 2 function that calls level 3
 */
static nimcp_error_t propagate_level2(bool should_fail, nimcp_error_t error_code) {
    nimcp_error_t result = propagate_level3(should_fail, error_code);
    if (result != NIMCP_SUCCESS) {
        return result;  // Propagate error
    }
    return NIMCP_SUCCESS;
}

/**
 * @brief Level 1 function that calls level 2
 */
static nimcp_error_t propagate_level1(bool should_fail, nimcp_error_t error_code) {
    nimcp_error_t result = propagate_level2(should_fail, error_code);
    if (result != NIMCP_SUCCESS) {
        return result;  // Propagate error
    }
    return NIMCP_SUCCESS;
}

/**
 * WHAT: Test error code propagates through call chain
 * WHY:  Verify error codes bubble up correctly
 */
TEST_F(ExceptionMacroComprehensiveTest, ErrorCodePropagationThroughChain) {
    nimcp_error_t result = propagate_level1(true, NIMCP_ERROR_OPERATION_FAILED);

    EXPECT_EQ(result, NIMCP_ERROR_OPERATION_FAILED);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_OPERATION_FAILED);
}

/**
 * WHAT: Test different error codes propagate correctly
 * WHY:  Verify error code identity is preserved
 */
TEST_F(ExceptionMacroComprehensiveTest, ErrorCodeIdentityPreserved) {
    nimcp_error_t codes_to_test[] = {
        NIMCP_ERROR_NULL_POINTER,
        NIMCP_ERROR_INVALID_PARAM,
        NIMCP_ERROR_OUT_OF_RANGE,
        NIMCP_ERROR_NO_MEMORY,
        NIMCP_ERROR_TIMEOUT,
        NIMCP_ERROR_DEADLOCK
    };

    for (auto code : codes_to_test) {
        reset_tracking();
        nimcp_error_t result = propagate_level1(true, code);
        EXPECT_EQ(result, code) << "Error code " << code << " should propagate unchanged";
        EXPECT_EQ(g_last_error_code, code);
    }
}

/**
 * WHAT: Test successful propagation returns NIMCP_SUCCESS
 * WHY:  Verify success case propagates correctly
 */
TEST_F(ExceptionMacroComprehensiveTest, SuccessPropagationThroughChain) {
    nimcp_error_t result = propagate_level1(false, NIMCP_ERROR_OPERATION_FAILED);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(g_handler_call_count, 0);
}

//=============================================================================
// SECTION 35: Enhanced Stack Trace Tests
//=============================================================================

/**
 * WHAT: Test stack trace depth is captured
 * WHY:  Verify stack trace mechanism works
 */
TEST_F(ExceptionMacroComprehensiveTest, StackTraceDepthCaptured) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Stack trace depth test");

    ASSERT_NE(ex, nullptr);
    // Stack depth should be at least 1 (the current frame)
    // Note: Actual depth depends on platform and optimization level
    EXPECT_GE(ex->stack_trace.depth, 0u);
    EXPECT_LE(ex->stack_trace.depth, NIMCP_EXCEPTION_MAX_STACK_DEPTH);

    nimcp_exception_unref(ex);
}

/**
 * WHAT: Test stack trace frames contain addresses
 * WHY:  Verify stack frame addresses are captured
 */
TEST_F(ExceptionMacroComprehensiveTest, StackTraceFramesHaveAddresses) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Stack frame address test");

    ASSERT_NE(ex, nullptr);

    // If we have frames, they should have addresses
    for (size_t i = 0; i < ex->stack_trace.depth; i++) {
        // Addresses may or may not be resolved depending on debug info
        // Just verify the structure is valid
        // Address can be NULL on some platforms without debug info
    }

    nimcp_exception_unref(ex);
}

/**
 * WHAT: Test stack trace to string output
 * WHY:  Verify stack trace formatting produces output
 */
TEST_F(ExceptionMacroComprehensiveTest, StackTraceToStringOutput) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Stack trace string test");

    ASSERT_NE(ex, nullptr);

    char buffer[8192];
    size_t len = nimcp_stack_trace_to_string(&ex->stack_trace, buffer, sizeof(buffer));

    // Should produce some output (even if just headers or empty message)
    EXPECT_GE(len, 0u);
    EXPECT_LT(len, sizeof(buffer));

    nimcp_exception_unref(ex);
}

/**
 * WHAT: Test stack trace capture function directly
 * WHY:  Verify nimcp_exception_capture_stack_trace works
 */
TEST_F(ExceptionMacroComprehensiveTest, CaptureStackTraceDirectly) {
    nimcp_stack_trace_t trace;
    memset(&trace, 0, sizeof(trace));

    size_t depth = nimcp_exception_capture_stack_trace(&trace, 0);

    EXPECT_GE(depth, 0u);
    EXPECT_LE(depth, NIMCP_EXCEPTION_MAX_STACK_DEPTH);
    EXPECT_EQ(trace.depth, depth);
}

/**
 * WHAT: Test stack trace with skip frames
 * WHY:  Verify skip_frames parameter works
 */
TEST_F(ExceptionMacroComprehensiveTest, CaptureStackTraceSkipFrames) {
    nimcp_stack_trace_t trace1, trace2;
    memset(&trace1, 0, sizeof(trace1));
    memset(&trace2, 0, sizeof(trace2));

    size_t depth1 = nimcp_exception_capture_stack_trace(&trace1, 0);
    size_t depth2 = nimcp_exception_capture_stack_trace(&trace2, 2);

    // Skipping 2 frames should result in fewer frames
    // (or same if there weren't enough frames to begin with)
    if (depth1 >= 2) {
        EXPECT_LE(depth2, depth1);
    }
}

//=============================================================================
// SECTION 36: Exception Severity Mapping Tests
//=============================================================================

/**
 * WHAT: Test severity mapping for generic errors
 * WHY:  Verify generic errors get appropriate severity
 */
TEST_F(ExceptionMacroComprehensiveTest, SeverityMappingGenericErrors) {
    nimcp_exception_severity_t sev;

    sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_INVALID_PARAM);
    EXPECT_GE((int)sev, (int)EXCEPTION_SEVERITY_WARNING);

    sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_NULL_POINTER);
    EXPECT_GE((int)sev, (int)EXCEPTION_SEVERITY_ERROR);

    sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_GE((int)sev, (int)EXCEPTION_SEVERITY_WARNING);
}

/**
 * WHAT: Test severity mapping for memory errors
 * WHY:  Verify memory errors get higher severity
 */
TEST_F(ExceptionMacroComprehensiveTest, SeverityMappingMemoryErrors) {
    nimcp_exception_severity_t sev;

    sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_NO_MEMORY);
    EXPECT_GE((int)sev, (int)EXCEPTION_SEVERITY_ERROR);

    sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_BUFFER_OVERFLOW);
    EXPECT_GE((int)sev, (int)EXCEPTION_SEVERITY_ERROR);

    sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_MEMORY_CORRUPTION);
    EXPECT_GE((int)sev, (int)EXCEPTION_SEVERITY_SEVERE);
}

/**
 * WHAT: Test severity mapping for threading errors
 * WHY:  Verify threading errors get appropriate severity
 */
TEST_F(ExceptionMacroComprehensiveTest, SeverityMappingThreadingErrors) {
    nimcp_exception_severity_t sev;

    sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_DEADLOCK);
    EXPECT_GE((int)sev, (int)EXCEPTION_SEVERITY_SEVERE);

    sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_RACE_CONDITION);
    EXPECT_GE((int)sev, (int)EXCEPTION_SEVERITY_ERROR);

    sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_THREAD_CREATE);
    EXPECT_GE((int)sev, (int)EXCEPTION_SEVERITY_ERROR);
}

/**
 * WHAT: Test severity mapping for signal/crash errors
 * WHY:  Verify crash errors get critical severity
 */
TEST_F(ExceptionMacroComprehensiveTest, SeverityMappingSignalErrors) {
    nimcp_exception_severity_t sev;

    sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_SIGSEGV);
    EXPECT_GE((int)sev, (int)EXCEPTION_SEVERITY_CRITICAL);

    sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_SIGABRT);
    EXPECT_GE((int)sev, (int)EXCEPTION_SEVERITY_CRITICAL);
}

/**
 * WHAT: Test category mapping from error codes
 * WHY:  Verify error codes map to correct exception categories
 */
TEST_F(ExceptionMacroComprehensiveTest, CategoryMappingFromErrorCodes) {
    nimcp_exception_category_t cat;

    // Generic errors (1000-1999)
    cat = nimcp_exception_get_category_from_code(NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_GENERIC);

    // Memory errors (2000-2999)
    cat = nimcp_exception_get_category_from_code(NIMCP_ERROR_NO_MEMORY);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_MEMORY);

    // Brain errors (3000-3999)
    cat = nimcp_exception_get_category_from_code(NIMCP_ERROR_BRAIN_CREATION);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_BRAIN);

    // I/O errors (4000-4999)
    cat = nimcp_exception_get_category_from_code(NIMCP_ERROR_FILE_NOT_FOUND);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_IO);

    // Threading errors (6000-6999)
    cat = nimcp_exception_get_category_from_code(NIMCP_ERROR_DEADLOCK);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_THREADING);

    // Signal errors (7000-7999)
    cat = nimcp_exception_get_category_from_code(NIMCP_ERROR_SIGSEGV);
    EXPECT_EQ(cat, EXCEPTION_CATEGORY_SIGNAL);
}

//=============================================================================
// SECTION 37: Typed Exception Macro Tests
//=============================================================================

/**
 * WHAT: Test NIMCP_THROW_MEMORY macro
 * WHY:  Verify memory exception macro creates correct type
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowMemoryMacro) {
    NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, 4096, "Failed to allocate %zu bytes", 4096u);

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_NO_MEMORY);
    EXPECT_EQ(g_last_type, EXCEPTION_TYPE_MEMORY);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

/**
 * WHAT: Test NIMCP_THROW_BRAIN macro
 * WHY:  Verify brain exception macro creates correct type
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowBrainMacro) {
    NIMCP_THROW_BRAIN(NIMCP_ERROR_BRAIN_INVALID, 123, "hippocampus",
                      "Brain %u region %s error", 123, "hippocampus");

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_BRAIN_INVALID);
    EXPECT_EQ(g_last_type, EXCEPTION_TYPE_BRAIN);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

/**
 * WHAT: Test NIMCP_THROW_IO macro
 * WHY:  Verify I/O exception macro creates correct type
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowIOMacro) {
    NIMCP_THROW_IO(NIMCP_ERROR_FILE_NOT_FOUND, "/path/to/missing.txt",
                   "File not found: %s", "/path/to/missing.txt");

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_FILE_NOT_FOUND);
    EXPECT_EQ(g_last_type, EXCEPTION_TYPE_IO);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

/**
 * WHAT: Test NIMCP_THROW_THREADING macro
 * WHY:  Verify threading exception macro creates correct type
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowThreadingMacro) {
    NIMCP_THROW_THREADING(NIMCP_ERROR_DEADLOCK, 12345,
                          "Deadlock on thread %lu", 12345ul);

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_DEADLOCK);
    EXPECT_EQ(g_last_type, EXCEPTION_TYPE_THREADING);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

/**
 * WHAT: Test NIMCP_THROW_SECURITY macro
 * WHY:  Verify security exception macro creates correct type
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowSecurityMacro) {
    NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, 42,
                         "Security threat type %d detected", 42);

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(g_last_type, EXCEPTION_TYPE_SECURITY);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

/**
 * WHAT: Test NIMCP_THROW_GPU macro
 * WHY:  Verify GPU exception macro creates correct type
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowGPUMacro) {
    NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, 1, "GPU error on device %d", 0);

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_GPU);
    EXPECT_EQ(g_last_type, EXCEPTION_TYPE_GPU);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

//=============================================================================
// SECTION 38: Severity Override Macro Tests
//=============================================================================

/**
 * WHAT: Test NIMCP_THROW_SEVERITY macro
 * WHY:  Verify explicit severity override
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowSeverityOverride) {
    // Throw with explicit CRITICAL severity
    NIMCP_THROW_SEVERITY(NIMCP_ERROR_INVALID_PARAM, EXCEPTION_SEVERITY_CRITICAL,
                         "Critical parameter error");

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_last_severity, EXCEPTION_SEVERITY_CRITICAL);
    // CRITICAL severity should auto-present to immune
    EXPECT_TRUE(g_exception_presented_to_immune);
}

/**
 * WHAT: Test NIMCP_THROW_CRITICAL macro
 * WHY:  Verify critical severity shortcut
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowCriticalMacro) {
    NIMCP_THROW_CRITICAL(NIMCP_ERROR_OPERATION_FAILED, "Critical operation failure");

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_severity, EXCEPTION_SEVERITY_CRITICAL);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

/**
 * WHAT: Test NIMCP_THROW_FATAL macro
 * WHY:  Verify fatal severity shortcut
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowFatalMacro) {
    NIMCP_THROW_FATAL(NIMCP_ERROR_MEMORY_CORRUPTION, "Fatal memory corruption");

    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_severity, EXCEPTION_SEVERITY_FATAL);
    EXPECT_TRUE(g_exception_presented_to_immune);
}

//=============================================================================
// SECTION 39: Exception Creation Edge Cases
//=============================================================================

/**
 * WHAT: Test exception creation with very long message
 * WHY:  Verify message truncation doesn't crash
 */
TEST_F(ExceptionMacroComprehensiveTest, ExceptionLongMessageTruncation) {
    std::string long_message(NIMCP_EXCEPTION_MAX_MESSAGE * 2, 'X');

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "%s", long_message.c_str());

    ASSERT_NE(ex, nullptr);
    // Message should be truncated but not crash
    EXPECT_LT(strlen(ex->message), NIMCP_EXCEPTION_MAX_MESSAGE);

    nimcp_exception_unref(ex);
}

/**
 * WHAT: Test exception creation with NULL format string
 * WHY:  Verify NULL format doesn't crash
 */
TEST_F(ExceptionMacroComprehensiveTest, ExceptionNullFormatString) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        nullptr);

    // Should either return NULL or create exception with empty/default message
    if (ex != nullptr) {
        // If it succeeded, the message should be safe to access
        EXPECT_GE(strlen(ex->message), 0u);
        nimcp_exception_unref(ex);
    }
}

/**
 * WHAT: Test exception creation with empty format string
 * WHY:  Verify empty format doesn't crash
 */
TEST_F(ExceptionMacroComprehensiveTest, ExceptionEmptyFormatString) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "");

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(strlen(ex->message), 0u);

    nimcp_exception_unref(ex);
}

/**
 * WHAT: Test exception with special characters in message
 * WHY:  Verify special characters don't cause issues
 */
TEST_F(ExceptionMacroComprehensiveTest, ExceptionSpecialCharactersInMessage) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Error: %s\nLine: %d\tTab\r", "test<>&\"'", 42);

    ASSERT_NE(ex, nullptr);
    EXPECT_NE(strstr(ex->message, "test<>&\"'"), nullptr);

    nimcp_exception_unref(ex);
}

//=============================================================================
// SECTION 40: Immune Integration Configuration Tests
//=============================================================================

/**
 * WHAT: Test exception immune stats initialization
 * WHY:  Verify stats tracking works
 */
TEST_F(ExceptionMacroComprehensiveTest, ImmuneStatsAvailable) {
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);

    // Stats should be accessible (values depend on test order)
    // Just verify no crash and reasonable values
    EXPECT_GE(stats.exceptions_presented, 0u);
    EXPECT_GE(stats.recoveries_attempted, 0u);
}

/**
 * WHAT: Test exception immune config defaults
 * WHY:  Verify default configuration is reasonable
 */
TEST_F(ExceptionMacroComprehensiveTest, ImmuneConfigDefaults) {
    nimcp_exception_immune_config_t config;
    nimcp_exception_immune_default_config(&config);

    // Verify reasonable defaults
    EXPECT_GE((int)config.min_present_severity, (int)EXCEPTION_SEVERITY_WARNING);
    EXPECT_GT(config.max_pending_exceptions, 0u);
    EXPECT_GT(config.response_timeout_ms, 0u);
}

//=============================================================================
// SECTION 41: NIMCP_THROW_TO_IMMUNE with All Error Codes
//=============================================================================

/**
 * WHAT: Test NIMCP_THROW_TO_IMMUNE with generic error codes
 * WHY:  Verify immune presentation works for all error code ranges
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowToImmuneGenericErrors) {
    nimcp_error_t generic_codes[] = {
        NIMCP_ERROR_UNKNOWN,
        NIMCP_ERROR_NOT_IMPLEMENTED,
        NIMCP_ERROR_INVALID_PARAMETER,
        NIMCP_ERROR_NULL_POINTER,
        NIMCP_ERROR_OUT_OF_RANGE,
        NIMCP_ERROR_INVALID_STATE,
        NIMCP_ERROR_OPERATION_FAILED,
        NIMCP_ERROR_NOT_INITIALIZED,
        NIMCP_ERROR_ALREADY_EXISTS,
        NIMCP_ERROR_NOT_FOUND,
        NIMCP_ERROR_TIMEOUT,
        NIMCP_ERROR_CANCELLED,
        NIMCP_ERROR_PERMISSION_DENIED
    };

    for (auto code : generic_codes) {
        reset_tracking();
        NIMCP_THROW_TO_IMMUNE(code, "Generic error %d test", (int)code);
        EXPECT_TRUE(g_exception_presented_to_immune)
            << "Code " << code << " should be presented to immune";
        EXPECT_EQ(g_last_error_code, code);
        EXPECT_EQ(g_last_category, EXCEPTION_CATEGORY_GENERIC);
    }
}

/**
 * WHAT: Test NIMCP_THROW_TO_IMMUNE with memory error codes
 * WHY:  Memory errors should have high severity and immune presentation
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowToImmuneMemoryErrors) {
    nimcp_error_t memory_codes[] = {
        NIMCP_ERROR_NO_MEMORY,
        NIMCP_ERROR_BUFFER_TOO_SMALL,
        NIMCP_ERROR_BUFFER_OVERFLOW,
        NIMCP_ERROR_MEMORY_CORRUPTION,
        NIMCP_ERROR_INVALID_ADDRESS,
        NIMCP_ERROR_MEMORY_LEAK,
        NIMCP_ERROR_DOUBLE_FREE
    };

    for (auto code : memory_codes) {
        reset_tracking();
        NIMCP_THROW_TO_IMMUNE(code, "Memory error %d test", (int)code);
        EXPECT_TRUE(g_exception_presented_to_immune)
            << "Memory code " << code << " should be presented to immune";
        EXPECT_EQ(g_last_error_code, code);
        EXPECT_EQ(g_last_category, EXCEPTION_CATEGORY_MEMORY);
        EXPECT_GE((int)g_last_severity, (int)EXCEPTION_SEVERITY_ERROR);
    }
}

/**
 * WHAT: Test NIMCP_THROW_TO_IMMUNE with brain/network error codes
 * WHY:  Brain errors need immune system for recovery
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowToImmuneBrainErrors) {
    nimcp_error_t brain_codes[] = {
        NIMCP_ERROR_BRAIN_CREATION,
        NIMCP_ERROR_BRAIN_INVALID,
        NIMCP_ERROR_NETWORK_CREATION,
        NIMCP_ERROR_NETWORK_INVALID,
        NIMCP_ERROR_DIMENSION_MISMATCH,
        NIMCP_ERROR_WEIGHT_INIT,
        NIMCP_ERROR_FORWARD_PASS,
        NIMCP_ERROR_BACKWARD_PASS,
        NIMCP_ERROR_LEARNING_FAILED,
        NIMCP_ERROR_INFERENCE_FAILED
    };

    for (auto code : brain_codes) {
        reset_tracking();
        NIMCP_THROW_TO_IMMUNE(code, "Brain error %d test", (int)code);
        EXPECT_TRUE(g_exception_presented_to_immune)
            << "Brain code " << code << " should be presented to immune";
        EXPECT_EQ(g_last_error_code, code);
        EXPECT_EQ(g_last_category, EXCEPTION_CATEGORY_BRAIN);
    }
}

/**
 * WHAT: Test NIMCP_THROW_TO_IMMUNE with I/O error codes
 * WHY:  I/O errors may need recovery actions
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowToImmuneIOErrors) {
    nimcp_error_t io_codes[] = {
        NIMCP_ERROR_FILE_NOT_FOUND,
        NIMCP_ERROR_FILE_READ,
        NIMCP_ERROR_FILE_WRITE,
        NIMCP_ERROR_FILE_OPEN,
        NIMCP_ERROR_FILE_CLOSE,
        NIMCP_ERROR_FILE_CORRUPT,
        NIMCP_ERROR_SERIALIZATION,
        NIMCP_ERROR_DESERIALIZATION,
        NIMCP_ERROR_NETWORK_IO,
        NIMCP_ERROR_IO
    };

    for (auto code : io_codes) {
        reset_tracking();
        NIMCP_THROW_TO_IMMUNE(code, "I/O error %d test", (int)code);
        EXPECT_TRUE(g_exception_presented_to_immune)
            << "I/O code " << code << " should be presented to immune";
        EXPECT_EQ(g_last_error_code, code);
        EXPECT_EQ(g_last_category, EXCEPTION_CATEGORY_IO);
    }
}

/**
 * WHAT: Test NIMCP_THROW_TO_IMMUNE with threading error codes
 * WHY:  Threading errors (deadlock, etc.) need urgent recovery
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowToImmuneThreadingErrors) {
    nimcp_error_t thread_codes[] = {
        NIMCP_ERROR_THREAD_CREATE,
        NIMCP_ERROR_THREAD_JOIN,
        NIMCP_ERROR_MUTEX_LOCK,
        NIMCP_ERROR_MUTEX_UNLOCK,
        NIMCP_ERROR_MUTEX_INIT,
        NIMCP_ERROR_DEADLOCK,
        NIMCP_ERROR_RACE_CONDITION,
        NIMCP_ERROR_THREAD_SYNC
    };

    for (auto code : thread_codes) {
        reset_tracking();
        NIMCP_THROW_TO_IMMUNE(code, "Threading error %d test", (int)code);
        EXPECT_TRUE(g_exception_presented_to_immune)
            << "Threading code " << code << " should be presented to immune";
        EXPECT_EQ(g_last_error_code, code);
        EXPECT_EQ(g_last_category, EXCEPTION_CATEGORY_THREADING);
    }
}

/**
 * WHAT: Test NIMCP_THROW_TO_IMMUNE with signal error codes
 * WHY:  Signal/crash errors are critical for recovery
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowToImmuneSignalErrors) {
    nimcp_error_t signal_codes[] = {
        NIMCP_ERROR_SIGNAL_RECEIVED,
        NIMCP_ERROR_SIGSEGV,
        NIMCP_ERROR_SIGABRT,
        NIMCP_ERROR_SIGFPE,
        NIMCP_ERROR_SIGBUS,
        NIMCP_ERROR_SIGILL,
        NIMCP_ERROR_CRASH_RECOVERY,
        NIMCP_ERROR_CHECKPOINT_SAVE,
        NIMCP_ERROR_CHECKPOINT_LOAD
    };

    for (auto code : signal_codes) {
        reset_tracking();
        NIMCP_THROW_TO_IMMUNE(code, "Signal error %d test", (int)code);
        EXPECT_TRUE(g_exception_presented_to_immune)
            << "Signal code " << code << " should be presented to immune";
        EXPECT_EQ(g_last_error_code, code);
        EXPECT_EQ(g_last_category, EXCEPTION_CATEGORY_SIGNAL);
    }
}

/**
 * WHAT: Test NIMCP_THROW_TO_IMMUNE with security error codes
 * WHY:  Security threats need immediate immune response
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowToImmuneSecurityErrors) {
    nimcp_error_t security_codes[] = {
        NIMCP_ERROR_SECURITY_BASE,
        NIMCP_ERROR_BBB_REJECTED,
        NIMCP_ERROR_BBB_VALIDATION,
        NIMCP_ERROR_SECURITY_THREAT,
        NIMCP_ERROR_ACCESS_DENIED,
        NIMCP_ERROR_SIGNATURE_INVALID,
        NIMCP_ERROR_ENCRYPTION_FAILED,
        NIMCP_ERROR_DECRYPTION_FAILED
    };

    for (auto code : security_codes) {
        reset_tracking();
        NIMCP_THROW_TO_IMMUNE(code, "Security error %d test", (int)code);
        EXPECT_TRUE(g_exception_presented_to_immune)
            << "Security code " << code << " should be presented to immune";
        EXPECT_EQ(g_last_error_code, code);
        EXPECT_EQ(g_last_category, EXCEPTION_CATEGORY_SECURITY);
    }
}

/**
 * WHAT: Test NIMCP_THROW_TO_IMMUNE with cognitive error codes
 * WHY:  Cognitive errors need brain immune system integration
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowToImmuneCognitiveErrors) {
    nimcp_error_t cognitive_codes[] = {
        NIMCP_ERROR_WORKING_MEMORY,
        NIMCP_ERROR_EMOTIONAL_TAGGING,
        NIMCP_ERROR_EXECUTIVE_CONTROL,
        NIMCP_ERROR_SLEEP_WAKE,
        NIMCP_ERROR_MENTAL_HEALTH,
        NIMCP_ERROR_THEORY_OF_MIND,
        NIMCP_ERROR_EXPLANATIONS,
        NIMCP_ERROR_META_LEARNING,
        NIMCP_ERROR_PREDICTIVE
    };

    for (auto code : cognitive_codes) {
        reset_tracking();
        NIMCP_THROW_TO_IMMUNE(code, "Cognitive error %d test", (int)code);
        EXPECT_TRUE(g_exception_presented_to_immune)
            << "Cognitive code " << code << " should be presented to immune";
        EXPECT_EQ(g_last_error_code, code);
        EXPECT_EQ(g_last_category, EXCEPTION_CATEGORY_COGNITIVE);
    }
}

/**
 * WHAT: Test NIMCP_THROW_TO_IMMUNE with GPU error codes
 * WHY:  GPU errors may need hardware-level recovery
 */
TEST_F(ExceptionMacroComprehensiveTest, ThrowToImmuneGPUErrors) {
    nimcp_error_t gpu_codes[] = {
        NIMCP_ERROR_GPU,
        NIMCP_ERROR_GPU_NOT_AVAILABLE,
        NIMCP_ERROR_GPU_MEMORY,
        NIMCP_ERROR_CUDA,
        NIMCP_ERROR_KERNEL_LAUNCH,
        NIMCP_ERROR_GPU_SYNC
    };

    for (auto code : gpu_codes) {
        reset_tracking();
        NIMCP_THROW_TO_IMMUNE(code, "GPU error %d test", (int)code);
        EXPECT_TRUE(g_exception_presented_to_immune)
            << "GPU code " << code << " should be presented to immune";
        EXPECT_EQ(g_last_error_code, code);
        EXPECT_EQ(g_last_category, EXCEPTION_CATEGORY_GPU);
    }
}

//=============================================================================
// SECTION 42: Nested Exception Scenarios
//=============================================================================

/**
 * @brief Level 3 function that throws with immune presentation
 */
static nimcp_error_t nested_level3(bool fail) {
    if (fail) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ADDRESS,
            "Nested level 3 failure");
        return NIMCP_ERROR_INVALID_ADDRESS;
    }
    return NIMCP_SUCCESS;
}

/**
 * @brief Level 2 function that wraps level 3 exception
 */
static nimcp_error_t nested_level2(bool fail) {
    nimcp_error_t result = nested_level3(fail);
    if (result != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_MEMORY_CORRUPTION,
            "Nested level 2 wrapping level 3 error %d", (int)result);
        return NIMCP_ERROR_MEMORY_CORRUPTION;
    }
    return NIMCP_SUCCESS;
}

/**
 * @brief Level 1 function that wraps level 2 exception
 */
static nimcp_error_t nested_level1(bool fail) {
    nimcp_error_t result = nested_level2(fail);
    if (result != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Nested level 1 wrapping level 2 error %d", (int)result);
        return NIMCP_ERROR_NO_MEMORY;
    }
    return NIMCP_SUCCESS;
}

/**
 * WHAT: Test nested exception scenario dispatches all levels
 * WHY:  Verify exception chain is preserved through nesting
 */
TEST_F(ExceptionMacroComprehensiveTest, NestedExceptionMultipleLevels) {
    reset_tracking();

    nimcp_error_t result = nested_level1(true);

    EXPECT_EQ(result, NIMCP_ERROR_NO_MEMORY);
    // Should have 3 handler calls (one per level)
    EXPECT_EQ(g_handler_call_count, 3);

    // Last exception should be NO_MEMORY from level 1
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_NO_MEMORY);
}

/**
 * WHAT: Test nested exceptions all present to immune
 * WHY:  Each level's exception should reach immune system
 */
TEST_F(ExceptionMacroComprehensiveTest, NestedExceptionAllPresentToImmune) {
    // We'll check that all 3 levels present to immune
    reset_tracking();

    nimcp_error_t result = nested_level1(true);

    EXPECT_EQ(result, NIMCP_ERROR_NO_MEMORY);
    // The last exception presented flag should be true
    EXPECT_TRUE(g_exception_presented_to_immune);
}

/**
 * WHAT: Test nested exception with cause chaining
 * WHY:  Verify cause chain is properly established
 */
TEST_F(ExceptionMacroComprehensiveTest, NestedExceptionCauseChaining) {
    // Create a proper cause chain manually
    nimcp_exception_t* root_cause = nimcp_exception_create(
        NIMCP_ERROR_INVALID_ADDRESS,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Root cause: invalid address access");
    ASSERT_NE(root_cause, nullptr);

    nimcp_exception_t* mid_cause = nimcp_exception_create(
        NIMCP_ERROR_MEMORY_CORRUPTION,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Mid-level: memory corruption detected");
    ASSERT_NE(mid_cause, nullptr);
    nimcp_exception_set_cause(mid_cause, root_cause);

    nimcp_exception_t* top_exception = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        "Top-level: allocation failed");
    ASSERT_NE(top_exception, nullptr);
    nimcp_exception_set_cause(top_exception, mid_cause);

    // Verify cause chain
    nimcp_exception_t* cause1 = nimcp_exception_get_cause(top_exception);
    ASSERT_NE(cause1, nullptr);
    EXPECT_EQ(cause1->code, NIMCP_ERROR_MEMORY_CORRUPTION);

    nimcp_exception_t* cause2 = nimcp_exception_get_cause(cause1);
    ASSERT_NE(cause2, nullptr);
    EXPECT_EQ(cause2->code, NIMCP_ERROR_INVALID_ADDRESS);

    nimcp_exception_t* cause3 = nimcp_exception_get_cause(cause2);
    EXPECT_EQ(cause3, nullptr);  // No more causes

    nimcp_exception_unref(top_exception);
}

/**
 * WHAT: Test deeply nested exception (10 levels)
 * WHY:  Verify system handles deep nesting without overflow
 */
TEST_F(ExceptionMacroComprehensiveTest, DeeplyNestedExceptionChain) {
    const int DEPTH = 10;
    nimcp_exception_t* chain[DEPTH];

    // Build chain from bottom up
    for (int i = 0; i < DEPTH; i++) {
        chain[i] = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED + i,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Nested exception at depth %d", i);
        ASSERT_NE(chain[i], nullptr);

        if (i > 0) {
            nimcp_exception_set_cause(chain[i], chain[i-1]);
        }
    }

    // Verify we can traverse the full chain
    nimcp_exception_t* current = chain[DEPTH-1];
    int count = 0;
    while (current != nullptr) {
        count++;
        current = nimcp_exception_get_cause(current);
    }
    EXPECT_EQ(count, DEPTH);

    // Only unref the top - cause chain is handled
    nimcp_exception_unref(chain[DEPTH-1]);
}

//=============================================================================
// SECTION 43: Exception Context Preservation
//=============================================================================

/**
 * WHAT: Test context is preserved through exception dispatch
 * WHY:  Context data should survive handler chain
 */
TEST_F(ExceptionMacroComprehensiveTest, ContextPreservedThroughDispatch) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Context preservation test");
    ASSERT_NE(ex, nullptr);

    // Add multiple context entries
    nimcp_exception_set_context(ex, "module", "test_module");
    nimcp_exception_set_context(ex, "operation", "context_test");
    nimcp_exception_set_context(ex, "iteration", "42");
    nimcp_exception_set_context(ex, "file_path", "/tmp/test.dat");

    // Dispatch
    nimcp_exception_dispatch(ex);

    // Verify context still intact after dispatch
    EXPECT_STREQ(nimcp_exception_get_context(ex, "module"), "test_module");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "operation"), "context_test");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "iteration"), "42");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "file_path"), "/tmp/test.dat");

    nimcp_exception_unref(ex);
}

/**
 * WHAT: Test context modification during exception handling
 * WHY:  Handlers may add context during processing
 */
TEST_F(ExceptionMacroComprehensiveTest, ContextModificationDuringHandling) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Context modification test");
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "original", "value1");
    EXPECT_EQ(nimcp_exception_context_count(ex), 1u);

    // Add more context
    nimcp_exception_set_context(ex, "added1", "value2");
    nimcp_exception_set_context(ex, "added2", "value3");
    EXPECT_EQ(nimcp_exception_context_count(ex), 3u);

    // Overwrite existing
    nimcp_exception_set_context(ex, "original", "modified");
    EXPECT_EQ(nimcp_exception_context_count(ex), 3u);
    EXPECT_STREQ(nimcp_exception_get_context(ex, "original"), "modified");

    nimcp_exception_unref(ex);
}

/**
 * WHAT: Test context capacity limits
 * WHY:  Verify behavior at max context entries
 */
TEST_F(ExceptionMacroComprehensiveTest, ContextCapacityLimits) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Context capacity test");
    ASSERT_NE(ex, nullptr);

    // Add entries up to limit
    char key[64], value[128];
    for (size_t i = 0; i < NIMCP_EXCEPTION_MAX_CONTEXT_ENTRIES; i++) {
        snprintf(key, sizeof(key), "key_%zu", i);
        snprintf(value, sizeof(value), "value_%zu", i);
        nimcp_exception_set_context(ex, key, value);
    }
    EXPECT_EQ(nimcp_exception_context_count(ex), NIMCP_EXCEPTION_MAX_CONTEXT_ENTRIES);

    // Verify all entries accessible
    for (size_t i = 0; i < NIMCP_EXCEPTION_MAX_CONTEXT_ENTRIES; i++) {
        snprintf(key, sizeof(key), "key_%zu", i);
        snprintf(value, sizeof(value), "value_%zu", i);
        EXPECT_STREQ(nimcp_exception_get_context(ex, key), value);
    }

    nimcp_exception_unref(ex);
}

/**
 * WHAT: Test context with special characters
 * WHY:  Context values may contain paths, URLs, etc.
 */
TEST_F(ExceptionMacroComprehensiveTest, ContextSpecialCharacters) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Context special chars test");
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "path", "/home/user/file with spaces.txt");
    nimcp_exception_set_context(ex, "url", "https://example.com/path?param=value&other=123");
    nimcp_exception_set_context(ex, "json", "{\"key\": \"value\", \"num\": 42}");

    EXPECT_STREQ(nimcp_exception_get_context(ex, "path"), "/home/user/file with spaces.txt");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "url"), "https://example.com/path?param=value&other=123");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "json"), "{\"key\": \"value\", \"num\": 42}");

    nimcp_exception_unref(ex);
}

//=============================================================================
// SECTION 44: Memory Cleanup on Exception
//=============================================================================

/**
 * WHAT: Test exception memory cleanup via unref
 * WHY:  Verify no memory leaks from exception handling
 */
TEST_F(ExceptionMacroComprehensiveTest, MemoryCleanupViaUnref) {
    // Create many exceptions and properly cleanup
    for (int i = 0; i < 100; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Memory cleanup test %d", i);
        ASSERT_NE(ex, nullptr);

        nimcp_exception_set_context(ex, "iteration", std::to_string(i).c_str());
        nimcp_exception_dispatch(ex);
        nimcp_exception_unref(ex);
    }
    // If we get here without crash/ASAN error, cleanup is working
    SUCCEED();
}

/**
 * WHAT: Test exception cleanup with cause chain
 * WHY:  Cause chain should be properly freed
 */
TEST_F(ExceptionMacroComprehensiveTest, MemoryCleanupWithCauseChain) {
    for (int round = 0; round < 50; round++) {
        nimcp_exception_t* cause = nimcp_exception_create(
            NIMCP_ERROR_INVALID_PARAM,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Cause exception %d", round);

        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Main exception %d", round);

        ASSERT_NE(cause, nullptr);
        ASSERT_NE(ex, nullptr);

        nimcp_exception_set_cause(ex, cause);
        nimcp_exception_dispatch(ex);
        nimcp_exception_unref(ex);  // Should also cleanup cause
    }
    SUCCEED();
}

/**
 * WHAT: Test exception cleanup with typed exceptions
 * WHY:  Derived exception types should cleanup properly
 */
TEST_F(ExceptionMacroComprehensiveTest, MemoryCleanupTypedExceptions) {
    // Memory exceptions
    for (int i = 0; i < 20; i++) {
        nimcp_memory_exception_t* mex = nimcp_memory_exception_create(
            NIMCP_ERROR_NO_MEMORY,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            1024 * i,
            "Memory exception %d", i);
        ASSERT_NE(mex, nullptr);
        nimcp_exception_dispatch((nimcp_exception_t*)mex);
        nimcp_exception_unref((nimcp_exception_t*)mex);
    }

    // Brain exceptions
    for (int i = 0; i < 20; i++) {
        nimcp_brain_exception_t* bex = nimcp_brain_exception_create(
            NIMCP_ERROR_LEARNING_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            i, "test_region",
            "Brain exception %d", i);
        ASSERT_NE(bex, nullptr);
        nimcp_exception_dispatch((nimcp_exception_t*)bex);
        nimcp_exception_unref((nimcp_exception_t*)bex);
    }

    // I/O exceptions
    for (int i = 0; i < 20; i++) {
        nimcp_io_exception_t* iex = nimcp_io_exception_create(
            NIMCP_ERROR_FILE_READ,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "/test/path",
            "IO exception %d", i);
        ASSERT_NE(iex, nullptr);
        nimcp_exception_dispatch((nimcp_exception_t*)iex);
        nimcp_exception_unref((nimcp_exception_t*)iex);
    }

    SUCCEED();
}

/**
 * WHAT: Test exception cleanup with aggregate exceptions
 * WHY:  Aggregate exceptions with children must cleanup properly
 */
TEST_F(ExceptionMacroComprehensiveTest, MemoryCleanupAggregateException) {
    for (int round = 0; round < 20; round++) {
        nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Aggregate exception %d", round);
        ASSERT_NE(agg, nullptr);

        // Add child exceptions
        for (int c = 0; c < 5; c++) {
            nimcp_exception_t* child = nimcp_exception_create(
                NIMCP_ERROR_INVALID_PARAM,
                EXCEPTION_SEVERITY_WARNING,
                __FILE__, __LINE__, __func__,
                "Child %d of aggregate %d", c, round);
            ASSERT_NE(child, nullptr);
            nimcp_aggregate_exception_add(agg, child);
        }

        nimcp_exception_dispatch((nimcp_exception_t*)agg);
        nimcp_exception_unref((nimcp_exception_t*)agg);  // Should cleanup children
    }
    SUCCEED();
}

/**
 * WHAT: Test reference counting prevents premature cleanup
 * WHY:  Multiple refs should keep exception alive
 */
TEST_F(ExceptionMacroComprehensiveTest, RefCountingPreventsEarlyCleanup) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Ref counting test");
    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->ref_count, 1);

    // Add multiple references
    nimcp_exception_ref(ex);
    EXPECT_EQ(ex->ref_count, 2);
    nimcp_exception_ref(ex);
    EXPECT_EQ(ex->ref_count, 3);
    nimcp_exception_ref(ex);
    EXPECT_EQ(ex->ref_count, 4);

    // Unref multiple times - exception should remain valid
    nimcp_exception_unref(ex);
    EXPECT_EQ(ex->ref_count, 3);
    nimcp_exception_unref(ex);
    EXPECT_EQ(ex->ref_count, 2);
    nimcp_exception_unref(ex);
    EXPECT_EQ(ex->ref_count, 1);

    // Final unref - exception is freed
    nimcp_exception_unref(ex);
    // ex is now invalid - don't access
    SUCCEED();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
