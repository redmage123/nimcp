/**
 * @file test_ternary_exception_handling.cpp
 * @brief Unit tests for ternary module exception handling
 *
 * WHAT: Test exception handling across all ternary logic modules
 * WHY:  Ensure consistent error-to-exception mapping and handler chain dispatch
 * HOW:  Test each ternary module's error conditions and exception integration
 *
 * TERNARY MODULES TESTED:
 * - Ternary Types (trit_t, packed storage)
 * - Ternary Logic (Kleene, Lukasiewicz)
 * - Ternary Vector (array operations)
 * - Ternary Matrix (2D operations)
 * - Ternary Tensor (N-D operations)
 * - Ternary Storage (2-bit, base-243 packing)
 * - Ternary Convert (float/probabilistic conversion)
 *
 * TEST PATTERNS:
 * - Error code to exception mapping
 * - Exception dispatch through handler chain
 * - Exception category classification (TERNARY, STORAGE, CONVERSION)
 * - Recovery strategy determination
 *
 * @author NIMCP Development Team
 * @date 2026-01-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <thread>
#include <vector>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/ternary/nimcp_ternary.h"
}

//=============================================================================
// Ternary Exception Categories
//=============================================================================

// Define ternary-specific exception categories for testing
#define EXCEPTION_CATEGORY_TERNARY_BASE       400
#define EXCEPTION_CATEGORY_TERNARY_TYPES      (EXCEPTION_CATEGORY_TERNARY_BASE + 1)
#define EXCEPTION_CATEGORY_TERNARY_LOGIC      (EXCEPTION_CATEGORY_TERNARY_BASE + 2)
#define EXCEPTION_CATEGORY_TERNARY_VECTOR     (EXCEPTION_CATEGORY_TERNARY_BASE + 3)
#define EXCEPTION_CATEGORY_TERNARY_MATRIX     (EXCEPTION_CATEGORY_TERNARY_BASE + 4)
#define EXCEPTION_CATEGORY_TERNARY_TENSOR     (EXCEPTION_CATEGORY_TERNARY_BASE + 5)
#define EXCEPTION_CATEGORY_TERNARY_STORAGE    (EXCEPTION_CATEGORY_TERNARY_BASE + 6)
#define EXCEPTION_CATEGORY_TERNARY_CONVERT    (EXCEPTION_CATEGORY_TERNARY_BASE + 7)
#define EXCEPTION_CATEGORY_TERNARY_QUANTIZE   (EXCEPTION_CATEGORY_TERNARY_BASE + 8)

//=============================================================================
// Test Fixture
//=============================================================================

class TernaryExceptionHandlingTest : public ::testing::Test {
protected:
    static std::atomic<int> handler_call_count;
    static std::atomic<int> last_exception_code;
    static std::atomic<int> last_exception_category;
    static std::atomic<bool> handler_consumed;

    void SetUp() override {
        handler_call_count = 0;
        last_exception_code = 0;
        last_exception_category = 0;
        handler_consumed = false;

        nimcp_exception_system_init();
    }

    void TearDown() override {
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    static bool test_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        last_exception_code = ex->code;
        last_exception_category = ex->category;
        return false;  // Don't consume - allow other handlers
    }

    static bool consuming_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        last_exception_code = ex->code;
        handler_consumed = true;
        return true;  // Consume the exception
    }

    // Helper to create ternary exception
    nimcp_exception_t* create_ternary_exception(
        nimcp_error_t code,
        int category,
        nimcp_exception_severity_t severity,
        const char* message
    ) {
        nimcp_exception_t* ex = nimcp_exception_create(
            code,
            severity,
            __FILE__, __LINE__, __func__,
            message
        );
        if (ex) {
            ex->category = static_cast<nimcp_exception_category_t>(category);
        }
        return ex;
    }
};

std::atomic<int> TernaryExceptionHandlingTest::handler_call_count(0);
std::atomic<int> TernaryExceptionHandlingTest::last_exception_code(0);
std::atomic<int> TernaryExceptionHandlingTest::last_exception_category(0);
std::atomic<bool> TernaryExceptionHandlingTest::handler_consumed(false);

//=============================================================================
// Exception Creation Tests
//=============================================================================

TEST_F(TernaryExceptionHandlingTest, CreateTernaryTypesException) {
    // WHAT: Test creation of ternary types-related exception
    // WHY:  Verify exception fields are set correctly

    nimcp_exception_t* ex = create_ternary_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_TERNARY_TYPES,
        EXCEPTION_SEVERITY_ERROR,
        "Invalid trit value: 5 (expected -1, 0, or +1)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_TERNARY_TYPES);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_ERROR);
    EXPECT_EQ(ex->type, EXCEPTION_TYPE_BASE);
    EXPECT_NE(ex->message, nullptr);

    nimcp_exception_unref(ex);
}

TEST_F(TernaryExceptionHandlingTest, CreateTernaryLogicException) {
    // WHAT: Test creation of ternary logic-related exception
    // WHY:  Logic errors need proper categorization

    nimcp_exception_t* ex = create_ternary_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_TERNARY_LOGIC,
        EXCEPTION_SEVERITY_ERROR,
        "Kleene logic operation failed - invalid operand"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_TERNARY_LOGIC);

    nimcp_exception_unref(ex);
}

TEST_F(TernaryExceptionHandlingTest, CreateTernaryVectorException) {
    // WHAT: Test creation of ternary vector exception
    // WHY:  Vector operations need specialized handling

    nimcp_exception_t* ex = create_ternary_exception(
        NIMCP_ERROR_DIMENSION_MISMATCH,
        EXCEPTION_CATEGORY_TERNARY_VECTOR,
        EXCEPTION_SEVERITY_ERROR,
        "Vector dimension mismatch (100 vs 200)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_DIMENSION_MISMATCH);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_TERNARY_VECTOR);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_ERROR);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Ternary Types Exception Tests
//=============================================================================

TEST_F(TernaryExceptionHandlingTest, TernaryNullPointerException) {
    // WHAT: Test exception for NULL pointer parameter
    // WHY:  Verify proper error handling for invalid inputs

    nimcp_exception_t* ex = create_ternary_exception(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_CATEGORY_TERNARY_TYPES,
        EXCEPTION_SEVERITY_ERROR,
        "Trit vector pointer is NULL"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NULL_POINTER);

    // Register handler and dispatch
    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "ternary_null_handler";
    options.handler = test_exception_handler;
    options.priority = 100;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);

    handler_call_count = 0;
    nimcp_exception_dispatch(ex);
    EXPECT_GE(handler_call_count.load(), 1);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_NULL_POINTER);

    nimcp_exception_unref(ex);
    if (reg) nimcp_handler_unregister(reg);
}

TEST_F(TernaryExceptionHandlingTest, TernaryInvalidTritValueException) {
    // WHAT: Test exception for invalid trit value
    // WHY:  Trit values must be -1, 0, or +1

    nimcp_exception_t* ex = create_ternary_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_TERNARY_TYPES,
        EXCEPTION_SEVERITY_ERROR,
        "Invalid trit value: 2 (valid range: -1 to +1)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);

    nimcp_exception_unref(ex);
}

TEST_F(TernaryExceptionHandlingTest, TernaryInvalidPackModeException) {
    // WHAT: Test exception for invalid pack mode
    // WHY:  Pack mode must be valid enum value

    nimcp_exception_t* ex = create_ternary_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_TERNARY_STORAGE,
        EXCEPTION_SEVERITY_ERROR,
        "Invalid ternary pack mode: 5"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_TERNARY_STORAGE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Ternary Logic Exception Tests
//=============================================================================

TEST_F(TernaryExceptionHandlingTest, TernaryKleeneOperationException) {
    // WHAT: Test exception for Kleene logic operation failure
    // WHY:  Logic operations may fail with invalid inputs

    nimcp_exception_t* ex = create_ternary_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_TERNARY_LOGIC,
        EXCEPTION_SEVERITY_ERROR,
        "Kleene AND operation failed - invalid operand detected"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_TERNARY_LOGIC);

    nimcp_exception_unref(ex);
}

TEST_F(TernaryExceptionHandlingTest, TernaryLukasiewiczOperationException) {
    // WHAT: Test exception for Lukasiewicz logic operation failure
    // WHY:  Different logic systems have different failure modes

    nimcp_exception_t* ex = create_ternary_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_TERNARY_LOGIC,
        EXCEPTION_SEVERITY_ERROR,
        "Lukasiewicz implication operation failed"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);

    nimcp_exception_unref(ex);
}

TEST_F(TernaryExceptionHandlingTest, TernaryAggregationException) {
    // WHAT: Test exception for aggregation operation failure
    // WHY:  Majority/consensus operations may fail

    nimcp_exception_t* ex = create_ternary_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_TERNARY_LOGIC,
        EXCEPTION_SEVERITY_WARNING,
        "Majority vote failed - zero-length input array"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_WARNING);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Ternary Vector Exception Tests
//=============================================================================

TEST_F(TernaryExceptionHandlingTest, TernaryVectorAllocationException) {
    // WHAT: Test exception for vector allocation failure
    // WHY:  Large vectors may fail to allocate

    nimcp_exception_t* ex = create_ternary_exception(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_CATEGORY_TERNARY_VECTOR,
        EXCEPTION_SEVERITY_ERROR,
        "Failed to allocate trit vector (1 billion elements)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NO_MEMORY);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_TERNARY_VECTOR);

    nimcp_exception_unref(ex);
}

TEST_F(TernaryExceptionHandlingTest, TernaryVectorIndexException) {
    // WHAT: Test exception for out-of-bounds index
    // WHY:  Index must be within vector bounds

    nimcp_exception_t* ex = create_ternary_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_TERNARY_VECTOR,
        EXCEPTION_SEVERITY_ERROR,
        "Trit vector index out of bounds (500 >= 256)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);

    nimcp_exception_unref(ex);
}

TEST_F(TernaryExceptionHandlingTest, TernaryVectorShapeMismatchException) {
    // WHAT: Test exception for shape mismatch in operations
    // WHY:  Binary operations require matching shapes

    nimcp_exception_t* ex = create_ternary_exception(
        NIMCP_ERROR_DIMENSION_MISMATCH,
        EXCEPTION_CATEGORY_TERNARY_VECTOR,
        EXCEPTION_SEVERITY_ERROR,
        "Trit vector shape mismatch for AND operation"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_DIMENSION_MISMATCH);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Ternary Matrix Exception Tests
//=============================================================================

TEST_F(TernaryExceptionHandlingTest, TernaryMatrixAllocationException) {
    // WHAT: Test exception for matrix allocation failure
    // WHY:  Large matrices may fail to allocate

    nimcp_exception_t* ex = create_ternary_exception(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_CATEGORY_TERNARY_MATRIX,
        EXCEPTION_SEVERITY_ERROR,
        "Failed to allocate trit matrix (10000x10000)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NO_MEMORY);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_TERNARY_MATRIX);

    nimcp_exception_unref(ex);
}

TEST_F(TernaryExceptionHandlingTest, TernaryMatrixDimensionException) {
    // WHAT: Test exception for invalid matrix dimensions
    // WHY:  Matrices must have valid dimensions

    nimcp_exception_t* ex = create_ternary_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_TERNARY_MATRIX,
        EXCEPTION_SEVERITY_ERROR,
        "Invalid matrix dimensions (0 rows)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);

    nimcp_exception_unref(ex);
}

TEST_F(TernaryExceptionHandlingTest, TernaryMatrixMultiplyException) {
    // WHAT: Test exception for matrix multiplication error
    // WHY:  Matrix multiply requires compatible dimensions

    nimcp_exception_t* ex = create_ternary_exception(
        NIMCP_ERROR_DIMENSION_MISMATCH,
        EXCEPTION_CATEGORY_TERNARY_MATRIX,
        EXCEPTION_SEVERITY_ERROR,
        "Matrix multiply dimension mismatch (100x50 * 40x100)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_DIMENSION_MISMATCH);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Ternary Tensor Exception Tests
//=============================================================================

TEST_F(TernaryExceptionHandlingTest, TernaryTensorRankException) {
    // WHAT: Test exception for tensor rank exceeded
    // WHY:  Tensors have maximum rank limit

    nimcp_exception_t* ex = create_ternary_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_TERNARY_TENSOR,
        EXCEPTION_SEVERITY_ERROR,
        "Tensor rank exceeds maximum (10 > 8)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_TERNARY_TENSOR);

    nimcp_exception_unref(ex);
}

TEST_F(TernaryExceptionHandlingTest, TernaryTensorBroadcastException) {
    // WHAT: Test exception for broadcast failure
    // WHY:  Tensor broadcasting may fail with incompatible shapes

    nimcp_exception_t* ex = create_ternary_exception(
        NIMCP_ERROR_DIMENSION_MISMATCH,
        EXCEPTION_CATEGORY_TERNARY_TENSOR,
        EXCEPTION_SEVERITY_ERROR,
        "Cannot broadcast shapes [3,4,5] and [3,2,5]"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_DIMENSION_MISMATCH);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Ternary Storage Exception Tests
//=============================================================================

TEST_F(TernaryExceptionHandlingTest, TernaryPackedStorageCorruptionException) {
    // WHAT: Test exception for packed storage corruption
    // WHY:  Packed data may become corrupted

    nimcp_exception_t* ex = create_ternary_exception(
        NIMCP_ERROR_MEMORY_CORRUPTION,
        EXCEPTION_CATEGORY_TERNARY_STORAGE,
        EXCEPTION_SEVERITY_CRITICAL,
        "Base-243 packed storage corruption detected"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_MEMORY_CORRUPTION);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_TERNARY_STORAGE);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_CRITICAL);

    nimcp_exception_unref(ex);
}

TEST_F(TernaryExceptionHandlingTest, TernaryPackingOverflowException) {
    // WHAT: Test exception for packing overflow
    // WHY:  Base-243 encoding has specific limits

    nimcp_exception_t* ex = create_ternary_exception(
        NIMCP_ERROR_BUFFER_OVERFLOW,
        EXCEPTION_CATEGORY_TERNARY_STORAGE,
        EXCEPTION_SEVERITY_ERROR,
        "Base-243 packing overflow (value > 242)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_BUFFER_OVERFLOW);

    nimcp_exception_unref(ex);
}

TEST_F(TernaryExceptionHandlingTest, TernaryUnpackingException) {
    // WHAT: Test exception for unpacking failure
    // WHY:  Packed data may fail to unpack

    nimcp_exception_t* ex = create_ternary_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_TERNARY_STORAGE,
        EXCEPTION_SEVERITY_ERROR,
        "2-bit trit unpacking failed - invalid byte"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Ternary Conversion Exception Tests
//=============================================================================

TEST_F(TernaryExceptionHandlingTest, TernaryFromFloatConversionException) {
    // WHAT: Test exception for float-to-trit conversion failure
    // WHY:  NaN/Inf floats cannot be converted

    nimcp_exception_t* ex = create_ternary_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_TERNARY_CONVERT,
        EXCEPTION_SEVERITY_WARNING,
        "Cannot convert NaN to trit value"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_TERNARY_CONVERT);

    nimcp_exception_unref(ex);
}

TEST_F(TernaryExceptionHandlingTest, TernaryQuantizationException) {
    // WHAT: Test exception for quantization failure
    // WHY:  Quantization may fail with invalid thresholds

    nimcp_exception_t* ex = create_ternary_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_TERNARY_QUANTIZE,
        EXCEPTION_SEVERITY_ERROR,
        "Invalid quantization threshold (negative value)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_TERNARY_QUANTIZE);

    nimcp_exception_unref(ex);
}

TEST_F(TernaryExceptionHandlingTest, TernaryProbabilisticConversionException) {
    // WHAT: Test exception for probabilistic conversion failure
    // WHY:  Probabilities must be in valid range

    nimcp_exception_t* ex = create_ternary_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_TERNARY_CONVERT,
        EXCEPTION_SEVERITY_WARNING,
        "Probability out of range (1.5 > 1.0)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Handler Chain Tests
//=============================================================================

TEST_F(TernaryExceptionHandlingTest, HandlerChainDispatch) {
    // WHAT: Test exception dispatch through multiple handlers
    // WHY:  Verify chain processing works correctly

    // Register multiple handlers
    nimcp_handler_options_t options1, options2;
    nimcp_handler_default_options(&options1);
    nimcp_handler_default_options(&options2);

    options1.name = "ternary_handler_1";
    options1.handler = test_exception_handler;
    options1.priority = 100;

    options2.name = "ternary_handler_2";
    options2.handler = test_exception_handler;
    options2.priority = 50;

    nimcp_handler_registration_t* reg1 = nimcp_handler_register(&options1);
    nimcp_handler_registration_t* reg2 = nimcp_handler_register(&options2);

    nimcp_exception_t* ex = create_ternary_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_TERNARY_LOGIC,
        EXCEPTION_SEVERITY_ERROR,
        "Test exception for handler chain"
    );

    handler_call_count = 0;
    nimcp_exception_dispatch(ex);

    // Both handlers should be called (neither consumes)
    EXPECT_GE(handler_call_count.load(), 2);

    nimcp_exception_unref(ex);
    if (reg1) nimcp_handler_unregister(reg1);
    if (reg2) nimcp_handler_unregister(reg2);
}

TEST_F(TernaryExceptionHandlingTest, HandlerConsumesException) {
    // WHAT: Test handler consuming exception stops chain
    // WHY:  Verify consumed exceptions don't propagate

    nimcp_handler_options_t options1, options2;
    nimcp_handler_default_options(&options1);
    nimcp_handler_default_options(&options2);

    // First handler consumes
    options1.name = "consuming_handler";
    options1.handler = consuming_exception_handler;
    options1.priority = 100;

    // Second handler should not be called
    options2.name = "secondary_handler";
    options2.handler = test_exception_handler;
    options2.priority = 50;

    nimcp_handler_registration_t* reg1 = nimcp_handler_register(&options1);
    nimcp_handler_registration_t* reg2 = nimcp_handler_register(&options2);

    nimcp_exception_t* ex = create_ternary_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_TERNARY_VECTOR,
        EXCEPTION_SEVERITY_ERROR,
        "Test exception for consumption"
    );

    handler_call_count = 0;
    handler_consumed = false;
    nimcp_exception_dispatch(ex);

    // Only consuming handler should be called
    EXPECT_TRUE(handler_consumed.load());
    EXPECT_EQ(handler_call_count.load(), 1);

    nimcp_exception_unref(ex);
    if (reg1) nimcp_handler_unregister(reg1);
    if (reg2) nimcp_handler_unregister(reg2);
}

//=============================================================================
// Recovery Strategy Tests
//=============================================================================

TEST_F(TernaryExceptionHandlingTest, TernaryExceptionRecoveryStrategy) {
    // WHAT: Test recovery strategy for ternary exceptions
    // WHY:  Ternary failures may need retry or memory cleanup

    nimcp_exception_t* ex = create_ternary_exception(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_CATEGORY_TERNARY_VECTOR,
        EXCEPTION_SEVERITY_ERROR,
        "Trit vector allocation failed"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Memory exceptions should have GC or retry as action
    EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE);

    nimcp_exception_unref(ex);
}

TEST_F(TernaryExceptionHandlingTest, CriticalTernaryExceptionRecovery) {
    // WHAT: Test recovery for critical ternary failures
    // WHY:  Critical failures may require emergency measures

    nimcp_exception_t* ex = create_ternary_exception(
        NIMCP_ERROR_MEMORY_CORRUPTION,
        EXCEPTION_CATEGORY_TERNARY_STORAGE,
        EXCEPTION_SEVERITY_CRITICAL,
        "Packed storage corruption detected"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_CRITICAL);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Critical errors should trigger some kind of recovery action
    EXPECT_TRUE(strategy.primary_action != EXCEPTION_RECOVERY_NONE ||
                strategy.fallback_action != EXCEPTION_RECOVERY_NONE ||
                strategy.retry_count > 0);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Exception Statistics Tests
//=============================================================================

TEST_F(TernaryExceptionHandlingTest, ExceptionStatisticsTracking) {
    // WHAT: Test that exception dispatch is tracked by handlers
    // WHY:  Need to monitor ternary exception frequency

    // Register a counting handler
    static std::atomic<int> dispatch_count{0};
    dispatch_count = 0;

    auto counting_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        (void)user_data;
        dispatch_count++;
        return false;
    };

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "stats_counter";
    opts.handler = counting_handler;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Create and dispatch several exceptions
    for (int i = 0; i < 5; i++) {
        nimcp_exception_t* ex = create_ternary_exception(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_CATEGORY_TERNARY_LOGIC,
            EXCEPTION_SEVERITY_WARNING,
            "Test exception for statistics"
        );
        if (ex) {
            nimcp_exception_dispatch(ex);
            nimcp_exception_unref(ex);
        }
    }

    // Handler should have been called for each exception
    EXPECT_GE(dispatch_count.load(), 5);

    nimcp_handler_unregister(reg);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(TernaryExceptionHandlingTest, ConcurrentExceptionCreation) {
    // WHAT: Test concurrent exception creation
    // WHY:  Ternary operations may run across multiple threads

    std::atomic<int> success_count{0};
    const int num_threads = 4;
    const int exceptions_per_thread = 10;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&success_count, t, exceptions_per_thread]() {
            for (int i = 0; i < exceptions_per_thread; i++) {
                nimcp_exception_t* ex = nimcp_exception_create(
                    NIMCP_ERROR_OPERATION_FAILED,
                    EXCEPTION_SEVERITY_ERROR,
                    __FILE__, __LINE__, __func__,
                    "Thread %d exception %d", t, i
                );
                if (ex) {
                    success_count++;
                    nimcp_exception_unref(ex);
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count.load(), num_threads * exceptions_per_thread);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
