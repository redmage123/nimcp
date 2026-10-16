/**
 * @file test_exception_error_codes.cpp
 * @brief Regression tests for exception error code behavior
 * @date 2026-01-21
 *
 * Tests to verify:
 * - Correct error codes are returned from functions
 * - Error code mappings are consistent
 * - Exception macro behavior matches expected error returns
 * - Edge cases in error handling work correctly
 *
 * These tests ensure that after the exception macro integration,
 * all functions continue to return the documented error codes.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <vector>
#include <thread>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/exception/nimcp_exception_circuit.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ExceptionErrorCodesTest : public ::testing::Test {
protected:
    void SetUp() override {
        int result = nimcp_exception_system_init();
        ASSERT_EQ(result, 0);
    }

    void TearDown() override {
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }
};

/* ============================================================================
 * Error Code Range Validation
 *
 * Verify error codes fall within their documented ranges
 * ============================================================================ */

TEST_F(ExceptionErrorCodesTest, ErrorCodeRanges_GenericInRange) {
    // Generic errors should be in 1000-1999
    EXPECT_GE(NIMCP_ERROR_UNKNOWN, 1000);
    EXPECT_LT(NIMCP_ERROR_UNKNOWN, 2000);

    EXPECT_GE(NIMCP_ERROR_NOT_IMPLEMENTED, 1000);
    EXPECT_LT(NIMCP_ERROR_NOT_IMPLEMENTED, 2000);

    EXPECT_GE(NIMCP_ERROR_INVALID_PARAMETER, 1000);
    EXPECT_LT(NIMCP_ERROR_INVALID_PARAMETER, 2000);

    EXPECT_GE(NIMCP_ERROR_NULL_POINTER, 1000);
    EXPECT_LT(NIMCP_ERROR_NULL_POINTER, 2000);

    EXPECT_GE(NIMCP_ERROR_OUT_OF_RANGE, 1000);
    EXPECT_LT(NIMCP_ERROR_OUT_OF_RANGE, 2000);
}

TEST_F(ExceptionErrorCodesTest, ErrorCodeRanges_MemoryInRange) {
    // Memory errors should be in 2000-2999
    EXPECT_GE(NIMCP_ERROR_NO_MEMORY, 2000);
    EXPECT_LT(NIMCP_ERROR_NO_MEMORY, 3000);

    EXPECT_GE(NIMCP_ERROR_BUFFER_TOO_SMALL, 2000);
    EXPECT_LT(NIMCP_ERROR_BUFFER_TOO_SMALL, 3000);

    EXPECT_GE(NIMCP_ERROR_BUFFER_OVERFLOW, 2000);
    EXPECT_LT(NIMCP_ERROR_BUFFER_OVERFLOW, 3000);

    EXPECT_GE(NIMCP_ERROR_MEMORY_CORRUPTION, 2000);
    EXPECT_LT(NIMCP_ERROR_MEMORY_CORRUPTION, 3000);
}

TEST_F(ExceptionErrorCodesTest, ErrorCodeRanges_BrainInRange) {
    // Brain/Network errors should be in 3000-3999
    EXPECT_GE(NIMCP_ERROR_BRAIN_CREATION, 3000);
    EXPECT_LT(NIMCP_ERROR_BRAIN_CREATION, 4000);

    EXPECT_GE(NIMCP_ERROR_BRAIN_INVALID, 3000);
    EXPECT_LT(NIMCP_ERROR_BRAIN_INVALID, 4000);

    EXPECT_GE(NIMCP_ERROR_FORWARD_PASS, 3000);
    EXPECT_LT(NIMCP_ERROR_FORWARD_PASS, 4000);
}

TEST_F(ExceptionErrorCodesTest, ErrorCodeRanges_IOInRange) {
    // I/O errors should be in 4000-4999
    EXPECT_GE(NIMCP_ERROR_FILE_NOT_FOUND, 4000);
    EXPECT_LT(NIMCP_ERROR_FILE_NOT_FOUND, 5000);

    EXPECT_GE(NIMCP_ERROR_FILE_READ, 4000);
    EXPECT_LT(NIMCP_ERROR_FILE_READ, 5000);

    EXPECT_GE(NIMCP_ERROR_FILE_WRITE, 4000);
    EXPECT_LT(NIMCP_ERROR_FILE_WRITE, 5000);
}

TEST_F(ExceptionErrorCodesTest, ErrorCodeRanges_ConfigInRange) {
    // Config errors should be in 5000-5999
    EXPECT_GE(NIMCP_ERROR_CONFIG_INVALID, 5000);
    EXPECT_LT(NIMCP_ERROR_CONFIG_INVALID, 6000);

    EXPECT_GE(NIMCP_ERROR_CONFIG_PARSE, 5000);
    EXPECT_LT(NIMCP_ERROR_CONFIG_PARSE, 6000);
}

TEST_F(ExceptionErrorCodesTest, ErrorCodeRanges_ThreadingInRange) {
    // Threading errors should be in 6000-6999
    EXPECT_GE(NIMCP_ERROR_THREAD_CREATE, 6000);
    EXPECT_LT(NIMCP_ERROR_THREAD_CREATE, 7000);

    EXPECT_GE(NIMCP_ERROR_MUTEX_LOCK, 6000);
    EXPECT_LT(NIMCP_ERROR_MUTEX_LOCK, 7000);

    EXPECT_GE(NIMCP_ERROR_DEADLOCK, 6000);
    EXPECT_LT(NIMCP_ERROR_DEADLOCK, 7000);
}

TEST_F(ExceptionErrorCodesTest, ErrorCodeRanges_SignalInRange) {
    // Signal errors should be in 7000-7999
    EXPECT_GE(NIMCP_ERROR_SIGNAL_RECEIVED, 7000);
    EXPECT_LT(NIMCP_ERROR_SIGNAL_RECEIVED, 8000);

    EXPECT_GE(NIMCP_ERROR_SIGSEGV, 7000);
    EXPECT_LT(NIMCP_ERROR_SIGSEGV, 8000);

    EXPECT_GE(NIMCP_ERROR_SIGFPE, 7000);
    EXPECT_LT(NIMCP_ERROR_SIGFPE, 8000);
}

TEST_F(ExceptionErrorCodesTest, ErrorCodeRanges_SecurityInRange) {
    // Security errors should be in 9000-9099
    EXPECT_GE(NIMCP_ERROR_SECURITY_BASE, 9000);
    EXPECT_LT(NIMCP_ERROR_SECURITY_BASE, 10000);

    EXPECT_GE(NIMCP_ERROR_BBB_REJECTED, 9000);
    EXPECT_LT(NIMCP_ERROR_BBB_REJECTED, 10000);

    EXPECT_GE(NIMCP_ERROR_SECURITY_THREAT, 9000);
    EXPECT_LT(NIMCP_ERROR_SECURITY_THREAT, 10000);
}

/* ============================================================================
 * Error Code to Category Mapping
 * ============================================================================ */

TEST_F(ExceptionErrorCodesTest, ErrorToCategory_Generic) {
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_UNKNOWN), EXCEPTION_CATEGORY_GENERIC);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_INVALID_PARAMETER), EXCEPTION_CATEGORY_GENERIC);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_NULL_POINTER), EXCEPTION_CATEGORY_GENERIC);
}

TEST_F(ExceptionErrorCodesTest, ErrorToCategory_Memory) {
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_NO_MEMORY), EXCEPTION_CATEGORY_MEMORY);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_BUFFER_OVERFLOW), EXCEPTION_CATEGORY_MEMORY);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_MEMORY_CORRUPTION), EXCEPTION_CATEGORY_MEMORY);
}

TEST_F(ExceptionErrorCodesTest, ErrorToCategory_Brain) {
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_BRAIN_CREATION), EXCEPTION_CATEGORY_BRAIN);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_FORWARD_PASS), EXCEPTION_CATEGORY_BRAIN);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_LEARNING_FAILED), EXCEPTION_CATEGORY_BRAIN);
}

TEST_F(ExceptionErrorCodesTest, ErrorToCategory_IO) {
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_FILE_NOT_FOUND), EXCEPTION_CATEGORY_IO);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_FILE_READ), EXCEPTION_CATEGORY_IO);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_NETWORK_IO), EXCEPTION_CATEGORY_IO);
}

TEST_F(ExceptionErrorCodesTest, ErrorToCategory_Threading) {
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_THREAD_CREATE), EXCEPTION_CATEGORY_THREADING);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_DEADLOCK), EXCEPTION_CATEGORY_THREADING);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_MUTEX_LOCK), EXCEPTION_CATEGORY_THREADING);
}

TEST_F(ExceptionErrorCodesTest, ErrorToCategory_Signal) {
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_SIGSEGV), EXCEPTION_CATEGORY_SIGNAL);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_SIGABRT), EXCEPTION_CATEGORY_SIGNAL);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_SIGFPE), EXCEPTION_CATEGORY_SIGNAL);
}

/* ============================================================================
 * Error Code to Severity Mapping
 * ============================================================================ */

TEST_F(ExceptionErrorCodesTest, ErrorToSeverity_CriticalSignals) {
    // Signal errors should be CRITICAL or FATAL
    nimcp_exception_severity_t sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_SIGSEGV);
    EXPECT_GE(sev, EXCEPTION_SEVERITY_CRITICAL);

    sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_SIGABRT);
    EXPECT_GE(sev, EXCEPTION_SEVERITY_CRITICAL);
}

TEST_F(ExceptionErrorCodesTest, ErrorToSeverity_SevereMemoryErrors) {
    // Memory errors should be at least SEVERE
    nimcp_exception_severity_t sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_NO_MEMORY);
    EXPECT_GE(sev, EXCEPTION_SEVERITY_SEVERE);

    sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_MEMORY_CORRUPTION);
    EXPECT_GE(sev, EXCEPTION_SEVERITY_SEVERE);
}

TEST_F(ExceptionErrorCodesTest, ErrorToSeverity_ModerateErrors) {
    // Parameter errors should be ERROR or lower
    nimcp_exception_severity_t sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_LE(sev, EXCEPTION_SEVERITY_ERROR);

    sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_NOT_FOUND);
    EXPECT_LE(sev, EXCEPTION_SEVERITY_ERROR);
}

/* ============================================================================
 * Exception Creation with Error Codes
 * ============================================================================ */

TEST_F(ExceptionErrorCodesTest, ExceptionCreate_PreservesErrorCode) {
    const nimcp_error_t test_codes[] = {
        NIMCP_ERROR_UNKNOWN,
        NIMCP_ERROR_NO_MEMORY,
        NIMCP_ERROR_BRAIN_CREATION,
        NIMCP_ERROR_FILE_NOT_FOUND,
        NIMCP_ERROR_DEADLOCK,
        NIMCP_ERROR_SIGSEGV
    };

    for (nimcp_error_t code : test_codes) {
        nimcp_exception_t* ex = nimcp_exception_create(
            code,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Test for code %d", code
        );

        ASSERT_NE(ex, nullptr) << "Failed to create exception for code " << code;
        EXPECT_EQ(ex->code, code) << "Code mismatch for " << code;

        nimcp_exception_unref(ex);
    }
}

TEST_F(ExceptionErrorCodesTest, SpecializedExceptions_PreserveErrorCodes) {
    // Memory exception
    nimcp_memory_exception_t* mex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1024,
        "Memory test"
    );
    EXPECT_EQ(mex->base.code, NIMCP_ERROR_NO_MEMORY);
    nimcp_exception_unref((nimcp_exception_t*)mex);

    // Brain exception
    nimcp_brain_exception_t* bex = nimcp_brain_exception_create(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1, "test",
        "Brain test"
    );
    EXPECT_EQ(bex->base.code, NIMCP_ERROR_FORWARD_PASS);
    nimcp_exception_unref((nimcp_exception_t*)bex);

    // IO exception
    nimcp_io_exception_t* iex = nimcp_io_exception_create(
        NIMCP_ERROR_FILE_READ,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "/test/path",
        "IO test"
    );
    EXPECT_EQ(iex->base.code, NIMCP_ERROR_FILE_READ);
    nimcp_exception_unref((nimcp_exception_t*)iex);

    // Threading exception
    nimcp_threading_exception_t* tex = nimcp_threading_exception_create(
        NIMCP_ERROR_DEADLOCK,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        12345,
        "Threading test"
    );
    EXPECT_EQ(tex->base.code, NIMCP_ERROR_DEADLOCK);
    nimcp_exception_unref((nimcp_exception_t*)tex);
}

/* ============================================================================
 * Error String Mapping
 * ============================================================================ */

TEST_F(ExceptionErrorCodesTest, ErrorToString_GenericErrors) {
    EXPECT_NE(nimcp_error_to_string(NIMCP_ERROR_UNKNOWN), nullptr);
    EXPECT_NE(nimcp_error_to_string(NIMCP_ERROR_NOT_IMPLEMENTED), nullptr);
    EXPECT_NE(nimcp_error_to_string(NIMCP_ERROR_INVALID_PARAMETER), nullptr);
    EXPECT_NE(nimcp_error_to_string(NIMCP_ERROR_NULL_POINTER), nullptr);
}

TEST_F(ExceptionErrorCodesTest, ErrorToString_MemoryErrors) {
    const char* msg = nimcp_error_to_string(NIMCP_ERROR_NO_MEMORY);
    EXPECT_NE(msg, nullptr);
    EXPECT_GT(strlen(msg), 0UL);
}

TEST_F(ExceptionErrorCodesTest, ErrorToString_BrainErrors) {
    const char* msg = nimcp_error_to_string(NIMCP_ERROR_BRAIN_CREATION);
    EXPECT_NE(msg, nullptr);
    EXPECT_GT(strlen(msg), 0UL);
}

TEST_F(ExceptionErrorCodesTest, ErrorToString_UnknownCode) {
    // Unknown codes should return a non-null string (perhaps "Unknown error")
    const char* msg = nimcp_error_to_string(99999);
    EXPECT_NE(msg, nullptr);
}

/* ============================================================================
 * Error Category Name Mapping
 * ============================================================================ */

TEST_F(ExceptionErrorCodesTest, CategoryName_AllRanges) {
    const char* name;

    name = nimcp_error_get_category_name(NIMCP_ERROR_UNKNOWN);
    EXPECT_NE(name, nullptr);

    name = nimcp_error_get_category_name(NIMCP_ERROR_NO_MEMORY);
    EXPECT_NE(name, nullptr);

    name = nimcp_error_get_category_name(NIMCP_ERROR_BRAIN_CREATION);
    EXPECT_NE(name, nullptr);

    name = nimcp_error_get_category_name(NIMCP_ERROR_FILE_NOT_FOUND);
    EXPECT_NE(name, nullptr);

    name = nimcp_error_get_category_name(NIMCP_ERROR_THREAD_CREATE);
    EXPECT_NE(name, nullptr);

    name = nimcp_error_get_category_name(NIMCP_ERROR_SIGSEGV);
    EXPECT_NE(name, nullptr);
}

/* ============================================================================
 * Success/Failure Detection Helpers
 * ============================================================================ */

TEST_F(ExceptionErrorCodesTest, IsSuccess_Validation) {
    // Success codes (0-999)
    EXPECT_TRUE(nimcp_error_is_success(NIMCP_SUCCESS));
    EXPECT_TRUE(nimcp_error_is_success(NIMCP_SUCCESS_WITH_WARNINGS));
    EXPECT_TRUE(nimcp_error_is_success(NIMCP_SUCCESS_PARTIAL));
    EXPECT_TRUE(nimcp_error_is_success(0));
    EXPECT_TRUE(nimcp_error_is_success(500));
    EXPECT_TRUE(nimcp_error_is_success(999));

    // Failure codes (1000+)
    EXPECT_FALSE(nimcp_error_is_success(1000));
    EXPECT_FALSE(nimcp_error_is_success(NIMCP_ERROR_UNKNOWN));
    EXPECT_FALSE(nimcp_error_is_success(NIMCP_ERROR_NO_MEMORY));
}

TEST_F(ExceptionErrorCodesTest, IsFailure_Validation) {
    // Success codes should not be failures
    EXPECT_FALSE(nimcp_error_is_failure(NIMCP_SUCCESS));
    EXPECT_FALSE(nimcp_error_is_failure(0));
    EXPECT_FALSE(nimcp_error_is_failure(999));

    // Failure codes
    EXPECT_TRUE(nimcp_error_is_failure(1000));
    EXPECT_TRUE(nimcp_error_is_failure(NIMCP_ERROR_UNKNOWN));
    EXPECT_TRUE(nimcp_error_is_failure(NIMCP_ERROR_NO_MEMORY));
    EXPECT_TRUE(nimcp_error_is_failure(NIMCP_ERROR_SIGSEGV));
}

TEST_F(ExceptionErrorCodesTest, GetCategory_FromCode) {
    EXPECT_EQ(nimcp_error_get_category(NIMCP_SUCCESS), 0);
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_UNKNOWN), 1);
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_NO_MEMORY), 2);
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_BRAIN_CREATION), 3);
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_FILE_NOT_FOUND), 4);
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_CONFIG_INVALID), 5);
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_THREAD_CREATE), 6);
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_SIGSEGV), 7);
}

/* ============================================================================
 * Brain Region Error Codes
 * ============================================================================ */

TEST_F(ExceptionErrorCodesTest, BrainRegionErrors_Ranges) {
    // Motor cortex (10000-10099)
    EXPECT_EQ(NIMCP_ERROR_MOTOR_BASE, 10000);
    EXPECT_GE(NIMCP_ERROR_MOTOR_PLANNING, 10000);
    EXPECT_LT(NIMCP_ERROR_MOTOR_PLANNING, 10100);

    // Hippocampus (10100-10199)
    EXPECT_EQ(NIMCP_ERROR_HIPPOCAMPUS_BASE, 10100);
    EXPECT_GE(NIMCP_ERROR_HIPPOCAMPUS_ENCODING, 10100);
    EXPECT_LT(NIMCP_ERROR_HIPPOCAMPUS_ENCODING, 10200);

    // Prefrontal (10300-10399)
    EXPECT_EQ(NIMCP_ERROR_PREFRONTAL_BASE, 10300);
    EXPECT_GE(NIMCP_ERROR_PREFRONTAL_PLANNING, 10300);
    EXPECT_LT(NIMCP_ERROR_PREFRONTAL_PLANNING, 10400);
}

TEST_F(ExceptionErrorCodesTest, BrainRegionErrors_IsBrainRegion) {
    EXPECT_TRUE(nimcp_error_is_brain_region(NIMCP_ERROR_MOTOR_BASE));
    EXPECT_TRUE(nimcp_error_is_brain_region(NIMCP_ERROR_HIPPOCAMPUS_ENCODING));
    EXPECT_TRUE(nimcp_error_is_brain_region(NIMCP_ERROR_PREFRONTAL_PLANNING));

    EXPECT_FALSE(nimcp_error_is_brain_region(NIMCP_SUCCESS));
    EXPECT_FALSE(nimcp_error_is_brain_region(NIMCP_ERROR_UNKNOWN));
    EXPECT_FALSE(nimcp_error_is_brain_region(NIMCP_ERROR_NO_MEMORY));
}

TEST_F(ExceptionErrorCodesTest, BrainRegionErrors_Converters) {
    // Motor error conversion
    nimcp_error_t unified = motor_error_to_nimcp(2);  // MOTOR_ERROR_PLANNING
    EXPECT_EQ(unified, NIMCP_ERROR_MOTOR_BASE + 2);

    int local = nimcp_to_motor_error(NIMCP_ERROR_MOTOR_PLANNING);
    EXPECT_EQ(local, 2);

    // Success case
    EXPECT_EQ(motor_error_to_nimcp(0), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_to_motor_error(NIMCP_SUCCESS), 0);
}

/* ============================================================================
 * Signal-to-Error Code Mapping
 * ============================================================================ */

TEST_F(ExceptionErrorCodesTest, SignalToError_Mapping) {
    EXPECT_EQ(nimcp_signal_to_error_code(SIGSEGV), NIMCP_ERROR_SIGSEGV);
    EXPECT_EQ(nimcp_signal_to_error_code(SIGABRT), NIMCP_ERROR_SIGABRT);
    EXPECT_EQ(nimcp_signal_to_error_code(SIGFPE), NIMCP_ERROR_SIGFPE);
#ifdef SIGBUS
    EXPECT_EQ(nimcp_signal_to_error_code(SIGBUS), NIMCP_ERROR_SIGBUS);
#endif
    EXPECT_EQ(nimcp_signal_to_error_code(SIGILL), NIMCP_ERROR_SIGILL);
}

TEST_F(ExceptionErrorCodesTest, SignalName_Mapping) {
    const char* name;

    name = nimcp_signal_name(SIGSEGV);
    EXPECT_NE(name, nullptr);
    EXPECT_TRUE(strstr(name, "SEGV") != nullptr || strstr(name, "segv") != nullptr);

    name = nimcp_signal_name(SIGFPE);
    EXPECT_NE(name, nullptr);
}

/* ============================================================================
 * FEP Bridge Error Conversion
 * ============================================================================ */

TEST_F(ExceptionErrorCodesTest, FEPBridge_ToNimcp) {
    // FEP bridges return 0 for success, -1 for error
    EXPECT_EQ(nimcp_from_fep_result(0), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_from_fep_result(-1), NIMCP_ERROR_OPERATION_FAILED);
}

TEST_F(ExceptionErrorCodesTest, FEPBridge_FromNimcp) {
    EXPECT_EQ(nimcp_to_fep_result(NIMCP_SUCCESS), 0);
    EXPECT_EQ(nimcp_to_fep_result(NIMCP_SUCCESS_WITH_WARNINGS), 0);

    EXPECT_EQ(nimcp_to_fep_result(NIMCP_ERROR_UNKNOWN), -1);
    EXPECT_EQ(nimcp_to_fep_result(NIMCP_ERROR_NO_MEMORY), -1);
    EXPECT_EQ(nimcp_to_fep_result(NIMCP_ERROR_OPERATION_FAILED), -1);
}

/* ============================================================================
 * Handler Error Return Tests
 * ============================================================================ */

TEST_F(ExceptionErrorCodesTest, HandlerRegister_ErrorCodes) {
    // NULL options should return NULL (not an error code, but documented behavior)
    EXPECT_EQ(nimcp_handler_register(nullptr), nullptr);

    // NULL handler callback should return NULL
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.handler = nullptr;
    EXPECT_EQ(nimcp_handler_register(&opts), nullptr);
}

TEST_F(ExceptionErrorCodesTest, HandlerUnregister_ErrorCodes) {
    // Unregister NULL should return error
    EXPECT_NE(nimcp_handler_unregister(nullptr), 0);
}

/* ============================================================================
 * Recovery Callback Error Return Tests
 * ============================================================================ */

static int test_recovery_cb(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
    (void)ex; (void)action; (void)user_data;
    return 0;  // Success
}

TEST_F(ExceptionErrorCodesTest, RecoveryCallback_RegistrationErrors) {
    // Register valid callback
    int result = nimcp_register_recovery_callback(EXCEPTION_RECOVERY_GC, test_recovery_cb, nullptr);
    EXPECT_EQ(result, 0);

    // Unregister
    result = nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_GC);
    EXPECT_EQ(result, 0);
}

TEST_F(ExceptionErrorCodesTest, ExecuteRecovery_NoCallback) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Test"
    );

    // No callback registered for COMPACT
    int result = nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_COMPACT);
    EXPECT_EQ(result, -1);  // Should fail - no callback

    nimcp_exception_unref(ex);
}

/* ============================================================================
 * Exception Context Error Codes
 * ============================================================================ */

TEST_F(ExceptionErrorCodesTest, ExceptionContext_NullParams) {
    // NULL exception
    EXPECT_EQ(nimcp_exception_set_context(nullptr, "key", "value"), -1);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test"
    );

    // NULL key
    EXPECT_EQ(nimcp_exception_set_context(ex, nullptr, "value"), -1);

    // NULL value - might be allowed depending on implementation
    // Just verify it doesn't crash
    nimcp_exception_set_context(ex, "key", nullptr);

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionErrorCodesTest, ExceptionContext_RemoveNonexistent) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test"
    );

    // Remove key that doesn't exist
    int result = nimcp_exception_remove_context(ex, "nonexistent");
    EXPECT_EQ(result, -1);

    nimcp_exception_unref(ex);
}

/* ============================================================================
 * Aggregate Exception Error Codes
 * ============================================================================ */

TEST_F(ExceptionErrorCodesTest, AggregateException_AddNull) {
    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test"
    );

    // Add NULL child
    int result = nimcp_aggregate_exception_add(agg, nullptr);
    EXPECT_EQ(result, -1);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

TEST_F(ExceptionErrorCodesTest, AggregateException_AddToNull) {
    nimcp_exception_t* child = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Child"
    );

    // Add to NULL aggregate
    int result = nimcp_aggregate_exception_add(nullptr, child);
    EXPECT_EQ(result, -1);

    nimcp_exception_unref(child);
}

TEST_F(ExceptionErrorCodesTest, AggregateException_MaxChildren) {
    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test"
    );

    // Fill to max
    for (size_t i = 0; i < NIMCP_EXCEPTION_MAX_CHILDREN; i++) {
        nimcp_exception_t* child = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Child %zu", i
        );

        int result = nimcp_aggregate_exception_add(agg, child);
        EXPECT_EQ(result, 0) << "Failed to add child " << i;
    }

    // Should be at max now
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), (size_t)NIMCP_EXCEPTION_MAX_CHILDREN);

    // Try to add one more - should fail
    nimcp_exception_t* extra = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Extra"
    );

    int result = nimcp_aggregate_exception_add(agg, extra);
    EXPECT_EQ(result, -1);

    nimcp_exception_unref(extra);
    nimcp_exception_unref((nimcp_exception_t*)agg);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
