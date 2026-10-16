/**
 * @file test_overflow_protection.cpp
 * @brief Tests for overflow protection macros (P2-7 + P3-7)
 *
 * WHAT: Verify safe arithmetic macros detect overflow correctly
 * WHY:  P2-7 and P3-7 added NIMCP_MUL_SAFE/NIMCP_ADD_SAFE for size calculations
 * HOW:  Test boundary conditions, normal values, overflow cases
 *
 * NOTE: test_overflow_checks.cpp also tests these macros. This file adds
 * additional coverage for integration-level patterns and edge cases
 * specifically related to the P2-7/P3-7 fixes.
 *
 * Functions tested (from include/utils/nimcp_overflow.h):
 *   static inline bool nimcp_mul_safe(size_t a, size_t b);
 *   static inline bool nimcp_add_safe(size_t a, size_t b);
 *   static inline bool nimcp_mul_safe_result(size_t a, size_t b, size_t* result);
 *   static inline bool nimcp_add_safe_result(size_t a, size_t b, size_t* result);
 *
 * Macros:
 *   NIMCP_MUL_SAFE(a, b)
 *   NIMCP_ADD_SAFE(a, b)
 *   NIMCP_MUL_SAFE_RESULT(a, b, r)
 *   NIMCP_ADD_SAFE_RESULT(a, b, r)
 */

#include <gtest/gtest.h>
#include <climits>
#include <cstddef>

#include "utils/nimcp_overflow.h"

/* ============================================================================
 * Safe Addition - No Overflow
 * ============================================================================ */

TEST(OverflowProtection, AddSafeNoOverflow_SmallValues) {
    EXPECT_TRUE(nimcp_add_safe(0, 0));
    EXPECT_TRUE(nimcp_add_safe(1, 1));
    EXPECT_TRUE(nimcp_add_safe(100, 200));
    EXPECT_TRUE(nimcp_add_safe(1024, 4096));
}

TEST(OverflowProtection, AddSafeNoOverflow_LargeValues) {
    size_t half = SIZE_MAX / 2;
    EXPECT_TRUE(nimcp_add_safe(half, half));
    EXPECT_TRUE(nimcp_add_safe(half - 1, half));
    EXPECT_TRUE(nimcp_add_safe(SIZE_MAX - 1, 1));
}

/* ============================================================================
 * Safe Addition - Overflow Detection
 * ============================================================================ */

TEST(OverflowProtection, AddSafeDetectsOverflow) {
    EXPECT_FALSE(nimcp_add_safe(SIZE_MAX, 1));
    EXPECT_FALSE(nimcp_add_safe(SIZE_MAX, SIZE_MAX));

    size_t half = SIZE_MAX / 2;
    EXPECT_FALSE(nimcp_add_safe(half + 2, half));
}

TEST(OverflowProtection, AddSafeResultStoresCorrectly) {
    size_t result = 0;

    EXPECT_TRUE(nimcp_add_safe_result(100, 200, &result));
    EXPECT_EQ(result, 300u);

    // On overflow, result should be unchanged
    result = 42;
    EXPECT_FALSE(nimcp_add_safe_result(SIZE_MAX, 1, &result));
    EXPECT_EQ(result, 42u);
}

/* ============================================================================
 * Safe Multiplication - No Overflow
 * ============================================================================ */

TEST(OverflowProtection, MulSafeNoOverflow_SmallValues) {
    EXPECT_TRUE(nimcp_mul_safe(0, 0));
    EXPECT_TRUE(nimcp_mul_safe(1, 1));
    EXPECT_TRUE(nimcp_mul_safe(100, 100));
    EXPECT_TRUE(nimcp_mul_safe(1000, 1000));
}

TEST(OverflowProtection, MulSafeNoOverflow_Zero) {
    EXPECT_TRUE(nimcp_mul_safe(0, SIZE_MAX));
    EXPECT_TRUE(nimcp_mul_safe(SIZE_MAX, 0));
}

TEST(OverflowProtection, MulSafeNoOverflow_One) {
    EXPECT_TRUE(nimcp_mul_safe(1, SIZE_MAX));
    EXPECT_TRUE(nimcp_mul_safe(SIZE_MAX, 1));
}

/* ============================================================================
 * Safe Multiplication - Overflow Detection
 * ============================================================================ */

TEST(OverflowProtection, MulSafeDetectsOverflow) {
    EXPECT_FALSE(nimcp_mul_safe(SIZE_MAX, 2));
    EXPECT_FALSE(nimcp_mul_safe(2, SIZE_MAX));
    EXPECT_FALSE(nimcp_mul_safe(SIZE_MAX, SIZE_MAX));
}

TEST(OverflowProtection, MulSafeBoundaryCase) {
    size_t half = SIZE_MAX / 2;
    EXPECT_TRUE(nimcp_mul_safe(half, 2));
    EXPECT_FALSE(nimcp_mul_safe(half + 1, 2));
}

TEST(OverflowProtection, MulSafeResultStoresCorrectly) {
    size_t result = 0;

    EXPECT_TRUE(nimcp_mul_safe_result(10, 20, &result));
    EXPECT_EQ(result, 200u);

    // On overflow, result should be unchanged
    result = 42;
    EXPECT_FALSE(nimcp_mul_safe_result(SIZE_MAX, 2, &result));
    EXPECT_EQ(result, 42u);
}

/* ============================================================================
 * Macro Variants
 * ============================================================================ */

TEST(OverflowProtection, MacroAddSafe) {
    EXPECT_TRUE(NIMCP_ADD_SAFE(10, 20));
    EXPECT_FALSE(NIMCP_ADD_SAFE(SIZE_MAX, 1));
}

TEST(OverflowProtection, MacroMulSafe) {
    EXPECT_TRUE(NIMCP_MUL_SAFE(10, 20));
    EXPECT_FALSE(NIMCP_MUL_SAFE(SIZE_MAX, 2));
}

TEST(OverflowProtection, MacroResultVariants) {
    size_t result = 0;

    EXPECT_TRUE(NIMCP_ADD_SAFE_RESULT(50, 50, &result));
    EXPECT_EQ(result, 100u);

    result = 0;
    EXPECT_TRUE(NIMCP_MUL_SAFE_RESULT(7, 8, &result));
    EXPECT_EQ(result, 56u);
}

/* ============================================================================
 * Realistic Allocation Patterns
 * ============================================================================ */

TEST(OverflowProtection, StructArrayAllocation) {
    // Simulate: allocating an array of structs
    struct large_struct { char data[1024]; };
    size_t count = 1000;
    size_t total = 0;

    EXPECT_TRUE(NIMCP_MUL_SAFE_RESULT(count, sizeof(large_struct), &total));
    EXPECT_EQ(total, count * sizeof(large_struct));
}

TEST(OverflowProtection, HeaderPlusDataPattern) {
    // Simulate: header + variable-length data
    size_t header_size = 64;
    size_t data_size = 4096;
    size_t total = 0;

    EXPECT_TRUE(NIMCP_ADD_SAFE_RESULT(header_size, data_size, &total));
    EXPECT_EQ(total, header_size + data_size);
}

TEST(OverflowProtection, CombinedMulAndAdd) {
    // Simulate: count * elem_size + header
    size_t count = 1000;
    size_t elem_size = sizeof(double);
    size_t header = 128;

    size_t array_size = 0;
    EXPECT_TRUE(NIMCP_MUL_SAFE_RESULT(count, elem_size, &array_size));

    size_t total = 0;
    EXPECT_TRUE(NIMCP_ADD_SAFE_RESULT(array_size, header, &total));
    EXPECT_EQ(total, count * elem_size + header);
}

TEST(OverflowProtection, OverflowInCombinedPattern) {
    // Large count that would overflow when multiplied by element size
    size_t dangerous_count = SIZE_MAX / 4;
    size_t elem_size = 8;

    EXPECT_FALSE(NIMCP_MUL_SAFE(dangerous_count, elem_size));
}

TEST(OverflowProtection, NullResultPointer) {
    // NULL result pointer should still work (just check safety)
    EXPECT_TRUE(NIMCP_MUL_SAFE_RESULT(10, 20, nullptr));
    EXPECT_TRUE(NIMCP_ADD_SAFE_RESULT(10, 20, nullptr));
}
