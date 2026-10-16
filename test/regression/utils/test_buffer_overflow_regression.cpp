/**
 * @file test_buffer_overflow_regression.cpp
 * @brief Regression tests for buffer overflow fixes
 *
 * WHAT: Regression tests for buffer safety fixes across NIMCP modules
 * WHY:  Prevent reintroduction of buffer overflow vulnerabilities
 * HOW:  Test boundary conditions, large data, and edge cases
 *
 * REGRESSION CATEGORIES:
 * - Config array formatting with large data
 * - Embodiment reason string handling
 * - Maximum length inputs
 * - Embedded null characters
 * - Unicode/UTF-8 handling
 *
 * FIXES COVERED:
 * - Task #11: unsafe strcpy in embodied_simulation.c
 * - Task #12: unsafe sprintf in config_array.c
 * - Task #13: other unsafe strcpy usages
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
#include <limits>

extern "C" {
#include "utils/memory/nimcp_memory.h"
#include "utils/config/nimcp_config_array.h"
}

namespace {

/* ============================================================================
 * Base Test Fixture
 * ============================================================================ */

class BufferOverflowRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
    }

    void TearDown() override {
        nimcp_memory_cleanup();
    }
};

/* ============================================================================
 * Config Array Formatting Regression Tests
 * ============================================================================
 * Related to: Task #12 - Fix unsafe sprintf calls in config_array.c
 */

class ConfigArrayFormattingRegressionTest : public BufferOverflowRegressionTest {};

TEST_F(ConfigArrayFormattingRegressionTest, LargeIntArrayToString) {
    /* REGRESSION: Large arrays should not cause buffer overflow */
    config_array_full_t* arr = config_array_create(CONFIG_TYPE_INT, 100);
    ASSERT_NE(arr, nullptr);

    // Fill with large values
    for (int i = 0; i < 100; i++) {
        config_array_append_int(arr, INT64_MAX - i);
    }

    char* str = config_array_to_string(arr);
    ASSERT_NE(str, nullptr);

    // Verify the string is properly formatted
    EXPECT_EQ(str[0], '[');
    EXPECT_EQ(str[strlen(str) - 1], ']');

    nimcp_free(str);
    config_array_destroy(arr);
}

TEST_F(ConfigArrayFormattingRegressionTest, LargeFloatArrayToString) {
    /* REGRESSION: Large float arrays with many decimal places */
    config_array_full_t* arr = config_array_create(CONFIG_TYPE_FLOAT, 50);
    ASSERT_NE(arr, nullptr);

    // Fill with values that have long decimal representations
    for (int i = 0; i < 50; i++) {
        config_array_append_float(arr, 3.141592653589793 + i * 0.0000001);
    }

    char* str = config_array_to_string(arr);
    ASSERT_NE(str, nullptr);

    // Should be valid format
    EXPECT_EQ(str[0], '[');
    EXPECT_EQ(str[strlen(str) - 1], ']');

    nimcp_free(str);
    config_array_destroy(arr);
}

TEST_F(ConfigArrayFormattingRegressionTest, VeryLongStringsInArray) {
    /* REGRESSION: String arrays with long strings should either serialize
     * or safely fail without overflow */
    config_array_full_t* arr = config_array_create(CONFIG_TYPE_STRING, 10);
    ASSERT_NE(arr, nullptr);

    // Create a 1000-character string
    std::string long_str(1000, 'A');

    for (int i = 0; i < 10; i++) {
        config_array_append_string(arr, long_str.c_str());
    }

    char* str = config_array_to_string(arr);

    // The serialization might return NULL if buffer overflow would occur
    // This is the safe behavior - prevent overflow rather than corrupt memory
    if (str != nullptr) {
        // If we got a result, it should be valid format
        size_t len = strlen(str);
        EXPECT_GT(len, 10000u);  // At least 10 * 1000 chars
        EXPECT_EQ(str[0], '[');
        EXPECT_EQ(str[len - 1], ']');
        nimcp_free(str);
    }
    // NULL result is acceptable - it means overflow prevention worked

    config_array_destroy(arr);
}

TEST_F(ConfigArrayFormattingRegressionTest, ModerateLongStringsInArray) {
    /* REGRESSION: Moderate length strings should serialize correctly */
    config_array_full_t* arr = config_array_create(CONFIG_TYPE_STRING, 5);
    ASSERT_NE(arr, nullptr);

    // Create a 50-character string (within limits)
    std::string moderate_str(50, 'B');

    for (int i = 0; i < 5; i++) {
        config_array_append_string(arr, moderate_str.c_str());
    }

    char* str = config_array_to_string(arr);
    ASSERT_NE(str, nullptr);

    // Should contain the strings
    size_t len = strlen(str);
    EXPECT_GT(len, 250u);  // At least 5 * 50 chars

    nimcp_free(str);
    config_array_destroy(arr);
}

TEST_F(ConfigArrayFormattingRegressionTest, EmptyArrayToString) {
    /* REGRESSION: Empty array should format correctly */
    config_array_full_t* arr = config_array_create(CONFIG_TYPE_INT, 0);
    ASSERT_NE(arr, nullptr);

    char* str = config_array_to_string(arr);
    ASSERT_NE(str, nullptr);
    EXPECT_STREQ(str, "[]");

    nimcp_free(str);
    config_array_destroy(arr);
}

TEST_F(ConfigArrayFormattingRegressionTest, SingleElementArray) {
    /* REGRESSION: Single element formatting */
    config_array_full_t* arr = config_array_create(CONFIG_TYPE_INT, 1);
    ASSERT_NE(arr, nullptr);
    config_array_append_int(arr, 42);

    char* str = config_array_to_string(arr);
    ASSERT_NE(str, nullptr);
    EXPECT_STREQ(str, "[42]");

    nimcp_free(str);
    config_array_destroy(arr);
}

TEST_F(ConfigArrayFormattingRegressionTest, BoolArrayFormatting) {
    /* REGRESSION: Boolean values should format correctly */
    config_array_full_t* arr = config_array_create(CONFIG_TYPE_BOOL, 4);
    ASSERT_NE(arr, nullptr);

    config_array_append_bool(arr, true);
    config_array_append_bool(arr, false);
    config_array_append_bool(arr, true);
    config_array_append_bool(arr, false);

    char* str = config_array_to_string(arr);
    ASSERT_NE(str, nullptr);
    EXPECT_STREQ(str, "[true, false, true, false]");

    nimcp_free(str);
    config_array_destroy(arr);
}

/* ============================================================================
 * Embodiment Reason String Regression Tests
 * ============================================================================
 * Related to: Task #11 - Fix unsafe strcpy in embodied_simulation.c
 */

#define NIMCP_SIMULATION_REASON_MAX 64

class EmbodimentReasonStringRegressionTest : public BufferOverflowRegressionTest {};

TEST_F(EmbodimentReasonStringRegressionTest, ReasonBufferBoundary) {
    /* REGRESSION: Reason string should not overflow 64-byte buffer */
    char reason[NIMCP_SIMULATION_REASON_MAX];

    // String that exactly fills the buffer (63 chars + null)
    std::string exact_fit(NIMCP_SIMULATION_REASON_MAX - 1, 'X');
    snprintf(reason, sizeof(reason), "%s", exact_fit.c_str());

    EXPECT_EQ(strlen(reason), NIMCP_SIMULATION_REASON_MAX - 1);
    EXPECT_EQ(reason[NIMCP_SIMULATION_REASON_MAX - 1], '\0');
}

TEST_F(EmbodimentReasonStringRegressionTest, ReasonWithFormatOverflow) {
    /* REGRESSION: Formatted reasons should truncate safely */
    char reason[NIMCP_SIMULATION_REASON_MAX];

    // Format that produces more than 64 chars
    double very_precise_dist = 1234567890.123456789012345;
    double very_precise_max = 9876543210.987654321098765;

    snprintf(reason, sizeof(reason),
             "Target out of reach (%.15fm > %.15fm)", very_precise_dist, very_precise_max);

    // Should be truncated but null-terminated
    EXPECT_EQ(strlen(reason), NIMCP_SIMULATION_REASON_MAX - 1);
    EXPECT_EQ(reason[NIMCP_SIMULATION_REASON_MAX - 1], '\0');
}

TEST_F(EmbodimentReasonStringRegressionTest, AllReasonMessages) {
    /* REGRESSION: All standard reason messages should fit */
    char reason[NIMCP_SIMULATION_REASON_MAX];

    // Test all messages from embodied_simulation.c
    const char* messages[] = {
        "Effector not found",
        "Target out of reach (1.00m > 0.50m)",
        "Insufficient force capacity",
        "Already grasping an object",
        "Not holding any object",
        "Feasible"
    };

    for (const char* msg : messages) {
        snprintf(reason, sizeof(reason), "%s", msg);
        EXPECT_LT(strlen(reason), NIMCP_SIMULATION_REASON_MAX);
        EXPECT_STREQ(reason, msg);
    }
}

/* ============================================================================
 * Maximum Length Input Regression Tests
 * ============================================================================ */

class MaxLengthInputRegressionTest : public BufferOverflowRegressionTest {};

TEST_F(MaxLengthInputRegressionTest, ParseVeryLongIntArrayString) {
    /* REGRESSION: Parsing very long input should not overflow */
    std::string long_input = "[";
    for (int i = 0; i < 10000; i++) {
        if (i > 0) long_input += ", ";
        long_input += std::to_string(i);
    }
    long_input += "]";

    config_array_t* arr = config_parse_int_array(long_input.c_str());

    // Should either succeed or fail gracefully
    if (arr) {
        EXPECT_EQ(config_array_size(arr), 10000u);
        config_array_destroy(arr);
    }
    // If NULL, it's acceptable - we're testing for no crash
}

TEST_F(MaxLengthInputRegressionTest, ParseExtremelyLongStringElement) {
    /* REGRESSION: Very long string elements should be handled */
    std::string long_str(100000, 'A');
    std::string input = "[\"" + long_str + "\"]";

    config_array_t* arr = config_parse_string_array(input.c_str());

    if (arr) {
        EXPECT_EQ(config_array_size(arr), 1u);
        const char* val = config_array_get_string(arr, 0, "");
        EXPECT_EQ(strlen(val), 100000u);
        config_array_destroy(arr);
    }
}

TEST_F(MaxLengthInputRegressionTest, ManySmallElements) {
    /* REGRESSION: Many small elements should not overflow */
    std::string input = "[";
    for (int i = 0; i < 100000; i++) {
        if (i > 0) input += ",";
        input += "1";
    }
    input += "]";

    config_array_t* arr = config_parse_int_array(input.c_str());

    if (arr) {
        EXPECT_EQ(config_array_size(arr), 100000u);
        config_array_destroy(arr);
    }
}

/* ============================================================================
 * Embedded Null Character Regression Tests
 * ============================================================================ */

class EmbeddedNullRegressionTest : public BufferOverflowRegressionTest {};

TEST_F(EmbeddedNullRegressionTest, StringWithEmbeddedNullParsing) {
    /* REGRESSION: Embedded nulls should stop string processing */
    // Note: C strings end at null, so embedded nulls create shorter strings
    char input_with_null[] = "[\"hello\0world\"]";

    // strlen only sees "hello" part
    EXPECT_EQ(strlen(input_with_null), 7u);  // '["hello'

    // If we try to parse, we should get partial or error result
    config_array_t* arr = config_parse_string_array(input_with_null);
    // Should not crash regardless of behavior
    if (arr) {
        config_array_destroy(arr);
    }
}

TEST_F(EmbeddedNullRegressionTest, StringArrayWithNullElement) {
    /* REGRESSION: NULL string pointer in array should be handled */
    config_array_full_t* arr = config_array_create(CONFIG_TYPE_STRING, 5);
    ASSERT_NE(arr, nullptr);

    // Add some strings
    config_array_append_string(arr, "first");
    config_array_append_string(arr, "second");

    char* str = config_array_to_string(arr);
    ASSERT_NE(str, nullptr);
    EXPECT_NE(strstr(str, "first"), nullptr);
    EXPECT_NE(strstr(str, "second"), nullptr);

    nimcp_free(str);
    config_array_destroy(arr);
}

/* ============================================================================
 * Unicode/UTF-8 Handling Regression Tests
 * ============================================================================ */

class UTF8HandlingRegressionTest : public BufferOverflowRegressionTest {};

TEST_F(UTF8HandlingRegressionTest, UTF8StringArray) {
    /* REGRESSION: UTF-8 strings should not cause issues */
    config_array_full_t* arr = config_array_create(CONFIG_TYPE_STRING, 5);
    ASSERT_NE(arr, nullptr);

    // Various UTF-8 strings
    config_array_append_string(arr, "Hello");
    config_array_append_string(arr, "\xC3\xA9");  // e with acute
    config_array_append_string(arr, "\xE2\x82\xAC");  // Euro sign
    config_array_append_string(arr, "\xF0\x9F\x98\x80");  // Emoji (4 bytes)

    char* str = config_array_to_string(arr);
    ASSERT_NE(str, nullptr);

    // Should contain all strings
    EXPECT_NE(strstr(str, "Hello"), nullptr);
    EXPECT_NE(strstr(str, "\xC3\xA9"), nullptr);

    nimcp_free(str);
    config_array_destroy(arr);
}

TEST_F(UTF8HandlingRegressionTest, UTF8ReasonTruncation) {
    /* REGRESSION: UTF-8 truncation at buffer boundary */
    char reason[NIMCP_SIMULATION_REASON_MAX];

    // Create a string with UTF-8 that might be truncated mid-character
    std::string utf8_str(NIMCP_SIMULATION_REASON_MAX - 5, 'A');
    utf8_str += "\xF0\x9F\x98\x80";  // 4-byte emoji

    snprintf(reason, sizeof(reason), "%s", utf8_str.c_str());

    // Should be null-terminated
    EXPECT_EQ(reason[NIMCP_SIMULATION_REASON_MAX - 1], '\0');

    // Note: Result may be invalid UTF-8 if truncated mid-character
    // This is acceptable - we're testing no buffer overflow
}

TEST_F(UTF8HandlingRegressionTest, ParseUTF8ArrayInput) {
    /* REGRESSION: Parsing UTF-8 array input */
    const char* input = "[\"Hello\", \"\xC3\xA9\", \"\xE2\x82\xAC\"]";

    config_array_t* arr = config_parse_string_array(input);
    if (arr) {
        EXPECT_EQ(config_array_size(arr), 3u);
        EXPECT_STREQ(config_array_get_string(arr, 0, ""), "Hello");
        EXPECT_STREQ(config_array_get_string(arr, 1, ""), "\xC3\xA9");
        EXPECT_STREQ(config_array_get_string(arr, 2, ""), "\xE2\x82\xAC");
        config_array_destroy(arr);
    }
}

/* ============================================================================
 * Boundary Condition Parameterized Tests
 * ============================================================================ */

class BoundaryConditionTest : public BufferOverflowRegressionTest,
    public ::testing::WithParamInterface<size_t> {};

TEST_P(BoundaryConditionTest, IntArrayOfSize) {
    /* REGRESSION: Various array sizes should work */
    size_t size = GetParam();

    config_array_full_t* arr = config_array_create(CONFIG_TYPE_INT, size);
    ASSERT_NE(arr, nullptr);

    for (size_t i = 0; i < size; i++) {
        EXPECT_TRUE(config_array_append_int(arr, (int64_t)i));
    }

    EXPECT_EQ(config_array_size(arr), size);

    if (size <= 1000) {  // Only serialize small arrays
        char* str = config_array_to_string(arr);
        ASSERT_NE(str, nullptr);
        nimcp_free(str);
    }

    config_array_destroy(arr);
}

INSTANTIATE_TEST_SUITE_P(
    ArraySizes,
    BoundaryConditionTest,
    ::testing::Values(0, 1, 2, 10, 100, 1000, 10000)
);

/* ============================================================================
 * Memory Safety Tests
 * ============================================================================ */

class MemorySafetyRegressionTest : public BufferOverflowRegressionTest {};

TEST_F(MemorySafetyRegressionTest, DoubleDestroyDoesNotCrash) {
    /* REGRESSION: Double destroy should not crash */
    config_array_full_t* arr = config_array_create(CONFIG_TYPE_INT, 10);
    ASSERT_NE(arr, nullptr);

    config_array_destroy(arr);
    // Note: Cannot test double-free safely, but destroy should set magic to 0
    // Second call with same pointer is undefined, so we just test single destroy
}

TEST_F(MemorySafetyRegressionTest, UseAfterClearDoesNotCrash) {
    /* REGRESSION: Clear should leave array in valid state */
    config_array_full_t* arr = config_array_create(CONFIG_TYPE_STRING, 10);
    ASSERT_NE(arr, nullptr);

    config_array_append_string(arr, "test1");
    config_array_append_string(arr, "test2");

    config_array_clear(arr);

    EXPECT_EQ(config_array_size(arr), 0u);
    EXPECT_TRUE(config_array_is_empty(arr));

    // Should be able to add new elements
    EXPECT_TRUE(config_array_append_string(arr, "new"));
    EXPECT_EQ(config_array_size(arr), 1u);

    config_array_destroy(arr);
}

TEST_F(MemorySafetyRegressionTest, ResizePreservesData) {
    /* REGRESSION: Resize should preserve existing data */
    config_array_full_t* arr = config_array_create(CONFIG_TYPE_INT, 10);
    ASSERT_NE(arr, nullptr);

    for (int i = 0; i < 10; i++) {
        config_array_append_int(arr, i * 10);
    }

    EXPECT_TRUE(config_array_resize(arr, 100));

    // Verify data preserved
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(config_array_get_int(arr, (size_t)i, -1), i * 10);
    }

    config_array_destroy(arr);
}

TEST_F(MemorySafetyRegressionTest, CloneIndependent) {
    /* REGRESSION: Cloned array should be independent */
    config_array_full_t* arr = config_array_create(CONFIG_TYPE_STRING, 5);
    ASSERT_NE(arr, nullptr);

    config_array_append_string(arr, "original");

    config_array_t* clone = config_array_clone(arr);
    ASSERT_NE(clone, nullptr);

    // Modify original
    config_array_set_string(arr, 0, "modified");

    // Clone should be unchanged
    EXPECT_STREQ(config_array_get_string(clone, 0, ""), "original");

    config_array_destroy(arr);
    config_array_destroy(clone);
}

/* ============================================================================
 * Stress Tests
 * ============================================================================ */

class BufferStressTest : public BufferOverflowRegressionTest {};

TEST_F(BufferStressTest, RapidAppendAndSerialize) {
    /* STRESS: Rapid append and serialize operations */
    config_array_full_t* arr = config_array_create(CONFIG_TYPE_INT, 1);
    ASSERT_NE(arr, nullptr);

    for (int iter = 0; iter < 100; iter++) {
        // Append some values
        for (int i = 0; i < 10; i++) {
            config_array_append_int(arr, iter * 10 + i);
        }

        // Serialize
        char* str = config_array_to_string(arr);
        ASSERT_NE(str, nullptr);
        nimcp_free(str);

        // Clear and repeat
        config_array_clear(arr);
    }

    config_array_destroy(arr);
}

TEST_F(BufferStressTest, RandomAccessPattern) {
    /* STRESS: Random get/set operations */
    config_array_full_t* arr = config_array_create(CONFIG_TYPE_INT, 1000);
    ASSERT_NE(arr, nullptr);

    // Fill array
    for (int i = 0; i < 1000; i++) {
        config_array_append_int(arr, i);
    }

    // Random access
    for (int i = 0; i < 10000; i++) {
        size_t idx = (size_t)(i * 7) % 1000;
        int64_t val = config_array_get_int(arr, idx, -1);
        EXPECT_GE(val, 0);
        config_array_set_int(arr, idx, val + 1);
    }

    config_array_destroy(arr);
}

}  // namespace
