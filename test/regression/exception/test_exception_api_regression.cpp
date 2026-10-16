/**
 * @file test_exception_api_regression.cpp
 * @brief Regression tests for exception API contracts
 *
 * WHAT: Verify API contracts for exception functions remain stable
 * WHY:  Prevent breaking changes to public exception API
 * HOW:  Test exact values, function signatures, and return behaviors
 *
 * REGRESSION CATEGORIES:
 * 1. Enum Value Stability - Enum values must never change
 * 2. Constant Stability - Defined constants must remain stable
 * 3. Function Return Contracts - Return types and error conditions
 * 4. Error Code Mapping - Error codes must map consistently
 * 5. String Conversion - String representations must be stable
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdint>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/exception/nimcp_exception_circuit.h"
#include "utils/exception/nimcp_exception_trace.h"
#include "utils/exception/nimcp_exception_metrics.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class ExceptionApiRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_exception_system_init();
    }

    void TearDown() override {
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }
};

//=============================================================================
// Severity Enum Value Regression Tests
// REGRESSION: These exact numeric values are part of the API contract
//=============================================================================

TEST_F(ExceptionApiRegressionTest, SeverityEnumValuesNeverChange) {
    // REGRESSION: Severity enum values map to immune system severity (1-10)
    // These values are documented and MUST NOT change

    EXPECT_EQ(static_cast<int>(EXCEPTION_SEVERITY_DEBUG), 1)
        << "EXCEPTION_SEVERITY_DEBUG must equal 1 for immune system mapping";

    EXPECT_EQ(static_cast<int>(EXCEPTION_SEVERITY_INFO), 2)
        << "EXCEPTION_SEVERITY_INFO must equal 2 for immune system mapping";

    EXPECT_EQ(static_cast<int>(EXCEPTION_SEVERITY_WARNING), 3)
        << "EXCEPTION_SEVERITY_WARNING must equal 3 for immune system mapping";

    EXPECT_EQ(static_cast<int>(EXCEPTION_SEVERITY_ERROR), 5)
        << "EXCEPTION_SEVERITY_ERROR must equal 5 for immune system mapping";

    EXPECT_EQ(static_cast<int>(EXCEPTION_SEVERITY_SEVERE), 7)
        << "EXCEPTION_SEVERITY_SEVERE must equal 7 for immune system mapping";

    EXPECT_EQ(static_cast<int>(EXCEPTION_SEVERITY_CRITICAL), 9)
        << "EXCEPTION_SEVERITY_CRITICAL must equal 9 for immune system mapping";

    EXPECT_EQ(static_cast<int>(EXCEPTION_SEVERITY_FATAL), 10)
        << "EXCEPTION_SEVERITY_FATAL must equal 10 for immune system mapping";
}

//=============================================================================
// Category Enum Value Regression Tests
// REGRESSION: Category values map to error code ranges
//=============================================================================

TEST_F(ExceptionApiRegressionTest, CategoryEnumValuesNeverChange) {
    // REGRESSION: Category enum values map to error code ranges (X000-X999)
    // These values MUST match the error code range documentation

    EXPECT_EQ(static_cast<int>(EXCEPTION_CATEGORY_GENERIC), 1)
        << "EXCEPTION_CATEGORY_GENERIC maps to 1000-1999 range";

    EXPECT_EQ(static_cast<int>(EXCEPTION_CATEGORY_MEMORY), 2)
        << "EXCEPTION_CATEGORY_MEMORY maps to 2000-2999 range";

    EXPECT_EQ(static_cast<int>(EXCEPTION_CATEGORY_BRAIN), 3)
        << "EXCEPTION_CATEGORY_BRAIN maps to 3000-3999 range";

    EXPECT_EQ(static_cast<int>(EXCEPTION_CATEGORY_IO), 4)
        << "EXCEPTION_CATEGORY_IO maps to 4000-4999 range";

    EXPECT_EQ(static_cast<int>(EXCEPTION_CATEGORY_CONFIG), 5)
        << "EXCEPTION_CATEGORY_CONFIG maps to 5000-5999 range";

    EXPECT_EQ(static_cast<int>(EXCEPTION_CATEGORY_THREADING), 6)
        << "EXCEPTION_CATEGORY_THREADING maps to 6000-6999 range";

    EXPECT_EQ(static_cast<int>(EXCEPTION_CATEGORY_SIGNAL), 7)
        << "EXCEPTION_CATEGORY_SIGNAL maps to 7000-7999 range";

    EXPECT_EQ(static_cast<int>(EXCEPTION_CATEGORY_COGNITIVE), 8)
        << "EXCEPTION_CATEGORY_COGNITIVE maps to 8000-8999 range";

    EXPECT_EQ(static_cast<int>(EXCEPTION_CATEGORY_BRAIN_REGION), 10)
        << "EXCEPTION_CATEGORY_BRAIN_REGION maps to 10000-19999 range";

    EXPECT_EQ(static_cast<int>(EXCEPTION_CATEGORY_GPU), 11)
        << "EXCEPTION_CATEGORY_GPU maps to 1100-1199 range";

    EXPECT_EQ(static_cast<int>(EXCEPTION_CATEGORY_SECURITY), 20)
        << "EXCEPTION_CATEGORY_SECURITY is a security-related category";
}

//=============================================================================
// Exception Type Enum Value Regression Tests
// REGRESSION: Type enum values used for C polymorphism via casting
//=============================================================================

TEST_F(ExceptionApiRegressionTest, ExceptionTypeEnumValuesNeverChange) {
    // REGRESSION: Exception type enum values enable C polymorphism
    // Changing these would break type identification

    EXPECT_EQ(static_cast<int>(EXCEPTION_TYPE_BASE), 0)
        << "EXCEPTION_TYPE_BASE must be 0 (base type)";

    EXPECT_EQ(static_cast<int>(EXCEPTION_TYPE_MEMORY), 1)
        << "EXCEPTION_TYPE_MEMORY must be 1";

    EXPECT_EQ(static_cast<int>(EXCEPTION_TYPE_BRAIN), 2)
        << "EXCEPTION_TYPE_BRAIN must be 2";

    EXPECT_EQ(static_cast<int>(EXCEPTION_TYPE_IO), 3)
        << "EXCEPTION_TYPE_IO must be 3";

    EXPECT_EQ(static_cast<int>(EXCEPTION_TYPE_THREADING), 4)
        << "EXCEPTION_TYPE_THREADING must be 4";

    EXPECT_EQ(static_cast<int>(EXCEPTION_TYPE_SECURITY), 5)
        << "EXCEPTION_TYPE_SECURITY must be 5";

    EXPECT_EQ(static_cast<int>(EXCEPTION_TYPE_COGNITIVE), 6)
        << "EXCEPTION_TYPE_COGNITIVE must be 6";

    EXPECT_EQ(static_cast<int>(EXCEPTION_TYPE_GPU), 7)
        << "EXCEPTION_TYPE_GPU must be 7";

    EXPECT_EQ(static_cast<int>(EXCEPTION_TYPE_AGGREGATE), 8)
        << "EXCEPTION_TYPE_AGGREGATE must be 8";

    EXPECT_EQ(static_cast<int>(EXCEPTION_TYPE_SIGNAL), 9)
        << "EXCEPTION_TYPE_SIGNAL must be 9";
}

//=============================================================================
// Recovery Action Enum Value Regression Tests
// REGRESSION: Recovery actions map to immune antibody responses
//=============================================================================

TEST_F(ExceptionApiRegressionTest, RecoveryActionEnumValuesNeverChange) {
    // REGRESSION: Recovery action values are used in immune integration
    // and recovery strategy configuration

    EXPECT_EQ(static_cast<int>(EXCEPTION_RECOVERY_NONE), 0)
        << "EXCEPTION_RECOVERY_NONE must be 0 (no action)";

    EXPECT_EQ(static_cast<int>(EXCEPTION_RECOVERY_RETRY), 1)
        << "EXCEPTION_RECOVERY_RETRY must be 1";

    EXPECT_EQ(static_cast<int>(EXCEPTION_RECOVERY_GC), 2)
        << "EXCEPTION_RECOVERY_GC must be 2";

    EXPECT_EQ(static_cast<int>(EXCEPTION_RECOVERY_COMPACT), 3)
        << "EXCEPTION_RECOVERY_COMPACT must be 3";

    EXPECT_EQ(static_cast<int>(EXCEPTION_RECOVERY_ROLLBACK), 4)
        << "EXCEPTION_RECOVERY_ROLLBACK must be 4";

    EXPECT_EQ(static_cast<int>(EXCEPTION_RECOVERY_RESTART_THREAD), 5)
        << "EXCEPTION_RECOVERY_RESTART_THREAD must be 5";

    EXPECT_EQ(static_cast<int>(EXCEPTION_RECOVERY_RESTART_COMPONENT), 6)
        << "EXCEPTION_RECOVERY_RESTART_COMPONENT must be 6";

    EXPECT_EQ(static_cast<int>(EXCEPTION_RECOVERY_QUARANTINE), 7)
        << "EXCEPTION_RECOVERY_QUARANTINE must be 7";

    EXPECT_EQ(static_cast<int>(EXCEPTION_RECOVERY_REDUCE_LOAD), 8)
        << "EXCEPTION_RECOVERY_REDUCE_LOAD must be 8";

    EXPECT_EQ(static_cast<int>(EXCEPTION_RECOVERY_CLEAR_CACHE), 9)
        << "EXCEPTION_RECOVERY_CLEAR_CACHE must be 9";

    EXPECT_EQ(static_cast<int>(EXCEPTION_RECOVERY_EMERGENCY_SAVE), 10)
        << "EXCEPTION_RECOVERY_EMERGENCY_SAVE must be 10";

    EXPECT_EQ(static_cast<int>(EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN), 11)
        << "EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN must be 11";
}

//=============================================================================
// Constant Value Regression Tests
// REGRESSION: Constants define limits and must remain stable
//=============================================================================

TEST_F(ExceptionApiRegressionTest, ConstantValuesNeverChange) {
    // REGRESSION: These constants define buffer sizes and limits
    // Changing them could break serialization or allocation code

    EXPECT_EQ(NIMCP_EXCEPTION_MAX_MESSAGE, 256)
        << "NIMCP_EXCEPTION_MAX_MESSAGE must be 256 for message buffers";

    EXPECT_EQ(NIMCP_EXCEPTION_MAX_STACK_DEPTH, 32)
        << "NIMCP_EXCEPTION_MAX_STACK_DEPTH must be 32 for stack traces";

    EXPECT_EQ(NIMCP_EXCEPTION_EPITOPE_SIZE, 64)
        << "NIMCP_EXCEPTION_EPITOPE_SIZE must be 64 for immune fingerprints";

    EXPECT_EQ(NIMCP_EXCEPTION_MAX_CONTEXT, 512)
        << "NIMCP_EXCEPTION_MAX_CONTEXT must be 512 for context data";

    EXPECT_EQ(NIMCP_EXCEPTION_MAX_CHILDREN, 16)
        << "NIMCP_EXCEPTION_MAX_CHILDREN must be 16 for aggregate exceptions";

    EXPECT_EQ(NIMCP_EXCEPTION_MAX_CONTEXT_ENTRIES, 16)
        << "NIMCP_EXCEPTION_MAX_CONTEXT_ENTRIES must be 16";

    EXPECT_EQ(NIMCP_EXCEPTION_MAX_CONTEXT_KEY, 32)
        << "NIMCP_EXCEPTION_MAX_CONTEXT_KEY must be 32";

    EXPECT_EQ(NIMCP_EXCEPTION_MAX_CONTEXT_VALUE, 128)
        << "NIMCP_EXCEPTION_MAX_CONTEXT_VALUE must be 128";
}

//=============================================================================
// Handler Constants Regression Tests
//=============================================================================

TEST_F(ExceptionApiRegressionTest, HandlerConstantsNeverChange) {
    // REGRESSION: Handler constants define registration limits and priorities

    EXPECT_EQ(NIMCP_HANDLER_MAX_REGISTERED, 64)
        << "NIMCP_HANDLER_MAX_REGISTERED must be 64";

    EXPECT_EQ(NIMCP_TRY_STACK_DEPTH, 16)
        << "NIMCP_TRY_STACK_DEPTH must be 16 for nested try blocks";

    EXPECT_EQ(NIMCP_HANDLER_PRIORITY_HIGH, 100)
        << "NIMCP_HANDLER_PRIORITY_HIGH must be 100";

    EXPECT_EQ(NIMCP_HANDLER_PRIORITY_NORMAL, 50)
        << "NIMCP_HANDLER_PRIORITY_NORMAL must be 50";

    EXPECT_EQ(NIMCP_HANDLER_PRIORITY_LOW, 10)
        << "NIMCP_HANDLER_PRIORITY_LOW must be 10";
}

//=============================================================================
// Error Code Mapping Regression Tests
// REGRESSION: Error code to category mapping must be stable
//=============================================================================

TEST_F(ExceptionApiRegressionTest, ErrorCodeToCategoryMapping) {
    // REGRESSION: Error codes must map to correct categories
    // This mapping is used throughout the system for routing and handling

    // Generic errors (1000-1999) -> CATEGORY_GENERIC
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_UNKNOWN), EXCEPTION_CATEGORY_GENERIC);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_NOT_IMPLEMENTED), EXCEPTION_CATEGORY_GENERIC);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_NULL_POINTER), EXCEPTION_CATEGORY_GENERIC);

    // Memory errors (2000-2999) -> CATEGORY_MEMORY
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_NO_MEMORY), EXCEPTION_CATEGORY_MEMORY);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_BUFFER_OVERFLOW), EXCEPTION_CATEGORY_MEMORY);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_MEMORY_CORRUPTION), EXCEPTION_CATEGORY_MEMORY);

    // Brain errors (3000-3999) -> CATEGORY_BRAIN
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_BRAIN_CREATION), EXCEPTION_CATEGORY_BRAIN);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_NETWORK_CREATION), EXCEPTION_CATEGORY_BRAIN);

    // I/O errors (4000-4999) -> CATEGORY_IO
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_FILE_NOT_FOUND), EXCEPTION_CATEGORY_IO);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_FILE_READ), EXCEPTION_CATEGORY_IO);

    // Config errors (5000-5999) -> CATEGORY_CONFIG
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_CONFIG_INVALID), EXCEPTION_CATEGORY_CONFIG);

    // Threading errors (6000-6999) -> CATEGORY_THREADING
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_THREAD_CREATE), EXCEPTION_CATEGORY_THREADING);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_DEADLOCK), EXCEPTION_CATEGORY_THREADING);

    // Signal errors (7000-7999) -> CATEGORY_SIGNAL
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_SIGSEGV), EXCEPTION_CATEGORY_SIGNAL);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_SIGABRT), EXCEPTION_CATEGORY_SIGNAL);
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_SIGFPE), EXCEPTION_CATEGORY_SIGNAL);

    // Cognitive errors (8000-8999) -> CATEGORY_COGNITIVE
    EXPECT_EQ(nimcp_exception_get_category_from_code(NIMCP_ERROR_WORKING_MEMORY), EXCEPTION_CATEGORY_COGNITIVE);
}

//=============================================================================
// String Conversion Regression Tests
// REGRESSION: String representations must be stable for logging/metrics
//=============================================================================

TEST_F(ExceptionApiRegressionTest, SeverityToStringStable) {
    // REGRESSION: Severity string representations are used in logs and metrics

    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_DEBUG), "DEBUG");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_INFO), "INFO");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_WARNING), "WARNING");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_ERROR), "ERROR");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_SEVERE), "SEVERE");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_CRITICAL), "CRITICAL");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_FATAL), "FATAL");
}

TEST_F(ExceptionApiRegressionTest, CategoryToStringStable) {
    // REGRESSION: Category string representations are used in logs and metrics

    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_GENERIC), "GENERIC");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_MEMORY), "MEMORY");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_BRAIN), "BRAIN");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_IO), "IO");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_CONFIG), "CONFIG");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_THREADING), "THREADING");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_SIGNAL), "SIGNAL");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_COGNITIVE), "COGNITIVE");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_GPU), "GPU");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_SECURITY), "SECURITY");
}

TEST_F(ExceptionApiRegressionTest, TypeToStringStable) {
    // REGRESSION: Type string representations are used in logs and metrics

    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_BASE), "BASE");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_MEMORY), "MEMORY");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_BRAIN), "BRAIN");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_IO), "IO");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_THREADING), "THREADING");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_SECURITY), "SECURITY");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_COGNITIVE), "COGNITIVE");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_GPU), "GPU");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_AGGREGATE), "AGGREGATE");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_SIGNAL), "SIGNAL");
}

TEST_F(ExceptionApiRegressionTest, RecoveryActionToStringStable) {
    // REGRESSION: Recovery action string representations are used in logs

    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_NONE), "NONE");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_RETRY), "RETRY");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_GC), "GC");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_COMPACT), "COMPACT");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_ROLLBACK), "ROLLBACK");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_RESTART_THREAD), "RESTART_THREAD");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_RESTART_COMPONENT), "RESTART_COMPONENT");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_QUARANTINE), "QUARANTINE");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_REDUCE_LOAD), "REDUCE_LOAD");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_CLEAR_CACHE), "CLEAR_CACHE");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_EMERGENCY_SAVE), "EMERGENCY_SAVE");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN), "GRACEFUL_SHUTDOWN");
}

//=============================================================================
// Exception Creation API Regression Tests
// REGRESSION: Function return types and behavior must be stable
//=============================================================================

TEST_F(ExceptionApiRegressionTest, ExceptionCreateReturnsValidPointer) {
    // REGRESSION: nimcp_exception_create must return non-NULL on valid input

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test exception"
    );

    ASSERT_NE(ex, nullptr)
        << "nimcp_exception_create must return non-NULL on valid input";

    // Verify type is set correctly
    EXPECT_EQ(ex->type, EXCEPTION_TYPE_BASE);

    // Verify ref_count is initialized to 1
    EXPECT_EQ(ex->ref_count, 1);

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionApiRegressionTest, TypedExceptionCreationPreservesBaseType) {
    // REGRESSION: Typed exceptions must have base as first member for casting

    nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1024,
        "Memory test"
    );

    ASSERT_NE(mem_ex, nullptr);

    // Cast to base must work (base is first member)
    nimcp_exception_t* base = (nimcp_exception_t*)mem_ex;
    EXPECT_EQ(base->type, EXCEPTION_TYPE_MEMORY);
    EXPECT_EQ(base->code, NIMCP_ERROR_NO_MEMORY);

    nimcp_exception_unref(base);
}

TEST_F(ExceptionApiRegressionTest, BrainExceptionCreation) {
    // REGRESSION: Brain exception creation must set all fields correctly

    nimcp_brain_exception_t* brain_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BRAIN_CREATION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        42,  // brain_id
        "visual_cortex",  // region_name
        "Brain test"
    );

    ASSERT_NE(brain_ex, nullptr);
    EXPECT_EQ(brain_ex->base.type, EXCEPTION_TYPE_BRAIN);
    EXPECT_EQ(brain_ex->brain_id, 42u);
    EXPECT_STREQ(brain_ex->region_name, "visual_cortex");

    nimcp_exception_unref((nimcp_exception_t*)brain_ex);
}

//=============================================================================
// Reference Counting Regression Tests
// REGRESSION: Reference counting behavior must be stable
//=============================================================================

TEST_F(ExceptionApiRegressionTest, ReferenceCountingContract) {
    // REGRESSION: Reference counting contract must be maintained

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Ref count test"
    );

    ASSERT_NE(ex, nullptr);

    // Initial ref count is 1
    EXPECT_EQ(ex->ref_count, 1);

    // Adding reference returns same pointer
    nimcp_exception_t* ref = nimcp_exception_ref(ex);
    EXPECT_EQ(ref, ex);
    EXPECT_EQ(ex->ref_count, 2);

    // Unref decrements
    nimcp_exception_unref(ex);
    EXPECT_EQ(ex->ref_count, 1);

    // Final unref frees (can't check after, but should not crash)
    nimcp_exception_unref(ex);
}

//=============================================================================
// Context API Regression Tests
// REGRESSION: Context key-value operations must be stable
//=============================================================================

TEST_F(ExceptionApiRegressionTest, ContextApiContract) {
    // REGRESSION: Context API behavior must be stable

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Context test"
    );

    ASSERT_NE(ex, nullptr);

    // Set context returns 0 on success
    int result = nimcp_exception_set_context(ex, "key1", "value1");
    EXPECT_EQ(result, 0) << "set_context must return 0 on success";

    // Get context returns correct value
    const char* value = nimcp_exception_get_context(ex, "key1");
    ASSERT_NE(value, nullptr);
    EXPECT_STREQ(value, "value1");

    // Get context returns NULL for missing key
    value = nimcp_exception_get_context(ex, "nonexistent");
    EXPECT_EQ(value, nullptr) << "get_context must return NULL for missing key";

    // Context count is accurate
    EXPECT_EQ(nimcp_exception_context_count(ex), 1u);

    // Remove context returns 0 on success
    result = nimcp_exception_remove_context(ex, "key1");
    EXPECT_EQ(result, 0) << "remove_context must return 0 on success";

    // Remove context returns -1 for missing key
    result = nimcp_exception_remove_context(ex, "key1");
    EXPECT_EQ(result, -1) << "remove_context must return -1 for missing key";

    nimcp_exception_unref(ex);
}

//=============================================================================
// Aggregate Exception API Regression Tests
//=============================================================================

TEST_F(ExceptionApiRegressionTest, AggregateExceptionApiContract) {
    // REGRESSION: Aggregate exception API must be stable

    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Aggregate test"
    );

    ASSERT_NE(agg, nullptr);
    EXPECT_EQ(agg->base.type, EXCEPTION_TYPE_AGGREGATE);

    // Initial count is 0
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 0u);

    // Add child
    nimcp_exception_t* child = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Child 1"
    );

    int result = nimcp_aggregate_exception_add(agg, child);
    EXPECT_EQ(result, 0) << "add must return 0 on success";
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 1u);

    // Get child by index
    nimcp_exception_t* retrieved = nimcp_aggregate_exception_get(agg, 0);
    EXPECT_EQ(retrieved, child);

    // Get with invalid index returns NULL
    retrieved = nimcp_aggregate_exception_get(agg, 100);
    EXPECT_EQ(retrieved, nullptr);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

//=============================================================================
// System Initialization Regression Tests
// REGRESSION: Init/shutdown behavior must be stable
//=============================================================================

TEST_F(ExceptionApiRegressionTest, SystemInitializationContract) {
    // Note: Already initialized in SetUp, shutdown in TearDown

    // REGRESSION: is_initialized returns correct state
    EXPECT_TRUE(nimcp_exception_system_is_initialized())
        << "is_initialized must return true after init";

    nimcp_exception_system_shutdown();
    EXPECT_FALSE(nimcp_exception_system_is_initialized())
        << "is_initialized must return false after shutdown";

    // Multiple init is safe
    EXPECT_EQ(nimcp_exception_system_init(), 0);
    EXPECT_EQ(nimcp_exception_system_init(), 0)
        << "Multiple init calls should succeed (or be idempotent)";
}

//=============================================================================
// Thread-Local Current Exception Regression Tests
//=============================================================================

TEST_F(ExceptionApiRegressionTest, ThreadLocalCurrentExceptionContract) {
    // REGRESSION: Thread-local current exception API must be stable

    // Initially no current exception
    nimcp_exception_t* current = nimcp_exception_get_current();
    // Note: This may or may not be NULL depending on previous tests

    // Create and set current
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Current test"
    );

    nimcp_exception_set_current(ex);
    current = nimcp_exception_get_current();
    EXPECT_EQ(current, ex);

    // Clear current
    nimcp_exception_clear_current();
    current = nimcp_exception_get_current();
    EXPECT_EQ(current, nullptr);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Exception Cause Chain Regression Tests
//=============================================================================

TEST_F(ExceptionApiRegressionTest, CauseChainContract) {
    // REGRESSION: Cause chain API must be stable

    nimcp_exception_t* cause = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Root cause"
    );

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_BRAIN_CREATION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Wrapper"
    );

    // Set cause (takes ownership of reference)
    nimcp_exception_ref(cause);  // Add ref since set_cause takes ownership
    nimcp_exception_set_cause(ex, cause);

    // Get cause returns correct exception
    nimcp_exception_t* retrieved_cause = nimcp_exception_get_cause(ex);
    EXPECT_EQ(retrieved_cause, cause);

    nimcp_exception_unref(cause);
    nimcp_exception_unref(ex);  // This should also unref the cause
}

//=============================================================================
// Circuit Breaker Constants Regression Tests
//=============================================================================

TEST_F(ExceptionApiRegressionTest, CircuitBreakerConstantsNeverChange) {
    // REGRESSION: Circuit breaker constants define system behavior

    EXPECT_EQ(NIMCP_CIRCUIT_MAX_TRACKED, 128)
        << "NIMCP_CIRCUIT_MAX_TRACKED must be 128";

    EXPECT_EQ(NIMCP_SUPPRESSION_MAX_ENTRIES, 64)
        << "NIMCP_SUPPRESSION_MAX_ENTRIES must be 64";

    EXPECT_EQ(NIMCP_CIRCUIT_DEFAULT_THRESHOLD, 10)
        << "NIMCP_CIRCUIT_DEFAULT_THRESHOLD must be 10 exceptions/min";

    EXPECT_EQ(NIMCP_CIRCUIT_DEFAULT_RESET_MS, 30000)
        << "NIMCP_CIRCUIT_DEFAULT_RESET_MS must be 30000ms (30s)";

    EXPECT_EQ(NIMCP_CIRCUIT_HALF_OPEN_ALLOW, 3)
        << "NIMCP_CIRCUIT_HALF_OPEN_ALLOW must be 3";
}

//=============================================================================
// Circuit State Enum Regression Tests
//=============================================================================

TEST_F(ExceptionApiRegressionTest, CircuitStateEnumValuesNeverChange) {
    // REGRESSION: Circuit state enum values are stored and serialized

    EXPECT_EQ(static_cast<int>(CIRCUIT_STATE_CLOSED), 0)
        << "CIRCUIT_STATE_CLOSED must be 0 (normal operation)";

    EXPECT_EQ(static_cast<int>(CIRCUIT_STATE_OPEN), 1)
        << "CIRCUIT_STATE_OPEN must be 1 (blocking)";

    EXPECT_EQ(static_cast<int>(CIRCUIT_STATE_HALF_OPEN), 2)
        << "CIRCUIT_STATE_HALF_OPEN must be 2 (testing)";
}

//=============================================================================
// Trace Constants Regression Tests
//=============================================================================

TEST_F(ExceptionApiRegressionTest, TraceConstantsNeverChange) {
    // REGRESSION: Trace constants affect buffer sizes and serialization

    EXPECT_EQ(NIMCP_TRACE_ID_SIZE, 16)
        << "NIMCP_TRACE_ID_SIZE must be 16 bytes";

    EXPECT_EQ(NIMCP_MAX_PROPAGATION_PATH, 32)
        << "NIMCP_MAX_PROPAGATION_PATH must be 32 hops";

    EXPECT_EQ(NIMCP_MAX_MODULE_NAME_LEN, 64)
        << "NIMCP_MAX_MODULE_NAME_LEN must be 64";

    EXPECT_EQ(NIMCP_MAX_MSG_TYPE_LEN, 32)
        << "NIMCP_MAX_MSG_TYPE_LEN must be 32";

    EXPECT_EQ(NIMCP_TRACE_HEADER_SIZE, 128)
        << "NIMCP_TRACE_HEADER_SIZE must be 128";

    EXPECT_EQ(NIMCP_MAX_TRACE_STACK, 16)
        << "NIMCP_MAX_TRACE_STACK must be 16";
}

//=============================================================================
// Trace Flags Regression Tests
//=============================================================================

TEST_F(ExceptionApiRegressionTest, TraceFlagsNeverChange) {
    // REGRESSION: Trace flags are bit flags and must remain stable

    EXPECT_EQ(NIMCP_TRACE_FLAG_SAMPLED, 0x01)
        << "NIMCP_TRACE_FLAG_SAMPLED must be 0x01";

    EXPECT_EQ(NIMCP_TRACE_FLAG_DEBUG, 0x02)
        << "NIMCP_TRACE_FLAG_DEBUG must be 0x02";

    EXPECT_EQ(NIMCP_TRACE_FLAG_DEFERRED, 0x04)
        << "NIMCP_TRACE_FLAG_DEFERRED must be 0x04";

    EXPECT_EQ(NIMCP_TRACE_FLAG_SECURE, 0x08)
        << "NIMCP_TRACE_FLAG_SECURE must be 0x08";
}

//=============================================================================
// Metrics Constants Regression Tests
//=============================================================================

TEST_F(ExceptionApiRegressionTest, MetricsConstantsNeverChange) {
    // REGRESSION: Metrics constants define storage and algorithm parameters

    EXPECT_EQ(NIMCP_METRICS_MAX_PATTERNS, 256)
        << "NIMCP_METRICS_MAX_PATTERNS must be 256";

    EXPECT_EQ(NIMCP_METRICS_HISTORY_SIZE, 1000)
        << "NIMCP_METRICS_HISTORY_SIZE must be 1000";

    EXPECT_FLOAT_EQ(NIMCP_METRICS_EMA_ALPHA, 0.1f)
        << "NIMCP_METRICS_EMA_ALPHA must be 0.1f";

    EXPECT_EQ(NIMCP_METRICS_MIN_SAMPLES, 5)
        << "NIMCP_METRICS_MIN_SAMPLES must be 5";

    EXPECT_EQ(NIMCP_METRICS_MAX_CONSECUTIVE_FAILURES, 10)
        << "NIMCP_METRICS_MAX_CONSECUTIVE_FAILURES must be 10";

    EXPECT_EQ(NIMCP_METRICS_EXCEPTION_RECOVERY_COUNT, 12)
        << "NIMCP_METRICS_EXCEPTION_RECOVERY_COUNT must be 12";

    EXPECT_EQ(NIMCP_METRICS_MAX_CATEGORIES, 20)
        << "NIMCP_METRICS_MAX_CATEGORIES must be 20";
}

//=============================================================================
// Immune Integration Constants Regression Tests
//=============================================================================

TEST_F(ExceptionApiRegressionTest, ImmuneIntegrationConstantsNeverChange) {
    // REGRESSION: Immune integration constants

    EXPECT_EQ(NIMCP_EXCEPTION_IMMUNE_MIN_SEVERITY, EXCEPTION_SEVERITY_SEVERE)
        << "NIMCP_EXCEPTION_IMMUNE_MIN_SEVERITY must equal EXCEPTION_SEVERITY_SEVERE";

    EXPECT_EQ(NIMCP_EXCEPTION_IMMUNE_QUEUE_SIZE, 256)
        << "NIMCP_EXCEPTION_IMMUNE_QUEUE_SIZE must be 256";
}

//=============================================================================
// Antigen Source Enum Regression Tests
//=============================================================================

TEST_F(ExceptionApiRegressionTest, AntigenSourceEnumValuesNeverChange) {
    // REGRESSION: Antigen source enum mirrors brain_antigen_source_t

    EXPECT_EQ(static_cast<int>(EX_ANTIGEN_SOURCE_BBB), 0)
        << "EX_ANTIGEN_SOURCE_BBB must be 0";

    EXPECT_EQ(static_cast<int>(EX_ANTIGEN_SOURCE_BFT), 1)
        << "EX_ANTIGEN_SOURCE_BFT must be 1";

    EXPECT_EQ(static_cast<int>(EX_ANTIGEN_SOURCE_ANOMALY), 2)
        << "EX_ANTIGEN_SOURCE_ANOMALY must be 2";

    EXPECT_EQ(static_cast<int>(EX_ANTIGEN_SOURCE_SWARM), 3)
        << "EX_ANTIGEN_SOURCE_SWARM must be 3";

    EXPECT_EQ(static_cast<int>(EX_ANTIGEN_SOURCE_MANUAL), 4)
        << "EX_ANTIGEN_SOURCE_MANUAL must be 4";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
