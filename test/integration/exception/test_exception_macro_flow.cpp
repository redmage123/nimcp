/**
 * @file test_exception_macro_flow.cpp
 * @brief Integration tests for exception macro flow across modules
 *
 * WHAT: Test exception macros (NIMCP_THROW, NIMCP_THROW_TO_IMMUNE, etc.) integration
 * WHY:  Verify macros correctly create, dispatch, and present exceptions
 * HOW:  Use macros in simulated module contexts and verify end-to-end behavior
 *
 * TEST SCENARIOS:
 * - NIMCP_THROW creates and dispatches exceptions
 * - NIMCP_THROW_TO_IMMUNE presents to immune system
 * - NIMCP_THROW_IF conditional throwing
 * - NIMCP_CHECK_THROW return pattern
 * - Typed exception macros (NIMCP_THROW_BRAIN, NIMCP_THROW_GPU, NIMCP_THROW_SECURITY)
 * - Recovery macros (NIMCP_THROW_AND_RECOVER)
 * - Severity override macros (NIMCP_THROW_CRITICAL, NIMCP_THROW_FATAL)
 *
 * HEADER FILES READ:
 * - include/utils/exception/nimcp_exception_macros.h
 * - include/utils/exception/nimcp_exception.h
 * - include/utils/exception/nimcp_exception_handlers.h
 * - include/utils/exception/nimcp_exception_immune.h
 * - include/utils/error/nimcp_error_codes.h
 *
 * FUNCTION SIGNATURES USED:
 * - nimcp_exception_create(code, severity, file, line, func, format, ...)
 * - nimcp_exception_dispatch(ex) -> bool
 * - nimcp_exception_present_to_immune(ex, response) -> int
 * - nimcp_exception_throw(code, file, line, func, format, ...)
 * - nimcp_exception_system_init() -> int
 * - nimcp_exception_system_shutdown()
 * - nimcp_exception_immune_init(config) -> int
 * - nimcp_exception_immune_shutdown()
 * - nimcp_handler_register(options) -> nimcp_handler_registration_t*
 * - nimcp_handler_unregister(reg) -> int
 * - nimcp_exception_unref(ex)
 * - nimcp_brain_exception_create(code, severity, file, line, func, brain_id, region_name, format, ...)
 * - nimcp_gpu_exception_create(code, severity, file, line, func, device_id, cuda_err, format, ...)
 * - nimcp_security_exception_create(code, severity, file, line, func, threat_type, format, ...)
 *
 * @author NIMCP Development Team
 * @date 2026-01-21
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
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Helper Structures
//=============================================================================

/**
 * @brief Record of an exception received by a handler
 */
struct ExceptionRecord {
    nimcp_error_t code;
    nimcp_exception_severity_t severity;
    nimcp_exception_type_t type;
    nimcp_exception_category_t category;
    std::string message;
    std::string file;
    std::string function;
    int line;
    bool presented_to_immune;
    uint64_t timestamp_us;
};

/**
 * @brief Shared test state
 */
static struct {
    std::vector<ExceptionRecord> received_exceptions;
    std::atomic<int> handler_call_count{0};
    std::atomic<bool> should_consume{false};
    nimcp_exception_recovery_action_t last_recovery_action{EXCEPTION_RECOVERY_NONE};
    std::atomic<int> recovery_call_count{0};
} g_macro_test_state;

//=============================================================================
// Test Handler Functions
//=============================================================================

/**
 * @brief Recording handler that captures exception details
 */
static bool macro_recording_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;

    ExceptionRecord record;
    record.code = ex->code;
    record.severity = ex->severity;
    record.type = ex->type;
    record.category = ex->category;
    record.message = ex->message;
    record.file = ex->file ? ex->file : "";
    record.function = ex->function ? ex->function : "";
    record.line = ex->line;
    record.presented_to_immune = ex->presented_to_immune;
    record.timestamp_us = ex->timestamp_us;

    g_macro_test_state.received_exceptions.push_back(record);
    g_macro_test_state.handler_call_count++;

    return g_macro_test_state.should_consume.load();
}

/**
 * @brief Recovery callback for testing recovery macros
 */
static int test_recovery_callback(nimcp_exception_t* ex,
                                   nimcp_exception_recovery_action_t action,
                                   void* user_data) {
    (void)ex;
    (void)user_data;
    g_macro_test_state.last_recovery_action = action;
    g_macro_test_state.recovery_call_count++;
    return 0;  // Success
}

//=============================================================================
// Test Fixture
//=============================================================================

class ExceptionMacroFlowTest : public ::testing::Test {
protected:
    std::vector<nimcp_handler_registration_t*> registrations_;

    void SetUp() override {
        // Clear test state
        g_macro_test_state.received_exceptions.clear();
        g_macro_test_state.handler_call_count = 0;
        g_macro_test_state.should_consume = false;
        g_macro_test_state.last_recovery_action = EXCEPTION_RECOVERY_NONE;
        g_macro_test_state.recovery_call_count = 0;

        // Initialize exception system
        nimcp_exception_system_init();

        // Initialize immune system (may fail if not fully implemented, that's ok)
        nimcp_exception_immune_config_t config;
        nimcp_exception_immune_default_config(&config);
        config.enable_auto_present = false;  // Manual control for testing
        nimcp_exception_immune_init(&config);

        // Register our test handler
        nimcp_handler_options_t options;
        nimcp_handler_default_options(&options);
        options.name = "macro_test_handler";
        options.handler = macro_recording_handler;
        options.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        options.user_data = nullptr;

        auto* reg = nimcp_handler_register(&options);
        if (reg) {
            registrations_.push_back(reg);
        }
    }

    void TearDown() override {
        // Unregister handlers
        for (auto* reg : registrations_) {
            if (reg) {
                nimcp_handler_unregister(reg);
            }
        }
        registrations_.clear();

        // Cleanup
        nimcp_exception_clear_current();
        nimcp_exception_handlers_shutdown();
        nimcp_exception_immune_shutdown();
        nimcp_exception_system_shutdown();
    }

    /**
     * @brief Helper to verify last exception matches expected values
     */
    void verifyLastException(nimcp_error_t expected_code,
                             nimcp_exception_severity_t expected_severity = EXCEPTION_SEVERITY_ERROR) {
        ASSERT_FALSE(g_macro_test_state.received_exceptions.empty())
            << "No exceptions received";
        const auto& last = g_macro_test_state.received_exceptions.back();
        EXPECT_EQ(last.code, expected_code) << "Exception code mismatch";
        EXPECT_EQ(last.severity, expected_severity) << "Exception severity mismatch";
    }

    /**
     * @brief Helper to verify exception type
     */
    void verifyLastExceptionType(nimcp_exception_type_t expected_type) {
        ASSERT_FALSE(g_macro_test_state.received_exceptions.empty())
            << "No exceptions received";
        const auto& last = g_macro_test_state.received_exceptions.back();
        EXPECT_EQ(last.type, expected_type) << "Exception type mismatch";
    }
};

//=============================================================================
// Test: NIMCP_THROW Macro Creates and Dispatches Exception
//=============================================================================

TEST_F(ExceptionMacroFlowTest, NimcpThrowCreatesAndDispatchesException) {
    // WHAT: Verify NIMCP_THROW macro creates exception and dispatches to handlers
    // WHY:  Core functionality - macros must work correctly

    int initial_count = g_macro_test_state.handler_call_count.load();

    // Use the macro
    NIMCP_THROW(NIMCP_ERROR_INVALID_PARAMETER, "Test parameter '%s' is invalid", "test_param");

    // Verify handler was called
    EXPECT_GT(g_macro_test_state.handler_call_count.load(), initial_count)
        << "Handler should have been called";

    // Verify exception details
    verifyLastException(NIMCP_ERROR_INVALID_PARAMETER);

    // Verify message contains our parameter
    const auto& last = g_macro_test_state.received_exceptions.back();
    EXPECT_TRUE(last.message.find("test_param") != std::string::npos)
        << "Exception message should contain parameter name";
}

//=============================================================================
// Test: NIMCP_THROW_IF Conditional Throwing
//=============================================================================

TEST_F(ExceptionMacroFlowTest, NimcpThrowIfConditionalThrowing) {
    // WHAT: Verify NIMCP_THROW_IF only throws when condition is false
    // WHY:  Conditional throwing is a common pattern

    int initial_count = g_macro_test_state.handler_call_count.load();

    // Condition true - should NOT throw
    bool condition_true = true;
    NIMCP_THROW_IF(condition_true, NIMCP_ERROR_INVALID_STATE, "Should not throw");

    EXPECT_EQ(g_macro_test_state.handler_call_count.load(), initial_count)
        << "Handler should NOT have been called when condition is true";

    // Condition false - SHOULD throw
    bool condition_false = false;
    NIMCP_THROW_IF(condition_false, NIMCP_ERROR_INVALID_STATE, "Should throw");

    EXPECT_GT(g_macro_test_state.handler_call_count.load(), initial_count)
        << "Handler SHOULD have been called when condition is false";

    verifyLastException(NIMCP_ERROR_INVALID_STATE);
}

//=============================================================================
// Test: NIMCP_THROW_TO_IMMUNE Presents to Immune System
//=============================================================================

TEST_F(ExceptionMacroFlowTest, NimcpThrowToImmunePresentToImmune) {
    // WHAT: Verify NIMCP_THROW_TO_IMMUNE presents exception to immune system
    // WHY:  Immune integration is core to NIMCP's self-healing architecture

    int initial_count = g_macro_test_state.handler_call_count.load();

    // Use the immune-presenting macro
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Memory allocation failed for %zu bytes",
                          (size_t)1024);

    EXPECT_GT(g_macro_test_state.handler_call_count.load(), initial_count)
        << "Handler should have been called";

    // Verify exception was marked as presented to immune
    // Note: The macro sets presented_to_immune in the exception before dispatch
    verifyLastException(NIMCP_ERROR_NO_MEMORY);

    const auto& last = g_macro_test_state.received_exceptions.back();
    EXPECT_TRUE(last.presented_to_immune)
        << "Exception should be marked as presented to immune";
}

//=============================================================================
// Test: NIMCP_THROW_BRAIN Creates Brain Exception
//=============================================================================

TEST_F(ExceptionMacroFlowTest, NimcpThrowBrainCreatesBrainException) {
    // WHAT: Verify NIMCP_THROW_BRAIN creates properly typed brain exception
    // WHY:  Brain exceptions carry additional context (brain_id, region_name)

    int initial_count = g_macro_test_state.handler_call_count.load();

    // Use the brain exception macro
    NIMCP_THROW_BRAIN(NIMCP_ERROR_BRAIN_CREATION, 42, "visual_cortex",
                      "Brain region %s failed to initialize", "visual_cortex");

    EXPECT_GT(g_macro_test_state.handler_call_count.load(), initial_count)
        << "Handler should have been called";

    // Verify exception type
    verifyLastExceptionType(EXCEPTION_TYPE_BRAIN);
    verifyLastException(NIMCP_ERROR_BRAIN_CREATION);
}

//=============================================================================
// Test: NIMCP_THROW_GPU Creates GPU Exception
//=============================================================================

TEST_F(ExceptionMacroFlowTest, NimcpThrowGpuCreatesGpuException) {
    // WHAT: Verify NIMCP_THROW_GPU creates properly typed GPU exception
    // WHY:  GPU exceptions carry device_id and CUDA error codes

    int initial_count = g_macro_test_state.handler_call_count.load();

    // Use the GPU exception macro
    NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, 1, "CUDA kernel failed on device %d", 0);

    EXPECT_GT(g_macro_test_state.handler_call_count.load(), initial_count)
        << "Handler should have been called";

    // Verify exception type
    verifyLastExceptionType(EXCEPTION_TYPE_GPU);
    verifyLastException(NIMCP_ERROR_GPU);
}

//=============================================================================
// Test: NIMCP_THROW_SECURITY Creates Security Exception
//=============================================================================

TEST_F(ExceptionMacroFlowTest, NimcpThrowSecurityCreatesSecurityException) {
    // WHAT: Verify NIMCP_THROW_SECURITY creates critical security exception
    // WHY:  Security exceptions are always critical severity

    int initial_count = g_macro_test_state.handler_call_count.load();

    // Use the security exception macro
    NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, 1, "Threat detected: %s", "injection_attempt");

    EXPECT_GT(g_macro_test_state.handler_call_count.load(), initial_count)
        << "Handler should have been called";

    // Verify exception type and severity
    verifyLastExceptionType(EXCEPTION_TYPE_SECURITY);

    const auto& last = g_macro_test_state.received_exceptions.back();
    EXPECT_EQ(last.severity, EXCEPTION_SEVERITY_CRITICAL)
        << "Security exceptions should always be CRITICAL severity";
}

//=============================================================================
// Test: NIMCP_THROW_MEMORY Creates Memory Exception
//=============================================================================

TEST_F(ExceptionMacroFlowTest, NimcpThrowMemoryCreatesMemoryException) {
    // WHAT: Verify NIMCP_THROW_MEMORY creates properly typed memory exception
    // WHY:  Memory exceptions carry allocation size information

    int initial_count = g_macro_test_state.handler_call_count.load();

    // Use the memory exception macro
    size_t requested_size = 1024 * 1024;  // 1MB
    NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, requested_size,
                       "Failed to allocate %zu bytes", requested_size);

    EXPECT_GT(g_macro_test_state.handler_call_count.load(), initial_count)
        << "Handler should have been called";

    // Verify exception type
    verifyLastExceptionType(EXCEPTION_TYPE_MEMORY);
    verifyLastException(NIMCP_ERROR_NO_MEMORY);
}

//=============================================================================
// Test: NIMCP_THROW_IO Creates IO Exception
//=============================================================================

TEST_F(ExceptionMacroFlowTest, NimcpThrowIoCreatesIoException) {
    // WHAT: Verify NIMCP_THROW_IO creates properly typed I/O exception
    // WHY:  I/O exceptions carry file path information

    int initial_count = g_macro_test_state.handler_call_count.load();

    // Use the I/O exception macro
    const char* path = "/tmp/nonexistent_file.dat";
    NIMCP_THROW_IO(NIMCP_ERROR_FILE_NOT_FOUND, path, "File not found: %s", path);

    EXPECT_GT(g_macro_test_state.handler_call_count.load(), initial_count)
        << "Handler should have been called";

    // Verify exception type
    verifyLastExceptionType(EXCEPTION_TYPE_IO);
    verifyLastException(NIMCP_ERROR_FILE_NOT_FOUND);
}

//=============================================================================
// Test: NIMCP_THROW_THREADING Creates Threading Exception
//=============================================================================

TEST_F(ExceptionMacroFlowTest, NimcpThrowThreadingCreatesThreadingException) {
    // WHAT: Verify NIMCP_THROW_THREADING creates properly typed threading exception
    // WHY:  Threading exceptions carry thread ID information

    int initial_count = g_macro_test_state.handler_call_count.load();

    // Use the threading exception macro
    uint64_t thread_id = 12345;
    NIMCP_THROW_THREADING(NIMCP_ERROR_DEADLOCK, thread_id,
                          "Deadlock detected in thread %lu", (unsigned long)thread_id);

    EXPECT_GT(g_macro_test_state.handler_call_count.load(), initial_count)
        << "Handler should have been called";

    // Verify exception type
    verifyLastExceptionType(EXCEPTION_TYPE_THREADING);
    verifyLastException(NIMCP_ERROR_DEADLOCK);
}

//=============================================================================
// Test: NIMCP_THROW_SEVERITY Override Severity
//=============================================================================

TEST_F(ExceptionMacroFlowTest, NimcpThrowSeverityOverridesSeverity) {
    // WHAT: Verify NIMCP_THROW_SEVERITY allows explicit severity setting
    // WHY:  Sometimes need to override the default severity from error code

    int initial_count = g_macro_test_state.handler_call_count.load();

    // Use severity override macro with WARNING severity
    NIMCP_THROW_SEVERITY(NIMCP_ERROR_OPERATION_FAILED, EXCEPTION_SEVERITY_WARNING,
                         "Non-critical operation failed");

    EXPECT_GT(g_macro_test_state.handler_call_count.load(), initial_count)
        << "Handler should have been called";

    verifyLastException(NIMCP_ERROR_OPERATION_FAILED, EXCEPTION_SEVERITY_WARNING);
}

//=============================================================================
// Test: NIMCP_THROW_CRITICAL Always Critical Severity
//=============================================================================

TEST_F(ExceptionMacroFlowTest, NimcpThrowCriticalAlwaysCriticalSeverity) {
    // WHAT: Verify NIMCP_THROW_CRITICAL always creates CRITICAL severity exception
    // WHY:  Critical exceptions should bypass normal severity calculation

    int initial_count = g_macro_test_state.handler_call_count.load();

    // Use critical macro with a normally non-critical error code
    NIMCP_THROW_CRITICAL(NIMCP_ERROR_INVALID_PARAMETER, "Critical parameter error");

    EXPECT_GT(g_macro_test_state.handler_call_count.load(), initial_count)
        << "Handler should have been called";

    verifyLastException(NIMCP_ERROR_INVALID_PARAMETER, EXCEPTION_SEVERITY_CRITICAL);
}

//=============================================================================
// Test: NIMCP_THROW_FATAL Always Fatal Severity
//=============================================================================

TEST_F(ExceptionMacroFlowTest, NimcpThrowFatalAlwaysFatalSeverity) {
    // WHAT: Verify NIMCP_THROW_FATAL always creates FATAL severity exception
    // WHY:  Fatal exceptions trigger emergency response

    int initial_count = g_macro_test_state.handler_call_count.load();

    // Use fatal macro
    NIMCP_THROW_FATAL(NIMCP_ERROR_MEMORY_CORRUPTION, "Fatal memory corruption detected");

    EXPECT_GT(g_macro_test_state.handler_call_count.load(), initial_count)
        << "Handler should have been called";

    verifyLastException(NIMCP_ERROR_MEMORY_CORRUPTION, EXCEPTION_SEVERITY_FATAL);
}

//=============================================================================
// Test: NIMCP_CHECK_THROW Return Pattern (Simulated)
//=============================================================================

/**
 * @brief Helper function to simulate NIMCP_CHECK_THROW behavior
 */
static nimcp_error_t function_using_check_throw(void* ptr) {
    // This macro both throws AND returns if condition fails
    NIMCP_CHECK_THROW(ptr != NULL, NIMCP_ERROR_NULL_POINTER, "Pointer is NULL");
    return NIMCP_SUCCESS;
}

TEST_F(ExceptionMacroFlowTest, NimcpCheckThrowReturnsOnFailure) {
    // WHAT: Verify NIMCP_CHECK_THROW returns error code after throwing
    // WHY:  Common pattern for input validation

    int initial_count = g_macro_test_state.handler_call_count.load();

    // Call with NULL - should throw and return error
    nimcp_error_t result = function_using_check_throw(NULL);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER)
        << "Function should return error code on NULL pointer";

    EXPECT_GT(g_macro_test_state.handler_call_count.load(), initial_count)
        << "Exception should have been dispatched";

    verifyLastException(NIMCP_ERROR_NULL_POINTER);

    // Clear and call with valid pointer
    g_macro_test_state.received_exceptions.clear();
    initial_count = g_macro_test_state.handler_call_count.load();

    int dummy = 42;
    result = function_using_check_throw(&dummy);

    EXPECT_EQ(result, NIMCP_SUCCESS)
        << "Function should succeed with valid pointer";

    // No new exceptions should have been dispatched
    EXPECT_EQ(g_macro_test_state.handler_call_count.load(), initial_count)
        << "No exception should be thrown for valid pointer";
}

//=============================================================================
// Test: Multiple Throws in Sequence
//=============================================================================

TEST_F(ExceptionMacroFlowTest, MultipleThrowsInSequence) {
    // WHAT: Verify multiple sequential throws all get dispatched
    // WHY:  Real code may throw multiple exceptions during error handling

    int initial_count = g_macro_test_state.handler_call_count.load();

    NIMCP_THROW(NIMCP_ERROR_INVALID_PARAMETER, "First error");
    NIMCP_THROW(NIMCP_ERROR_OUT_OF_RANGE, "Second error");
    NIMCP_THROW(NIMCP_ERROR_INVALID_STATE, "Third error");

    int final_count = g_macro_test_state.handler_call_count.load();
    EXPECT_EQ(final_count - initial_count, 3)
        << "All three exceptions should have been dispatched";

    ASSERT_GE(g_macro_test_state.received_exceptions.size(), 3u);

    // Verify order
    size_t n = g_macro_test_state.received_exceptions.size();
    EXPECT_EQ(g_macro_test_state.received_exceptions[n-3].code, NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(g_macro_test_state.received_exceptions[n-2].code, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_macro_test_state.received_exceptions[n-1].code, NIMCP_ERROR_INVALID_STATE);
}

//=============================================================================
// Test: Exception Contains Source Location
//=============================================================================

TEST_F(ExceptionMacroFlowTest, ExceptionContainsSourceLocation) {
    // WHAT: Verify exceptions created by macros contain __FILE__, __LINE__, __func__
    // WHY:  Source location is critical for debugging

    NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "Test error");

    ASSERT_FALSE(g_macro_test_state.received_exceptions.empty());
    const auto& last = g_macro_test_state.received_exceptions.back();

    // File should contain this test file name
    EXPECT_FALSE(last.file.empty()) << "File should not be empty";
    EXPECT_TRUE(last.file.find("test_exception_macro_flow.cpp") != std::string::npos)
        << "File should contain test file name, got: " << last.file;

    // Line should be positive
    EXPECT_GT(last.line, 0) << "Line number should be positive";

    // Function should not be empty (may vary by compiler)
    EXPECT_FALSE(last.function.empty()) << "Function should not be empty";
}

//=============================================================================
// Test: NIMCP_THROW_TO_IMMUNE_IF Conditional Immune Presentation
//=============================================================================

TEST_F(ExceptionMacroFlowTest, NimcpThrowToImmuneIfConditional) {
    // WHAT: Verify NIMCP_THROW_TO_IMMUNE_IF only throws when condition is false
    // WHY:  Conditional immune presentation pattern

    int initial_count = g_macro_test_state.handler_call_count.load();

    // Condition true - should NOT throw
    bool ok = true;
    NIMCP_THROW_TO_IMMUNE_IF(ok, NIMCP_ERROR_INVALID_STATE, "Should not throw");

    EXPECT_EQ(g_macro_test_state.handler_call_count.load(), initial_count)
        << "Handler should NOT have been called when condition is true";

    // Condition false - SHOULD throw
    ok = false;
    NIMCP_THROW_TO_IMMUNE_IF(ok, NIMCP_ERROR_INVALID_STATE, "Should throw to immune");

    EXPECT_GT(g_macro_test_state.handler_call_count.load(), initial_count)
        << "Handler SHOULD have been called when condition is false";

    // Verify immune presentation flag
    const auto& last = g_macro_test_state.received_exceptions.back();
    EXPECT_TRUE(last.presented_to_immune)
        << "Exception should be marked as presented to immune";
}

//=============================================================================
// Test: NIMCP_CHECK_THROW_IMMUNE Return Pattern with Immune
//=============================================================================

/**
 * @brief Helper function to simulate NIMCP_CHECK_THROW_IMMUNE behavior
 */
static nimcp_error_t function_using_check_throw_immune(size_t size) {
    NIMCP_CHECK_THROW_IMMUNE(size > 0, NIMCP_ERROR_INVALID_PARAMETER,
                              "Size must be positive, got %zu", size);
    return NIMCP_SUCCESS;
}

TEST_F(ExceptionMacroFlowTest, NimcpCheckThrowImmuneReturnsOnFailure) {
    // WHAT: Verify NIMCP_CHECK_THROW_IMMUNE returns and presents to immune
    // WHY:  Combines validation with immune presentation

    int initial_count = g_macro_test_state.handler_call_count.load();

    // Call with zero size - should throw and return
    nimcp_error_t result = function_using_check_throw_immune(0);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAMETER)
        << "Function should return error code on invalid input";

    EXPECT_GT(g_macro_test_state.handler_call_count.load(), initial_count)
        << "Exception should have been dispatched";

    const auto& last = g_macro_test_state.received_exceptions.back();
    EXPECT_TRUE(last.presented_to_immune)
        << "Exception should be marked as presented to immune";
}

//=============================================================================
// Test: Exception Category Derived from Error Code
//=============================================================================

TEST_F(ExceptionMacroFlowTest, ExceptionCategoryDerivedFromErrorCode) {
    // WHAT: Verify exception category is correctly derived from error code range
    // WHY:  Category affects routing to specialized handlers

    // Memory error (2000-2999)
    NIMCP_THROW(NIMCP_ERROR_NO_MEMORY, "Memory error");
    EXPECT_EQ(g_macro_test_state.received_exceptions.back().category, EXCEPTION_CATEGORY_MEMORY);

    // Brain error (3000-3999)
    NIMCP_THROW(NIMCP_ERROR_BRAIN_CREATION, "Brain error");
    EXPECT_EQ(g_macro_test_state.received_exceptions.back().category, EXCEPTION_CATEGORY_BRAIN);

    // I/O error (4000-4999)
    NIMCP_THROW(NIMCP_ERROR_FILE_NOT_FOUND, "I/O error");
    EXPECT_EQ(g_macro_test_state.received_exceptions.back().category, EXCEPTION_CATEGORY_IO);

    // Threading error (6000-6999)
    NIMCP_THROW(NIMCP_ERROR_DEADLOCK, "Threading error");
    EXPECT_EQ(g_macro_test_state.received_exceptions.back().category, EXCEPTION_CATEGORY_THREADING);

    // Signal error (7000-7999)
    NIMCP_THROW(NIMCP_ERROR_SIGSEGV, "Signal error");
    EXPECT_EQ(g_macro_test_state.received_exceptions.back().category, EXCEPTION_CATEGORY_SIGNAL);
}

//=============================================================================
// Test: Format String with Multiple Arguments
//=============================================================================

TEST_F(ExceptionMacroFlowTest, FormatStringWithMultipleArguments) {
    // WHAT: Verify macro handles multiple printf-style arguments
    // WHY:  Real error messages often include multiple values

    const char* name = "test_component";
    int id = 42;
    double value = 3.14159;

    NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED,
                "Component '%s' (id=%d) failed with value=%f",
                name, id, value);

    ASSERT_FALSE(g_macro_test_state.received_exceptions.empty());
    const auto& last = g_macro_test_state.received_exceptions.back();

    // Message should contain all formatted values
    EXPECT_TRUE(last.message.find("test_component") != std::string::npos)
        << "Message should contain component name";
    EXPECT_TRUE(last.message.find("42") != std::string::npos)
        << "Message should contain ID";
    // Note: float formatting may vary
}

//=============================================================================
// Test: Macros Work in Loop
//=============================================================================

TEST_F(ExceptionMacroFlowTest, MacrosWorkInLoop) {
    // WHAT: Verify macros work correctly when called in a loop
    // WHY:  Ensure no macro expansion issues with repeated calls

    int initial_count = g_macro_test_state.handler_call_count.load();

    for (int i = 0; i < 5; i++) {
        NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "Loop iteration %d", i);
    }

    int final_count = g_macro_test_state.handler_call_count.load();
    EXPECT_EQ(final_count - initial_count, 5)
        << "All loop iterations should have thrown";
}

//=============================================================================
// Test: Macros Work in Nested Functions
//=============================================================================

static void inner_function() {
    NIMCP_THROW(NIMCP_ERROR_INVALID_STATE, "Error from inner function");
}

static void outer_function() {
    inner_function();
}

TEST_F(ExceptionMacroFlowTest, MacrosWorkInNestedFunctions) {
    // WHAT: Verify macros work correctly across nested function calls
    // WHY:  Real code has deep call stacks

    int initial_count = g_macro_test_state.handler_call_count.load();

    outer_function();

    EXPECT_GT(g_macro_test_state.handler_call_count.load(), initial_count)
        << "Exception from nested function should be dispatched";

    // Source location should point to inner function
    const auto& last = g_macro_test_state.received_exceptions.back();
    EXPECT_TRUE(last.function.find("inner") != std::string::npos ||
                last.function == "inner_function")
        << "Function should be inner_function, got: " << last.function;
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
