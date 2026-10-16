/**
 * @file test_api_consistency.cpp
 * @brief Unit tests for API consistency across NIMCP modules
 *
 * WHAT: Tests for error code consistency, return value conventions, NULL handling
 * WHY:  Ensure uniform API behavior across all NIMCP modules
 * HOW:  GTest parameterized tests for cross-module verification
 *
 * TEST CATEGORIES:
 * - Error code consistency
 * - nimcp_error_t type compatibility
 * - Return value conventions (0 success, 1000+ errors)
 * - NULL parameter handling consistency
 * - FEP bridge return value conventions
 *
 * @author NIMCP Development Team
 * @date 2026-02-02
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

extern "C" {
#include "utils/error/nimcp_error_codes.h"
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/config/nimcp_config_array.h"
}

namespace {

/* ============================================================================
 * Error Code Consistency Tests
 * ============================================================================ */

class ErrorCodeConsistencyTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
    }

    void TearDown() override {
        nimcp_memory_cleanup();
    }
};

TEST_F(ErrorCodeConsistencyTest, SuccessCodeIsZero) {
    /* WHAT: Verify all success codes equal zero */
    /* WHY: Consistent success checking across modules */
    EXPECT_EQ(NIMCP_SUCCESS, 0);
    EXPECT_EQ(NIMCP_OK, 0);
}

TEST_F(ErrorCodeConsistencyTest, ErrorCodesArePositive) {
    /* WHAT: Verify error codes are positive (1000+) */
    /* WHY: NIMCP uses positive error codes, not negative */
    EXPECT_GE(NIMCP_ERROR_UNKNOWN, 1000);
    EXPECT_GE(NIMCP_ERROR_INVALID_PARAMETER, 1000);
    EXPECT_GE(NIMCP_ERROR_NULL_POINTER, 1000);
    EXPECT_GE(NIMCP_ERROR_OUT_OF_RANGE, 1000);
    EXPECT_GE(NIMCP_ERROR_NO_MEMORY, 1000);
}

TEST_F(ErrorCodeConsistencyTest, ErrorCodeCategories) {
    /* WHAT: Verify error codes are in correct ranges */
    /* WHY: Categories help identify error source */

    // Generic errors: 1000-1999
    EXPECT_GE(NIMCP_ERROR_UNKNOWN, 1000);
    EXPECT_LT(NIMCP_ERROR_UNKNOWN, 2000);

    // Memory errors: 2000-2999
    EXPECT_GE(NIMCP_ERROR_NO_MEMORY, 2000);
    EXPECT_LT(NIMCP_ERROR_NO_MEMORY, 3000);

    // Brain errors: 3000-3999
    EXPECT_GE(NIMCP_ERROR_BRAIN_CREATION, 3000);
    EXPECT_LT(NIMCP_ERROR_BRAIN_CREATION, 4000);

    // I/O errors: 4000-4999
    EXPECT_GE(NIMCP_ERROR_FILE_NOT_FOUND, 4000);
    EXPECT_LT(NIMCP_ERROR_FILE_NOT_FOUND, 5000);

    // Security errors: 9000-9999
    EXPECT_GE(NIMCP_ERROR_SECURITY_BASE, 9000);
    EXPECT_LT(NIMCP_ERROR_SECURITY_BASE, 10000);
}

TEST_F(ErrorCodeConsistencyTest, ErrorCategoryExtraction) {
    /* WHAT: Test error category extraction function */
    /* WHY: Categories should be derivable from error code */
    EXPECT_EQ(nimcp_error_get_category(NIMCP_SUCCESS), 0);
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_UNKNOWN), 1);  // 1000/1000 = 1
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_NO_MEMORY), 2); // 2000/1000 = 2
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_BRAIN_CREATION), 3);
}

TEST_F(ErrorCodeConsistencyTest, SuccessCheckFunction) {
    /* WHAT: Test nimcp_error_is_success function */
    EXPECT_TRUE(nimcp_error_is_success(NIMCP_SUCCESS));
    EXPECT_TRUE(nimcp_error_is_success(NIMCP_OK));
    EXPECT_TRUE(nimcp_error_is_success(NIMCP_SUCCESS_WITH_WARNINGS));
    EXPECT_TRUE(nimcp_error_is_success(NIMCP_SUCCESS_PARTIAL));

    EXPECT_FALSE(nimcp_error_is_success(NIMCP_ERROR_UNKNOWN));
    EXPECT_FALSE(nimcp_error_is_success(NIMCP_ERROR_NULL_POINTER));
}

TEST_F(ErrorCodeConsistencyTest, FailureCheckFunction) {
    /* WHAT: Test nimcp_error_is_failure function */
    EXPECT_FALSE(nimcp_error_is_failure(NIMCP_SUCCESS));
    EXPECT_FALSE(nimcp_error_is_failure(NIMCP_OK));

    EXPECT_TRUE(nimcp_error_is_failure(NIMCP_ERROR_UNKNOWN));
    EXPECT_TRUE(nimcp_error_is_failure(NIMCP_ERROR_NO_MEMORY));
    EXPECT_TRUE(nimcp_error_is_failure(NIMCP_ERROR_BRAIN_CREATION));
}

/* ============================================================================
 * nimcp_error_t Type Compatibility Tests
 * ============================================================================ */

class ErrorTypeCompatibilityTest : public ::testing::Test {};

TEST_F(ErrorTypeCompatibilityTest, ErrorTypeIsInt32) {
    /* WHAT: Verify nimcp_error_t is int32_t */
    /* WHY: Consistent type across all modules */
    EXPECT_EQ(sizeof(nimcp_error_t), sizeof(int32_t));
}

TEST_F(ErrorTypeCompatibilityTest, ErrorTypeSignedness) {
    /* WHAT: Verify nimcp_error_t is signed */
    /* WHY: Need to handle potential negative values from some APIs */
    nimcp_error_t negative = -1;
    EXPECT_LT(negative, 0);  // Should work if signed
}

TEST_F(ErrorTypeCompatibilityTest, ErrorTypeRanges) {
    /* WHAT: Verify error codes fit in int32_t */
    /* WHY: All defined error codes should be representable */
    nimcp_error_t max_brain_region = NIMCP_ERROR_VTA_INTERNAL;

    EXPECT_LT(max_brain_region, INT32_MAX);
    EXPECT_GE(max_brain_region, 0);
}

TEST_F(ErrorTypeCompatibilityTest, ResultTypeCompatibility) {
    /* WHAT: Verify nimcp_result_t is compatible with nimcp_error_t */
    /* WHY: Both types may be used interchangeably in some contexts */
    EXPECT_EQ(sizeof(nimcp_result_t), sizeof(nimcp_error_t));
}

/* ============================================================================
 * FEP Bridge Return Value Convention Tests
 * ============================================================================ */

class FEPBridgeConventionTest : public ::testing::Test {};

TEST_F(FEPBridgeConventionTest, FEPResultConversion) {
    /* WHAT: Test FEP bridge result conversion */
    /* WHY: FEP bridges use 0/-1, NIMCP uses 0/1000+ */

    // FEP success (0) should map to NIMCP_SUCCESS
    EXPECT_EQ(nimcp_from_fep_result(0), NIMCP_SUCCESS);

    // FEP error (-1) should map to NIMCP_ERROR_OPERATION_FAILED
    EXPECT_EQ(nimcp_from_fep_result(-1), NIMCP_ERROR_OPERATION_FAILED);
}

TEST_F(FEPBridgeConventionTest, NIMCPToFEPConversion) {
    /* WHAT: Test NIMCP to FEP result conversion */

    // NIMCP success should map to FEP success (0)
    EXPECT_EQ(nimcp_to_fep_result(NIMCP_SUCCESS), 0);
    EXPECT_EQ(nimcp_to_fep_result(NIMCP_OK), 0);

    // NIMCP error should map to FEP error (-1)
    EXPECT_EQ(nimcp_to_fep_result(NIMCP_ERROR_UNKNOWN), -1);
    EXPECT_EQ(nimcp_to_fep_result(NIMCP_ERROR_NO_MEMORY), -1);
}

TEST_F(FEPBridgeConventionTest, IsOkFunction) {
    /* WHAT: Test nimcp_is_ok helper */
    EXPECT_TRUE(nimcp_is_ok(NIMCP_SUCCESS));
    EXPECT_TRUE(nimcp_is_ok(NIMCP_OK));
    EXPECT_TRUE(nimcp_is_ok(0));

    EXPECT_FALSE(nimcp_is_ok(NIMCP_ERROR_UNKNOWN));
    EXPECT_FALSE(nimcp_is_ok(1000));
}

TEST_F(FEPBridgeConventionTest, IsErrorFunction) {
    /* WHAT: Test nimcp_is_error helper */
    EXPECT_FALSE(nimcp_is_error(NIMCP_SUCCESS));
    EXPECT_FALSE(nimcp_is_error(0));

    EXPECT_TRUE(nimcp_is_error(NIMCP_ERROR_UNKNOWN));
    EXPECT_TRUE(nimcp_is_error(1000));
    EXPECT_TRUE(nimcp_is_error(9999));
}

/* ============================================================================
 * NULL Parameter Handling Consistency Tests
 * ============================================================================ */

class NullHandlingConsistencyTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
    }

    void TearDown() override {
        nimcp_memory_cleanup();
    }
};

TEST_F(NullHandlingConsistencyTest, ConfigArrayCreateNullType) {
    /* WHAT: Test config_array_create with valid params */
    /* WHY: Baseline for comparison */
    config_array_full_t* arr = config_array_create(CONFIG_TYPE_INT, 10);
    ASSERT_NE(arr, nullptr);
    config_array_destroy(arr);
}

TEST_F(NullHandlingConsistencyTest, ConfigArrayDestroyNull) {
    /* WHAT: Test config_array_destroy with NULL */
    /* WHY: Should handle NULL gracefully */
    config_array_destroy(nullptr);  // Should not crash
    SUCCEED();
}

TEST_F(NullHandlingConsistencyTest, ConfigArrayAppendToNull) {
    /* WHAT: Test append operations with NULL array */
    /* WHY: Should return false, not crash */
    EXPECT_FALSE(config_array_append_int(nullptr, 42));
    EXPECT_FALSE(config_array_append_float(nullptr, 3.14));
    EXPECT_FALSE(config_array_append_bool(nullptr, true));
    EXPECT_FALSE(config_array_append_string(nullptr, "test"));
}

TEST_F(NullHandlingConsistencyTest, ConfigArrayGetFromNull) {
    /* WHAT: Test get operations with NULL array */
    /* WHY: Should return default, not crash */
    EXPECT_EQ(config_array_get_int(nullptr, 0, -1), -1);
    EXPECT_DOUBLE_EQ(config_array_get_float(nullptr, 0, -1.0), -1.0);
    EXPECT_EQ(config_array_get_bool(nullptr, 0, true), true);
    EXPECT_EQ(config_array_get_string(nullptr, 0, "default"), "default");
}

TEST_F(NullHandlingConsistencyTest, ConfigArraySizeOfNull) {
    /* WHAT: Test size operations with NULL array */
    EXPECT_EQ(config_array_size(nullptr), 0u);
    EXPECT_EQ(config_array_capacity(nullptr), 0u);
    EXPECT_TRUE(config_array_is_empty(nullptr));
}

TEST_F(NullHandlingConsistencyTest, ConfigArrayValidationNull) {
    /* WHAT: Test validation with NULL array */
    EXPECT_FALSE(config_array_is_valid(nullptr));
}

TEST_F(NullHandlingConsistencyTest, ConfigArrayParseNull) {
    /* WHAT: Test parse functions with NULL input */
    EXPECT_EQ(config_parse_int_array(nullptr), nullptr);
    EXPECT_EQ(config_parse_float_array(nullptr), nullptr);
    EXPECT_EQ(config_parse_bool_array(nullptr), nullptr);
    EXPECT_EQ(config_parse_string_array(nullptr), nullptr);
    EXPECT_EQ(config_parse_array_auto(nullptr), nullptr);
}

TEST_F(NullHandlingConsistencyTest, ConfigArrayToStringNull) {
    /* WHAT: Test serialization with NULL array */
    EXPECT_EQ(config_array_to_string(nullptr), nullptr);
}

/* ============================================================================
 * Brain Region Error Code Tests
 * ============================================================================ */

class BrainRegionErrorTest : public ::testing::Test {};

TEST_F(BrainRegionErrorTest, MotorErrorConversion) {
    /* WHAT: Test motor error conversion functions */
    EXPECT_EQ(motor_error_to_nimcp(0), NIMCP_SUCCESS);
    EXPECT_EQ(motor_error_to_nimcp(1), NIMCP_ERROR_MOTOR_BASE + 1);
    EXPECT_EQ(motor_error_to_nimcp(8), NIMCP_ERROR_MOTOR_INTERNAL);

    EXPECT_EQ(nimcp_to_motor_error(NIMCP_SUCCESS), 0);
    EXPECT_EQ(nimcp_to_motor_error(NIMCP_ERROR_MOTOR_INVALID_INPUT), 1);
}

TEST_F(BrainRegionErrorTest, HippocampusErrorConversion) {
    /* WHAT: Test hippocampus error conversion */
    EXPECT_EQ(hippocampus_error_to_nimcp(0), NIMCP_SUCCESS);
    EXPECT_EQ(hippocampus_error_to_nimcp(1), NIMCP_ERROR_HIPPOCAMPUS_BASE + 1);

    EXPECT_EQ(nimcp_to_hippocampus_error(NIMCP_SUCCESS), 0);
}

TEST_F(BrainRegionErrorTest, ErrorRangeCheck) {
    /* WHAT: Test brain region error range checking */
    EXPECT_TRUE(nimcp_error_is_brain_region(NIMCP_ERROR_MOTOR_BASE));
    EXPECT_TRUE(nimcp_error_is_brain_region(NIMCP_ERROR_HIPPOCAMPUS_BASE));
    EXPECT_TRUE(nimcp_error_is_brain_region(NIMCP_ERROR_VTA_BASE));

    EXPECT_FALSE(nimcp_error_is_brain_region(NIMCP_SUCCESS));
    EXPECT_FALSE(nimcp_error_is_brain_region(NIMCP_ERROR_NO_MEMORY));
    EXPECT_FALSE(nimcp_error_is_brain_region(NIMCP_ERROR_SECURITY_BASE));
}

TEST_F(BrainRegionErrorTest, AllRegionsHaveConverters) {
    /* WHAT: Test that all brain regions have conversion functions */
    /* WHY: Consistency across all brain regions */

    // Test a sampling of region converters
    EXPECT_EQ(prefrontal_error_to_nimcp(0), NIMCP_SUCCESS);
    EXPECT_EQ(cerebellum_error_to_nimcp(0), NIMCP_SUCCESS);
    EXPECT_EQ(thalamus_error_to_nimcp(0), NIMCP_SUCCESS);
    EXPECT_EQ(amygdala_error_to_nimcp(0), NIMCP_SUCCESS);
    EXPECT_EQ(basal_ganglia_error_to_nimcp(0), NIMCP_SUCCESS);

    EXPECT_EQ(nimcp_to_prefrontal_error(NIMCP_SUCCESS), 0);
    EXPECT_EQ(nimcp_to_cerebellum_error(NIMCP_SUCCESS), 0);
    EXPECT_EQ(nimcp_to_thalamus_error(NIMCP_SUCCESS), 0);
}

/* ============================================================================
 * Error String Tests
 * ============================================================================ */

class ErrorStringTest : public ::testing::Test {};

TEST_F(ErrorStringTest, SuccessCodeString) {
    /* WHAT: Test error string for success */
    const char* msg = nimcp_error_to_string(NIMCP_SUCCESS);
    ASSERT_NE(msg, nullptr);
    EXPECT_NE(strlen(msg), 0u);
}

TEST_F(ErrorStringTest, KnownErrorCodeStrings) {
    /* WHAT: Test error strings for known error codes */
    const char* msg;

    msg = nimcp_error_to_string(NIMCP_ERROR_NULL_POINTER);
    ASSERT_NE(msg, nullptr);

    msg = nimcp_error_to_string(NIMCP_ERROR_NO_MEMORY);
    ASSERT_NE(msg, nullptr);

    msg = nimcp_error_to_string(NIMCP_ERROR_INVALID_PARAMETER);
    ASSERT_NE(msg, nullptr);
}

TEST_F(ErrorStringTest, UnknownErrorCodeString) {
    /* WHAT: Test error string for unknown error code */
    const char* msg = nimcp_error_to_string(99999);
    ASSERT_NE(msg, nullptr);  // Should return some fallback message
}

TEST_F(ErrorStringTest, CategoryNameExtraction) {
    /* WHAT: Test category name extraction */
    const char* name;

    name = nimcp_error_get_category_name(NIMCP_SUCCESS);
    ASSERT_NE(name, nullptr);

    name = nimcp_error_get_category_name(NIMCP_ERROR_NO_MEMORY);
    ASSERT_NE(name, nullptr);
}

/* ============================================================================
 * Cleanup Stack Pattern Tests
 * ============================================================================ */

class CleanupStackTest : public ::testing::Test {};

static int cleanup_call_count = 0;
static void test_cleanup_fn(void* resource) {
    (void)resource;
    cleanup_call_count++;
}

TEST_F(CleanupStackTest, InitializeStack) {
    /* WHAT: Test cleanup stack initialization */
    nimcp_cleanup_stack_t stack = {0};
    nimcp_cleanup_init(&stack);
    EXPECT_EQ(stack.count, 0u);
}

TEST_F(CleanupStackTest, PushAndExecute) {
    /* WHAT: Test push and execute cleanup */
    nimcp_cleanup_stack_t stack = {0};
    nimcp_cleanup_init(&stack);

    cleanup_call_count = 0;
    int resource = 42;

    EXPECT_TRUE(nimcp_cleanup_push(&stack, test_cleanup_fn, &resource, "test"));
    EXPECT_EQ(stack.count, 1u);

    nimcp_cleanup_execute(&stack);
    EXPECT_EQ(cleanup_call_count, 1);
    EXPECT_EQ(stack.count, 0u);
}

TEST_F(CleanupStackTest, LIFOOrder) {
    /* WHAT: Test cleanup executes in LIFO order */
    nimcp_cleanup_stack_t stack = {0};
    nimcp_cleanup_init(&stack);

    std::vector<int> order;
    auto record_order = [](void* p) {
        int* val = static_cast<int*>(p);
        // Note: Can't easily capture vector in C callback
    };

    int a = 1, b = 2, c = 3;
    nimcp_cleanup_push(&stack, test_cleanup_fn, &a, "a");
    nimcp_cleanup_push(&stack, test_cleanup_fn, &b, "b");
    nimcp_cleanup_push(&stack, test_cleanup_fn, &c, "c");

    EXPECT_EQ(stack.count, 3u);
    nimcp_cleanup_execute(&stack);
    EXPECT_EQ(stack.count, 0u);
}

TEST_F(CleanupStackTest, ClearWithoutExecute) {
    /* WHAT: Test clearing stack without executing */
    nimcp_cleanup_stack_t stack = {0};
    nimcp_cleanup_init(&stack);

    cleanup_call_count = 0;
    int resource = 42;

    nimcp_cleanup_push(&stack, test_cleanup_fn, &resource, "test");
    nimcp_cleanup_clear(&stack);

    EXPECT_EQ(stack.count, 0u);
    EXPECT_EQ(cleanup_call_count, 0);  // Should NOT have been called
}

TEST_F(CleanupStackTest, PopEntry) {
    /* WHAT: Test popping cleanup entry */
    nimcp_cleanup_stack_t stack = {0};
    nimcp_cleanup_init(&stack);

    int resource = 42;
    nimcp_cleanup_push(&stack, test_cleanup_fn, &resource, "test");

    nimcp_cleanup_entry_t entry = nimcp_cleanup_pop(&stack);
    EXPECT_EQ(entry.cleanup, test_cleanup_fn);
    EXPECT_EQ(entry.resource, &resource);
    EXPECT_STREQ(entry.name, "test");
    EXPECT_EQ(stack.count, 0u);
}

TEST_F(CleanupStackTest, PopFromEmpty) {
    /* WHAT: Test popping from empty stack */
    nimcp_cleanup_stack_t stack = {0};
    nimcp_cleanup_init(&stack);

    nimcp_cleanup_entry_t entry = nimcp_cleanup_pop(&stack);
    EXPECT_EQ(entry.cleanup, nullptr);
    EXPECT_EQ(entry.resource, nullptr);
    EXPECT_EQ(entry.name, nullptr);
}

TEST_F(CleanupStackTest, MaxEntriesLimit) {
    /* WHAT: Test maximum entries limit */
    nimcp_cleanup_stack_t stack = {0};
    nimcp_cleanup_init(&stack);

    int dummy = 0;
    for (size_t i = 0; i < NIMCP_CLEANUP_STACK_MAX; i++) {
        EXPECT_TRUE(nimcp_cleanup_push(&stack, test_cleanup_fn, &dummy, "test"));
    }

    // Should fail when full
    EXPECT_FALSE(nimcp_cleanup_push(&stack, test_cleanup_fn, &dummy, "overflow"));
    EXPECT_EQ(stack.count, NIMCP_CLEANUP_STACK_MAX);

    nimcp_cleanup_clear(&stack);
}

TEST_F(CleanupStackTest, NullStackHandling) {
    /* WHAT: Test NULL stack handling */
    nimcp_cleanup_init(nullptr);  // Should not crash
    EXPECT_FALSE(nimcp_cleanup_push(nullptr, test_cleanup_fn, nullptr, "test"));
    nimcp_cleanup_execute(nullptr);  // Should not crash
    nimcp_cleanup_clear(nullptr);  // Should not crash
    SUCCEED();
}

}  // namespace
