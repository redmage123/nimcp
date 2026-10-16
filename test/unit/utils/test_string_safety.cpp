/**
 * @file test_string_safety.cpp
 * @brief Unit tests for string safety functions and buffer handling
 *
 * WHAT: Tests for safe string operations (snprintf, strncpy patterns)
 * WHY:  Ensure buffer overflow prevention and correct truncation behavior
 * HOW:  GTest parameterized tests for boundary conditions
 *
 * TEST CATEGORIES:
 * - snprintf boundary conditions
 * - String truncation handling
 * - NULL input handling
 * - Buffer size edge cases
 * - UTF-8 handling considerations
 *
 * @author NIMCP Development Team
 * @date 2026-02-02
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <tuple>

extern "C" {
#include "utils/memory/nimcp_memory.h"
}

namespace {

/* ============================================================================
 * Base Test Fixture
 * ============================================================================ */

class StringSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
    }

    void TearDown() override {
        nimcp_memory_cleanup();
    }
};

/* ============================================================================
 * snprintf Boundary Condition Tests
 * ============================================================================ */

class SnprintfBoundaryTest : public StringSafetyTest {};

TEST_F(SnprintfBoundaryTest, ExactFitBuffer) {
    /* WHAT: Test snprintf when output exactly fits buffer */
    /* WHY: Ensure no off-by-one errors */
    char buffer[6];  // "hello" + null = 6
    int written = snprintf(buffer, sizeof(buffer), "hello");

    EXPECT_EQ(written, 5);
    EXPECT_STREQ(buffer, "hello");
    EXPECT_EQ(buffer[5], '\0');  // Null terminator present
}

TEST_F(SnprintfBoundaryTest, BufferTooSmallTruncates) {
    /* WHAT: Test snprintf truncation behavior */
    /* WHY: Verify safe truncation without overflow */
    char buffer[4];
    int written = snprintf(buffer, sizeof(buffer), "hello");

    // snprintf returns what WOULD have been written (5), not what was written (3)
    EXPECT_EQ(written, 5);
    EXPECT_STREQ(buffer, "hel");  // Truncated with null terminator
    EXPECT_EQ(strlen(buffer), 3u);
}

TEST_F(SnprintfBoundaryTest, ZeroSizeBuffer) {
    /* WHAT: Test snprintf with zero-size buffer */
    /* WHY: Edge case that should not write anything */
    char buffer[10] = "original";
    int written = snprintf(buffer, 0, "new content");

    // Returns would-have-been-written count
    EXPECT_EQ(written, 11);
    // Buffer should be unchanged
    EXPECT_STREQ(buffer, "original");
}

TEST_F(SnprintfBoundaryTest, OneSizeBuffer) {
    /* WHAT: Test snprintf with size-1 buffer (null terminator only) */
    /* WHY: Edge case that should write only null */
    char buffer[1] = {'X'};
    int written = snprintf(buffer, 1, "hello");

    EXPECT_EQ(written, 5);  // Would have written 5 chars
    EXPECT_EQ(buffer[0], '\0');  // Only null terminator written
}

TEST_F(SnprintfBoundaryTest, FormatWithVariableWidth) {
    /* WHAT: Test snprintf with format specifiers */
    /* WHY: Ensure format specifiers don't cause overflow */
    char buffer[32];
    double value = 3.14159265358979;
    int written = snprintf(buffer, sizeof(buffer), "%.6f", value);

    EXPECT_GT(written, 0);
    EXPECT_LT((size_t)written, sizeof(buffer));
    EXPECT_STREQ(buffer, "3.141593");
}

TEST_F(SnprintfBoundaryTest, LargeNumberFormatting) {
    /* WHAT: Test snprintf with large numbers */
    /* WHY: Large numbers need more buffer space */
    char buffer[32];
    int64_t large_num = 9223372036854775807LL;  // INT64_MAX
    int written = snprintf(buffer, sizeof(buffer), "%ld", (long)large_num);

    EXPECT_GT(written, 0);
    EXPECT_LT((size_t)written, sizeof(buffer));
}

TEST_F(SnprintfBoundaryTest, NegativeReturnHandling) {
    /* WHAT: Document snprintf behavior on encoding errors */
    /* WHY: Some implementations return negative on error */
    char buffer[8];
    // Normal case should not return negative
    int written = snprintf(buffer, sizeof(buffer), "test");
    EXPECT_GE(written, 0);
}

/* ============================================================================
 * Parameterized Tests for Buffer Size Boundaries
 * ============================================================================ */

class SnprintfParameterizedTest : public StringSafetyTest,
    public ::testing::WithParamInterface<std::tuple<size_t, const char*, size_t>> {
    // Tuple: (buffer_size, input_string, expected_strlen_after)
};

TEST_P(SnprintfParameterizedTest, TruncationBehavior) {
    auto [buf_size, input, expected_len] = GetParam();

    std::vector<char> buffer(buf_size + 1, 'X');  // Extra byte to detect overflow
    if (buf_size > 0) {
        snprintf(buffer.data(), buf_size, "%s", input);

        // Check truncation
        EXPECT_EQ(strlen(buffer.data()), expected_len);
        // Check no overflow into extra byte
        EXPECT_EQ(buffer[buf_size], 'X');
    }
}

INSTANTIATE_TEST_SUITE_P(
    BufferSizes,
    SnprintfParameterizedTest,
    ::testing::Values(
        std::make_tuple(1, "hello", 0),     // Size 1 = only null
        std::make_tuple(2, "hello", 1),     // Size 2 = 1 char + null
        std::make_tuple(5, "hello", 4),     // Size 5 = 4 chars + null
        std::make_tuple(6, "hello", 5),     // Size 6 = exact fit
        std::make_tuple(10, "hello", 5),    // Size 10 = room to spare
        std::make_tuple(100, "hello", 5)    // Large buffer
    )
);

/* ============================================================================
 * String Truncation Handling Tests
 * ============================================================================ */

class StringTruncationTest : public StringSafetyTest {};

TEST_F(StringTruncationTest, SafeCopyWithTruncation) {
    /* WHAT: Test safe string copy with explicit truncation */
    /* WHY: Pattern used in embodied_simulation.c reason strings */
    char dest[16];
    const char* src = "This is a very long string that exceeds the buffer";

    // Safe copy pattern: snprintf guarantees null termination
    snprintf(dest, sizeof(dest), "%s", src);

    EXPECT_EQ(strlen(dest), 15u);  // 16 - 1 for null
    EXPECT_EQ(dest[15], '\0');
    // Verify it's a prefix of the original
    EXPECT_EQ(strncmp(dest, src, 15), 0);
}

TEST_F(StringTruncationTest, TruncationDetection) {
    /* WHAT: Test detecting when truncation occurred */
    /* WHY: May need to warn user about truncated data */
    char buffer[10];
    const char* input = "This exceeds ten chars";
    int written = snprintf(buffer, sizeof(buffer), "%s", input);

    bool was_truncated = (size_t)written >= sizeof(buffer);
    EXPECT_TRUE(was_truncated);
}

TEST_F(StringTruncationTest, NoTruncationDetection) {
    /* WHAT: Test when no truncation occurs */
    char buffer[100];
    const char* input = "Short";
    int written = snprintf(buffer, sizeof(buffer), "%s", input);

    bool was_truncated = (size_t)written >= sizeof(buffer);
    EXPECT_FALSE(was_truncated);
}

TEST_F(StringTruncationTest, MultipleFormatsInSnprintf) {
    /* WHAT: Test multiple format specifiers */
    /* WHY: Common pattern in error messages */
    char buffer[64];
    const char* name = "target";
    double dist = 1.5;
    double max_dist = 1.0;

    int written = snprintf(buffer, sizeof(buffer),
        "Target out of reach (%.2fm > %.2fm)", dist, max_dist);

    EXPECT_GT(written, 0);
    EXPECT_LT((size_t)written, sizeof(buffer));
    EXPECT_NE(strstr(buffer, "1.50"), nullptr);
    EXPECT_NE(strstr(buffer, "1.00"), nullptr);
}

/* ============================================================================
 * NULL Input Handling Tests
 * ============================================================================ */

class NullInputHandlingTest : public StringSafetyTest {};

TEST_F(NullInputHandlingTest, SnprintfNullBuffer) {
    /* WHAT: Test snprintf with NULL buffer and size 0 */
    /* WHY: Valid use case to calculate required size */
    int needed = snprintf(NULL, 0, "test %d string", 42);
    EXPECT_EQ(needed, 14);  // "test 42 string" = 14 chars
}

TEST_F(NullInputHandlingTest, StrlenNullHandling) {
    /* WHAT: Document that strlen with NULL is UB */
    /* WHY: Need explicit NULL checks before strlen */
    const char* ptr = "test";
    // This is safe
    EXPECT_EQ(strlen(ptr), 4u);

    // NOTE: strlen(NULL) is undefined behavior
    // Always check for NULL before calling strlen
}

TEST_F(NullInputHandlingTest, StrcmpNullHandling) {
    /* WHAT: Document strcmp with NULL is UB */
    /* WHY: Need explicit NULL checks before strcmp */
    const char* a = "test";
    const char* b = "test";

    EXPECT_EQ(strcmp(a, b), 0);
    // NOTE: strcmp(NULL, ...) or strcmp(..., NULL) is undefined behavior
}

/* ============================================================================
 * Embedded Null Character Tests
 * ============================================================================ */

class EmbeddedNullTest : public StringSafetyTest {};

TEST_F(EmbeddedNullTest, SnprintfStopsAtNull) {
    /* WHAT: Test that snprintf %s stops at embedded null */
    /* WHY: Standard behavior but worth documenting */
    char input[] = {'h', 'e', 'l', '\0', 'l', 'o', '\0'};
    char buffer[10];

    snprintf(buffer, sizeof(buffer), "%s", input);

    EXPECT_STREQ(buffer, "hel");
    EXPECT_EQ(strlen(buffer), 3u);
}

TEST_F(EmbeddedNullTest, MemcpyPreservesEmbeddedNull) {
    /* WHAT: Test that memcpy preserves embedded nulls */
    /* WHY: Different from string functions */
    char input[] = {'h', 'e', 'l', '\0', 'l', 'o'};
    char buffer[6];

    memcpy(buffer, input, 6);

    EXPECT_EQ(buffer[3], '\0');
    EXPECT_EQ(buffer[4], 'l');
}

/* ============================================================================
 * UTF-8 Boundary Tests
 * ============================================================================ */

class UTF8BoundaryTest : public StringSafetyTest {};

TEST_F(UTF8BoundaryTest, AsciiOnlyBuffer) {
    /* WHAT: Test ASCII-only strings work correctly */
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "ASCII only");
    EXPECT_STREQ(buffer, "ASCII only");
}

TEST_F(UTF8BoundaryTest, UTF8MultibyteCharacters) {
    /* WHAT: Test UTF-8 multibyte character handling */
    /* WHY: Truncation mid-character can produce invalid UTF-8 */
    const char* utf8_input = "Hello \xC3\xA9 world";  // e with acute
    char buffer[10];

    snprintf(buffer, sizeof(buffer), "%s", utf8_input);

    // snprintf doesn't understand UTF-8, may truncate mid-character
    EXPECT_LE(strlen(buffer), 9u);
    // Note: Result may be invalid UTF-8 if truncated in middle of multibyte char
}

TEST_F(UTF8BoundaryTest, UTF8LengthVsByteLength) {
    /* WHAT: Document difference between byte length and character length */
    const char* utf8_str = "\xC3\xA9";  // e with acute = 2 bytes, 1 character

    EXPECT_EQ(strlen(utf8_str), 2u);  // strlen counts bytes
    // Character count would be 1 (not tested here as we don't have Unicode lib)
}

/* ============================================================================
 * NIMCP Simulation Reason Buffer Tests
 * ============================================================================ */

// These tests specifically cover the patterns used in embodied_simulation.c

#define NIMCP_SIMULATION_REASON_MAX 64

class SimulationReasonBufferTest : public StringSafetyTest {};

TEST_F(SimulationReasonBufferTest, ReasonBufferExactSize) {
    /* WHAT: Test reason buffer with exact size message */
    /* Pattern from: nimcp_embodied_simulation.c */
    char reason[NIMCP_SIMULATION_REASON_MAX];

    snprintf(reason, NIMCP_SIMULATION_REASON_MAX, "Effector not found");

    EXPECT_LT(strlen(reason), NIMCP_SIMULATION_REASON_MAX);
    EXPECT_STREQ(reason, "Effector not found");
}

TEST_F(SimulationReasonBufferTest, ReasonBufferWithFormatting) {
    /* WHAT: Test reason buffer with formatted values */
    char reason[NIMCP_SIMULATION_REASON_MAX];
    double dist = 2.5;
    double max_reach = 1.0;

    snprintf(reason, NIMCP_SIMULATION_REASON_MAX,
             "Target out of reach (%.2fm > %.2fm)", dist, max_reach);

    EXPECT_LT(strlen(reason), NIMCP_SIMULATION_REASON_MAX);
    EXPECT_NE(strstr(reason, "2.50"), nullptr);
}

TEST_F(SimulationReasonBufferTest, ReasonBufferTruncation) {
    /* WHAT: Test that overly long reasons are safely truncated */
    char reason[NIMCP_SIMULATION_REASON_MAX];

    // Create a very long format string
    char long_format[256];
    memset(long_format, 'A', sizeof(long_format) - 1);
    long_format[sizeof(long_format) - 1] = '\0';

    int written = snprintf(reason, NIMCP_SIMULATION_REASON_MAX, "%s", long_format);

    // Written would be 255, but buffer only holds 63 chars + null
    EXPECT_GT(written, NIMCP_SIMULATION_REASON_MAX);
    EXPECT_EQ(strlen(reason), NIMCP_SIMULATION_REASON_MAX - 1);
    EXPECT_EQ(reason[NIMCP_SIMULATION_REASON_MAX - 1], '\0');
}

TEST_F(SimulationReasonBufferTest, NullReasonPointerSafety) {
    /* WHAT: Test NULL reason pointer handling */
    /* Pattern: if (reason) { snprintf(...) } */
    char* reason = nullptr;

    // This pattern is used in embodied_simulation.c
    if (reason) {
        snprintf(reason, NIMCP_SIMULATION_REASON_MAX, "Feasible");
    }
    // Should not crash - the if check prevents NULL dereference
    SUCCEED();
}

/* ============================================================================
 * Config Array Formatting Tests
 * ============================================================================ */

class ConfigArrayFormattingTest : public StringSafetyTest {};

TEST_F(ConfigArrayFormattingTest, IntArrayFormatting) {
    /* WHAT: Test integer array formatting */
    /* Pattern from: nimcp_config_array.c config_array_to_string */
    char buffer[64];
    int64_t values[] = {1, 2, 3, 4, 5};
    size_t count = 5;

    size_t remaining = sizeof(buffer);
    char* p = buffer;

    int written = snprintf(p, remaining, "[");
    ASSERT_GT(written, 0);
    ASSERT_LT((size_t)written, remaining);
    p += written;
    remaining -= (size_t)written;

    for (size_t i = 0; i < count && remaining > 0; i++) {
        if (i > 0) {
            written = snprintf(p, remaining, ", ");
            if (written < 0 || (size_t)written >= remaining) break;
            p += written;
            remaining -= (size_t)written;
        }

        written = snprintf(p, remaining, "%ld", (long)values[i]);
        if (written < 0 || (size_t)written >= remaining) break;
        p += written;
        remaining -= (size_t)written;
    }

    if (remaining > 0) {
        snprintf(p, remaining, "]");
    }

    EXPECT_STREQ(buffer, "[1, 2, 3, 4, 5]");
}

TEST_F(ConfigArrayFormattingTest, BufferOverflowPrevention) {
    /* WHAT: Test buffer overflow prevention in array formatting */
    char buffer[16];  // Too small for full output
    size_t remaining = sizeof(buffer);
    char* p = buffer;

    // Simulate formatting a large array
    int written = snprintf(p, remaining, "[1, 2, 3, 4, 5, 6, 7, 8, 9, 10]");

    // snprintf returns what would have been written
    EXPECT_GT(written, (int)sizeof(buffer));
    // But buffer is safely truncated
    EXPECT_EQ(strlen(buffer), sizeof(buffer) - 1);
}

TEST_F(ConfigArrayFormattingTest, EmptyArrayFormatting) {
    /* WHAT: Test empty array formatting */
    char buffer[10];

    snprintf(buffer, sizeof(buffer), "[]");

    EXPECT_STREQ(buffer, "[]");
}

TEST_F(ConfigArrayFormattingTest, LargeDataHandling) {
    /* WHAT: Test formatting with very large numbers */
    char buffer[128];
    int64_t large_val = 9223372036854775807LL;  // INT64_MAX

    int written = snprintf(buffer, sizeof(buffer), "[%ld]", (long)large_val);

    EXPECT_GT(written, 0);
    EXPECT_LT((size_t)written, sizeof(buffer));
}

}  // namespace
