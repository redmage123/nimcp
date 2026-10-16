// test_overflow_checks.cpp - Tests for NIMCP_MUL_SAFE/NIMCP_ADD_SAFE macros
// Tests P3-7: Overflow check macros for size calculations
#include <gtest/gtest.h>
#include <climits>
#include <cstddef>

#include "utils/nimcp_overflow.h"

// --- Multiplication Safety ---
TEST(OverflowChecks, MulSafe_SmallValues) {
    EXPECT_TRUE(NIMCP_MUL_SAFE(10, 20));
    EXPECT_TRUE(NIMCP_MUL_SAFE(1, 1));
    EXPECT_TRUE(NIMCP_MUL_SAFE(100, 100));
}

TEST(OverflowChecks, MulSafe_ZeroValues) {
    EXPECT_TRUE(NIMCP_MUL_SAFE(0, SIZE_MAX));
    EXPECT_TRUE(NIMCP_MUL_SAFE(SIZE_MAX, 0));
    EXPECT_TRUE(NIMCP_MUL_SAFE(0, 0));
}

TEST(OverflowChecks, MulSafe_OneValues) {
    EXPECT_TRUE(NIMCP_MUL_SAFE(1, SIZE_MAX));
    EXPECT_TRUE(NIMCP_MUL_SAFE(SIZE_MAX, 1));
}

TEST(OverflowChecks, MulSafe_Overflow) {
    EXPECT_FALSE(NIMCP_MUL_SAFE(SIZE_MAX, 2));
    EXPECT_FALSE(NIMCP_MUL_SAFE(2, SIZE_MAX));
    EXPECT_FALSE(NIMCP_MUL_SAFE(SIZE_MAX, SIZE_MAX));
}

TEST(OverflowChecks, MulSafe_NearOverflow) {
    size_t half = SIZE_MAX / 2;
    EXPECT_TRUE(NIMCP_MUL_SAFE(half, 2));
    EXPECT_FALSE(NIMCP_MUL_SAFE(half + 1, 2));
}

// --- Addition Safety ---
TEST(OverflowChecks, AddSafe_SmallValues) {
    EXPECT_TRUE(NIMCP_ADD_SAFE(10, 20));
    EXPECT_TRUE(NIMCP_ADD_SAFE(0, 0));
    EXPECT_TRUE(NIMCP_ADD_SAFE(1, 1));
}

TEST(OverflowChecks, AddSafe_ZeroValues) {
    EXPECT_TRUE(NIMCP_ADD_SAFE(0, SIZE_MAX));
    EXPECT_TRUE(NIMCP_ADD_SAFE(SIZE_MAX, 0));
}

TEST(OverflowChecks, AddSafe_Overflow) {
    EXPECT_FALSE(NIMCP_ADD_SAFE(SIZE_MAX, 1));
    EXPECT_FALSE(NIMCP_ADD_SAFE(1, SIZE_MAX));
    EXPECT_FALSE(NIMCP_ADD_SAFE(SIZE_MAX, SIZE_MAX));
}

TEST(OverflowChecks, AddSafe_NearOverflow) {
    EXPECT_TRUE(NIMCP_ADD_SAFE(SIZE_MAX - 1, 1));
    EXPECT_FALSE(NIMCP_ADD_SAFE(SIZE_MAX - 1, 2));
}

// --- Result variants ---
TEST(OverflowChecks, MulSafeResult_OK) {
    size_t result = 0;
    EXPECT_TRUE(NIMCP_MUL_SAFE_RESULT(10, 20, &result));
    EXPECT_EQ(result, 200u);
}

TEST(OverflowChecks, MulSafeResult_Overflow) {
    size_t result = 42;
    EXPECT_FALSE(NIMCP_MUL_SAFE_RESULT(SIZE_MAX, 2, &result));
    EXPECT_EQ(result, 42u);
}

TEST(OverflowChecks, MulSafeResult_NullResult) {
    EXPECT_TRUE(NIMCP_MUL_SAFE_RESULT(10, 20, nullptr));
}

TEST(OverflowChecks, AddSafeResult_OK) {
    size_t result = 0;
    EXPECT_TRUE(NIMCP_ADD_SAFE_RESULT(100, 200, &result));
    EXPECT_EQ(result, 300u);
}

TEST(OverflowChecks, AddSafeResult_Overflow) {
    size_t result = 42;
    EXPECT_FALSE(NIMCP_ADD_SAFE_RESULT(SIZE_MAX, 1, &result));
    EXPECT_EQ(result, 42u);
}

TEST(OverflowChecks, AddSafeResult_NullResult) {
    EXPECT_TRUE(NIMCP_ADD_SAFE_RESULT(10, 20, nullptr));
}

// --- Inline function variants ---
TEST(OverflowChecks, InlineMulSafe) {
    EXPECT_TRUE(nimcp_mul_safe(100, 100));
    EXPECT_FALSE(nimcp_mul_safe(SIZE_MAX, 2));
}

TEST(OverflowChecks, InlineAddSafe) {
    EXPECT_TRUE(nimcp_add_safe(100, 100));
    EXPECT_FALSE(nimcp_add_safe(SIZE_MAX, 1));
}

TEST(OverflowChecks, InlineMulSafeResult) {
    size_t r = 0;
    EXPECT_TRUE(nimcp_mul_safe_result(5, 6, &r));
    EXPECT_EQ(r, 30u);
}

TEST(OverflowChecks, InlineAddSafeResult) {
    size_t r = 0;
    EXPECT_TRUE(nimcp_add_safe_result(11, 22, &r));
    EXPECT_EQ(r, 33u);
}

// --- Practical allocation size checks ---
TEST(OverflowChecks, AllocationSizeCheck) {
    size_t count = 1000000;
    size_t elem_size = sizeof(double);
    size_t total = 0;
    EXPECT_TRUE(NIMCP_MUL_SAFE_RESULT(count, elem_size, &total));
    EXPECT_EQ(total, count * elem_size);
}

TEST(OverflowChecks, AllocationSizeOverflow) {
    size_t count = SIZE_MAX / 2;
    size_t elem_size = 4;
    EXPECT_FALSE(NIMCP_MUL_SAFE(count, elem_size));
}

TEST(OverflowChecks, HeaderPlusSizeOverflow) {
    size_t header = 64;
    size_t data = SIZE_MAX - 32;
    EXPECT_FALSE(NIMCP_ADD_SAFE(header, data));
}
