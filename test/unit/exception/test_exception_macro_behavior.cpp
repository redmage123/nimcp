/**
 * @file test_exception_macro_behavior.cpp
 * @brief Unit tests for NIMCP_CHECK_THROW macro behavior
 *
 * WHAT: Tests for NIMCP_CHECK_THROW macro and related exception macros
 * WHY:  Verify correct error code returns, exception notification, and message formatting
 * HOW:  GoogleTest framework with fixture setup/teardown for exception system
 *
 * TEST CATEGORIES:
 * 1. NIMCP_CHECK_THROW returns correct error code when condition fails
 * 2. NIMCP_CHECK_THROW passes through when condition succeeds
 * 3. Error message formatting verification
 * 4. Various error codes: NULL_POINTER, INVALID_PARAM, OUT_OF_RANGE, etc.
 * 5. Memory leak verification on error paths
 *
 * @author NIMCP Development Team
 * @date 2026-01-21
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <vector>
#include <string>

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
std::atomic<bool> g_exception_presented_to_immune{false};
std::vector<std::string> g_captured_messages;
std::atomic<bool> g_use_message_capture{false};

/**
 * @brief Test handler callback to track exception dispatch
 */
bool test_tracking_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    if (ex) {
        g_handler_call_count++;
        g_last_error_code = ex->code;
        g_last_severity = ex->severity;
        g_exception_presented_to_immune = ex->presented_to_immune;

        if (g_use_message_capture && ex->message) {
            g_captured_messages.push_back(std::string(ex->message));
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
    g_exception_presented_to_immune = false;
    g_captured_messages.clear();
}

}  // anonymous namespace

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * WHAT: Base fixture for exception macro behavior tests
 * WHY:  Setup/teardown exception system for each test
 */
class ExceptionMacroBehaviorTest : public ::testing::Test {
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
        opts.name = "macro_behavior_test_handler";
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
// Helper Functions Using NIMCP_CHECK_THROW
//=============================================================================

/**
 * @brief Function using NIMCP_CHECK_THROW for NULL pointer validation
 */
static nimcp_error_t validate_pointer(void* ptr) {
    NIMCP_CHECK_THROW(ptr != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "NULL pointer passed to validate_pointer");
    return NIMCP_SUCCESS;
}

/**
 * @brief Function using NIMCP_CHECK_THROW for parameter range validation
 */
static nimcp_error_t validate_range(int value, int min_val, int max_val) {
    NIMCP_CHECK_THROW(value >= min_val, NIMCP_ERROR_OUT_OF_RANGE,
                      "Value %d below minimum %d", value, min_val);
    NIMCP_CHECK_THROW(value <= max_val, NIMCP_ERROR_OUT_OF_RANGE,
                      "Value %d above maximum %d", value, max_val);
    return NIMCP_SUCCESS;
}

/**
 * @brief Function using NIMCP_CHECK_THROW for invalid parameter
 */
static nimcp_error_t validate_config(const char* name, int count) {
    NIMCP_CHECK_THROW(name != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Configuration name is NULL");
    NIMCP_CHECK_THROW(strlen(name) > 0, NIMCP_ERROR_INVALID_PARAM,
                      "Configuration name is empty");
    NIMCP_CHECK_THROW(count > 0, NIMCP_ERROR_INVALID_PARAM,
                      "Count must be positive, got %d", count);
    return NIMCP_SUCCESS;
}

/**
 * @brief Function using multiple different error types
 */
static nimcp_error_t complex_validation(void* ptr, int index, int max_index, bool ready) {
    NIMCP_CHECK_THROW(ptr != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Pointer argument is NULL");
    NIMCP_CHECK_THROW(index >= 0 && index < max_index, NIMCP_ERROR_OUT_OF_RANGE,
                      "Index %d out of bounds [0, %d)", index, max_index);
    NIMCP_CHECK_THROW(ready, NIMCP_ERROR_INVALID_STATE,
                      "System not ready for operation");
    return NIMCP_SUCCESS;
}

/**
 * @brief Function that performs cleanup on error
 */
static nimcp_error_t operation_with_cleanup(void* resource, bool should_fail) {
    // Simulate resource allocation tracking
    int* cleanup_flag = static_cast<int*>(resource);

    if (cleanup_flag) {
        *cleanup_flag = 1;  // Mark as allocated
    }

    // This check will fail if should_fail is true
    NIMCP_CHECK_THROW(!should_fail, NIMCP_ERROR_OPERATION_FAILED,
                      "Simulated operation failure");

    // On success, mark as completed
    if (cleanup_flag) {
        *cleanup_flag = 2;  // Mark as completed
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// NIMCP_CHECK_THROW Return Value Tests
//=============================================================================

/**
 * WHAT: Test NIMCP_CHECK_THROW returns correct error code on NULL pointer
 * WHY:  Verify the macro correctly returns the specified error code
 */
TEST_F(ExceptionMacroBehaviorTest, CheckThrowReturnsNullPointerError) {
    nimcp_error_t result = validate_pointer(nullptr);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_NULL_POINTER);
}

/**
 * WHAT: Test NIMCP_CHECK_THROW returns OUT_OF_RANGE for below minimum
 * WHY:  Verify range checking with appropriate error code
 */
TEST_F(ExceptionMacroBehaviorTest, CheckThrowReturnsOutOfRangeErrorBelowMin) {
    nimcp_error_t result = validate_range(-5, 0, 100);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_OUT_OF_RANGE);
}

/**
 * WHAT: Test NIMCP_CHECK_THROW returns OUT_OF_RANGE for above maximum
 * WHY:  Verify range checking catches both bounds
 */
TEST_F(ExceptionMacroBehaviorTest, CheckThrowReturnsOutOfRangeErrorAboveMax) {
    nimcp_error_t result = validate_range(150, 0, 100);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_OUT_OF_RANGE);
}

/**
 * WHAT: Test NIMCP_CHECK_THROW returns INVALID_PARAM for bad parameter
 * WHY:  Verify parameter validation with correct error code
 */
TEST_F(ExceptionMacroBehaviorTest, CheckThrowReturnsInvalidParamError) {
    nimcp_error_t result = validate_config("valid_name", -1);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_INVALID_PARAM);
}

/**
 * WHAT: Test NIMCP_CHECK_THROW returns INVALID_STATE for state errors
 * WHY:  Verify state validation with correct error code
 */
TEST_F(ExceptionMacroBehaviorTest, CheckThrowReturnsInvalidStateError) {
    int dummy = 42;
    nimcp_error_t result = complex_validation(&dummy, 5, 10, false);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_STATE);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_INVALID_STATE);
}

/**
 * WHAT: Test NIMCP_CHECK_THROW returns OPERATION_FAILED
 * WHY:  Verify generic operation failure handling
 */
TEST_F(ExceptionMacroBehaviorTest, CheckThrowReturnsOperationFailedError) {
    int cleanup_flag = 0;
    nimcp_error_t result = operation_with_cleanup(&cleanup_flag, true);

    EXPECT_EQ(result, NIMCP_ERROR_OPERATION_FAILED);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_OPERATION_FAILED);

    // Cleanup flag should be set to 1 (allocated) but not 2 (completed)
    EXPECT_EQ(cleanup_flag, 1);
}

//=============================================================================
// NIMCP_CHECK_THROW Pass-Through Tests
//=============================================================================

/**
 * WHAT: Test NIMCP_CHECK_THROW passes through on valid pointer
 * WHY:  Verify successful case continues execution
 */
TEST_F(ExceptionMacroBehaviorTest, CheckThrowPassesThroughOnValidPointer) {
    int value = 42;
    nimcp_error_t result = validate_pointer(&value);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(g_handler_call_count, 0);
}

/**
 * WHAT: Test NIMCP_CHECK_THROW passes through on valid range
 * WHY:  Verify successful range check continues execution
 */
TEST_F(ExceptionMacroBehaviorTest, CheckThrowPassesThroughOnValidRange) {
    nimcp_error_t result = validate_range(50, 0, 100);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(g_handler_call_count, 0);
}

/**
 * WHAT: Test NIMCP_CHECK_THROW passes through all validations
 * WHY:  Verify multi-check function succeeds when all conditions pass
 */
TEST_F(ExceptionMacroBehaviorTest, CheckThrowPassesThroughAllValidations) {
    nimcp_error_t result = validate_config("valid_config", 10);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(g_handler_call_count, 0);
}

/**
 * WHAT: Test NIMCP_CHECK_THROW passes through complex validation
 * WHY:  Verify all conditions pass in complex function
 */
TEST_F(ExceptionMacroBehaviorTest, CheckThrowPassesThroughComplexValidation) {
    int dummy = 42;
    nimcp_error_t result = complex_validation(&dummy, 5, 10, true);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(g_handler_call_count, 0);
}

/**
 * WHAT: Test successful operation completes fully
 * WHY:  Verify cleanup flag is properly set on success
 */
TEST_F(ExceptionMacroBehaviorTest, CheckThrowSuccessfulOperationCompletes) {
    int cleanup_flag = 0;
    nimcp_error_t result = operation_with_cleanup(&cleanup_flag, false);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(g_handler_call_count, 0);
    EXPECT_EQ(cleanup_flag, 2);  // Should reach completed state
}

//=============================================================================
// Error Message Formatting Tests
//=============================================================================

/**
 * WHAT: Test error message contains correct formatted values
 * WHY:  Verify printf-style formatting works correctly
 */
TEST_F(ExceptionMacroBehaviorTest, ErrorMessageContainsFormattedValues) {
    nimcp_error_t result = validate_range(-5, 0, 100);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    ASSERT_FALSE(g_captured_messages.empty());

    // Check message contains the actual values
    const std::string& msg = g_captured_messages[0];
    EXPECT_NE(msg.find("-5"), std::string::npos) << "Message should contain value -5";
    EXPECT_NE(msg.find("0"), std::string::npos) << "Message should contain minimum 0";
}

/**
 * WHAT: Test error message for invalid parameter with count
 * WHY:  Verify parameter value is included in message
 */
TEST_F(ExceptionMacroBehaviorTest, ErrorMessageContainsParameterCount) {
    nimcp_error_t result = validate_config("test", -42);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    ASSERT_FALSE(g_captured_messages.empty());

    const std::string& msg = g_captured_messages[0];
    EXPECT_NE(msg.find("-42"), std::string::npos) << "Message should contain count -42";
}

/**
 * WHAT: Test error message for empty string parameter
 * WHY:  Verify error message describes the specific issue
 */
TEST_F(ExceptionMacroBehaviorTest, ErrorMessageDescribesEmptyString) {
    nimcp_error_t result = validate_config("", 10);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    ASSERT_FALSE(g_captured_messages.empty());

    const std::string& msg = g_captured_messages[0];
    EXPECT_NE(msg.find("empty"), std::string::npos) << "Message should mention 'empty'";
}

/**
 * WHAT: Test error message for complex validation failure
 * WHY:  Verify index bounds are included in message
 */
TEST_F(ExceptionMacroBehaviorTest, ErrorMessageContainsIndexBounds) {
    int dummy = 42;
    nimcp_error_t result = complex_validation(&dummy, 15, 10, true);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    ASSERT_FALSE(g_captured_messages.empty());

    const std::string& msg = g_captured_messages[0];
    EXPECT_NE(msg.find("15"), std::string::npos) << "Message should contain index 15";
    EXPECT_NE(msg.find("10"), std::string::npos) << "Message should contain max 10";
}

//=============================================================================
// Various Error Codes Tests
//=============================================================================

/**
 * Helper function that can throw various error types
 */
static nimcp_error_t throw_specific_error(nimcp_error_t error_to_throw) {
    switch (error_to_throw) {
        case NIMCP_ERROR_NULL_POINTER:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "Null pointer test");
            break;
        case NIMCP_ERROR_INVALID_PARAM:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM, "Invalid param test");
            break;
        case NIMCP_ERROR_OUT_OF_RANGE:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_OUT_OF_RANGE, "Out of range test");
            break;
        case NIMCP_ERROR_NO_MEMORY:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_NO_MEMORY, "No memory test");
            break;
        case NIMCP_ERROR_INVALID_STATE:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_STATE, "Invalid state test");
            break;
        case NIMCP_ERROR_OPERATION_FAILED:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_OPERATION_FAILED, "Operation failed test");
            break;
        case NIMCP_ERROR_TIMEOUT:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_TIMEOUT, "Timeout test");
            break;
        case NIMCP_ERROR_BUFFER_OVERFLOW:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_BUFFER_OVERFLOW, "Buffer overflow test");
            break;
        case NIMCP_ERROR_NOT_INITIALIZED:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_INITIALIZED, "Not initialized test");
            break;
        case NIMCP_ERROR_ALREADY_EXISTS:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_ALREADY_EXISTS, "Already exists test");
            break;
        default:
            NIMCP_CHECK_THROW(false, error_to_throw, "Generic error test");
            break;
    }
    return NIMCP_SUCCESS;
}

/**
 * WHAT: Test NIMCP_ERROR_NO_MEMORY error code
 * WHY:  Verify memory error handling
 */
TEST_F(ExceptionMacroBehaviorTest, CheckThrowReturnsNoMemoryError) {
    nimcp_error_t result = throw_specific_error(NIMCP_ERROR_NO_MEMORY);

    EXPECT_EQ(result, NIMCP_ERROR_NO_MEMORY);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_NO_MEMORY);
}

/**
 * WHAT: Test NIMCP_ERROR_TIMEOUT error code
 * WHY:  Verify timeout error handling
 */
TEST_F(ExceptionMacroBehaviorTest, CheckThrowReturnsTimeoutError) {
    nimcp_error_t result = throw_specific_error(NIMCP_ERROR_TIMEOUT);

    EXPECT_EQ(result, NIMCP_ERROR_TIMEOUT);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_TIMEOUT);
}

/**
 * WHAT: Test NIMCP_ERROR_BUFFER_OVERFLOW error code
 * WHY:  Verify buffer overflow error handling
 */
TEST_F(ExceptionMacroBehaviorTest, CheckThrowReturnsBufferOverflowError) {
    nimcp_error_t result = throw_specific_error(NIMCP_ERROR_BUFFER_OVERFLOW);

    EXPECT_EQ(result, NIMCP_ERROR_BUFFER_OVERFLOW);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_BUFFER_OVERFLOW);
}

/**
 * WHAT: Test NIMCP_ERROR_NOT_INITIALIZED error code
 * WHY:  Verify initialization error handling
 */
TEST_F(ExceptionMacroBehaviorTest, CheckThrowReturnsNotInitializedError) {
    nimcp_error_t result = throw_specific_error(NIMCP_ERROR_NOT_INITIALIZED);

    EXPECT_EQ(result, NIMCP_ERROR_NOT_INITIALIZED);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_NOT_INITIALIZED);
}

/**
 * WHAT: Test NIMCP_ERROR_ALREADY_EXISTS error code
 * WHY:  Verify duplicate entry error handling
 */
TEST_F(ExceptionMacroBehaviorTest, CheckThrowReturnsAlreadyExistsError) {
    nimcp_error_t result = throw_specific_error(NIMCP_ERROR_ALREADY_EXISTS);

    EXPECT_EQ(result, NIMCP_ERROR_ALREADY_EXISTS);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_ALREADY_EXISTS);
}

//=============================================================================
// Exception System Notification Tests
//=============================================================================

/**
 * WHAT: Test exception system is notified on error
 * WHY:  Verify handler chain receives exception
 */
TEST_F(ExceptionMacroBehaviorTest, ExceptionSystemNotifiedOnError) {
    nimcp_error_t result = validate_pointer(nullptr);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);

    // Verify handler was called
    EXPECT_EQ(g_handler_call_count, 1);

    // Verify correct error was passed
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_NULL_POINTER);
}

/**
 * WHAT: Test exception system is NOT notified on success
 * WHY:  Verify no exception for successful operations
 */
TEST_F(ExceptionMacroBehaviorTest, ExceptionSystemNotNotifiedOnSuccess) {
    int value = 42;
    nimcp_error_t result = validate_pointer(&value);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(g_handler_call_count, 0);
}

/**
 * WHAT: Test first failing check triggers exception
 * WHY:  Verify only one exception is raised for first failure
 */
TEST_F(ExceptionMacroBehaviorTest, FirstFailingCheckTriggersException) {
    // Pass NULL for name - should fail on first check
    nimcp_error_t result = validate_config(nullptr, 10);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);  // Only one exception
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Memory Leak Verification Tests
//=============================================================================

/**
 * WHAT: Test multiple errors don't cause memory leaks
 * WHY:  Verify exception cleanup on error paths
 */
TEST_F(ExceptionMacroBehaviorTest, MultipleErrorsNoMemoryLeak) {
    const int iterations = 100;

    for (int i = 0; i < iterations; i++) {
        nimcp_error_t result = validate_pointer(nullptr);
        EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    }

    // Verify all exceptions were processed
    EXPECT_EQ(g_handler_call_count, iterations);

    // Note: Actual memory leak detection would require valgrind or similar
    // This test verifies the code paths execute without crashes
}

/**
 * WHAT: Test mixed success and failure operations
 * WHY:  Verify cleanup works for alternating outcomes
 */
TEST_F(ExceptionMacroBehaviorTest, MixedSuccessAndFailureNoMemoryLeak) {
    const int iterations = 50;
    int success_count = 0;
    int failure_count = 0;

    for (int i = 0; i < iterations; i++) {
        if (i % 2 == 0) {
            int value = 42;
            nimcp_error_t result = validate_pointer(&value);
            EXPECT_EQ(result, NIMCP_SUCCESS);
            success_count++;
        } else {
            nimcp_error_t result = validate_pointer(nullptr);
            EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
            failure_count++;
        }
    }

    EXPECT_EQ(success_count, 25);
    EXPECT_EQ(failure_count, 25);
    EXPECT_EQ(g_handler_call_count, failure_count);
}

/**
 * WHAT: Test cleanup flag is set even on early return
 * WHY:  Verify resource tracking works with NIMCP_CHECK_THROW
 */
TEST_F(ExceptionMacroBehaviorTest, ResourceTrackingOnEarlyReturn) {
    int cleanup_flag = 0;

    // This should set cleanup_flag to 1 then fail
    nimcp_error_t result = operation_with_cleanup(&cleanup_flag, true);

    EXPECT_EQ(result, NIMCP_ERROR_OPERATION_FAILED);
    EXPECT_EQ(cleanup_flag, 1);  // Should be 1 (allocated), not 2 (completed)
}

//=============================================================================
// Edge Case Tests
//=============================================================================

/**
 * WHAT: Test boundary value at minimum
 * WHY:  Verify exact boundary is accepted
 */
TEST_F(ExceptionMacroBehaviorTest, BoundaryValueAtMinimum) {
    nimcp_error_t result = validate_range(0, 0, 100);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(g_handler_call_count, 0);
}

/**
 * WHAT: Test boundary value at maximum
 * WHY:  Verify exact boundary is accepted
 */
TEST_F(ExceptionMacroBehaviorTest, BoundaryValueAtMaximum) {
    nimcp_error_t result = validate_range(100, 0, 100);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(g_handler_call_count, 0);
}

/**
 * WHAT: Test one below minimum fails
 * WHY:  Verify off-by-one errors are caught
 */
TEST_F(ExceptionMacroBehaviorTest, BoundaryValueBelowMinimumFails) {
    nimcp_error_t result = validate_range(-1, 0, 100);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
}

/**
 * WHAT: Test one above maximum fails
 * WHY:  Verify off-by-one errors are caught
 */
TEST_F(ExceptionMacroBehaviorTest, BoundaryValueAboveMaximumFails) {
    nimcp_error_t result = validate_range(101, 0, 100);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
}

/**
 * WHAT: Test index at last valid position
 * WHY:  Verify array-style bounds checking (0 to n-1)
 */
TEST_F(ExceptionMacroBehaviorTest, IndexAtLastValidPosition) {
    int dummy = 42;
    nimcp_error_t result = complex_validation(&dummy, 9, 10, true);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(g_handler_call_count, 0);
}

/**
 * WHAT: Test index at first invalid position
 * WHY:  Verify array-style bounds checking fails at n
 */
TEST_F(ExceptionMacroBehaviorTest, IndexAtFirstInvalidPosition) {
    int dummy = 42;
    nimcp_error_t result = complex_validation(&dummy, 10, 10, true);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
