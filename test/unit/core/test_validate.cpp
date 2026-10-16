/**
 * @file test_validate.cpp
 * @brief Comprehensive unit tests for nimcp_validate.c
 *
 * WHAT: Tests for field validation utilities
 * WHY: Ensure data integrity and validation functions work correctly
 * HOW: GoogleTest framework with fixture classes and edge case coverage
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <limits>

#include "utils/validation/nimcp_common.h"
#include "utils/validation/nimcp_validate.h"

//=============================================================================
// Integer Field Validation Tests
//=============================================================================

class IntegerFieldTest : public ::testing::Test {
   protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * WHAT: Test NULL pointer handling
 * WHY: Verify proper error handling for NULL input
 */
TEST_F(IntegerFieldTest, NullPointer)
{
    EXPECT_FALSE(nimcp_validate_integer_field(nullptr, sizeof(int32_t)));
}

/**
 * WHAT: Test invalid size handling
 * WHY: Ensure only standard integer sizes are accepted
 */
TEST_F(IntegerFieldTest, InvalidSize)
{
    int32_t value = 42;
    EXPECT_FALSE(nimcp_validate_integer_field(&value, 3));  // Not a standard size
    EXPECT_FALSE(nimcp_validate_integer_field(&value, 5));
    EXPECT_FALSE(nimcp_validate_integer_field(&value, 7));
}

/**
 * WHAT: Test valid 8-bit integer
 * WHY: Verify int8_t/uint8_t validation works
 */
TEST_F(IntegerFieldTest, Valid8Bit)
{
    int8_t value = 42;
    EXPECT_TRUE(nimcp_validate_integer_field(&value, sizeof(int8_t)));

    value = -128;
    EXPECT_TRUE(nimcp_validate_integer_field(&value, sizeof(int8_t)));

    value = 127;
    EXPECT_TRUE(nimcp_validate_integer_field(&value, sizeof(int8_t)));
}

/**
 * WHAT: Test valid 16-bit integer
 * WHY: Verify int16_t/uint16_t validation works
 */
TEST_F(IntegerFieldTest, Valid16Bit)
{
    int16_t value = 1000;
    EXPECT_TRUE(nimcp_validate_integer_field(&value, sizeof(int16_t)));

    value = -32768;
    EXPECT_TRUE(nimcp_validate_integer_field(&value, sizeof(int16_t)));

    value = 32767;
    EXPECT_TRUE(nimcp_validate_integer_field(&value, sizeof(int16_t)));
}

/**
 * WHAT: Test valid 32-bit integer within range
 * WHY: Verify int32_t validation with range checking
 */
TEST_F(IntegerFieldTest, Valid32Bit)
{
    int32_t value = 42;
    EXPECT_TRUE(nimcp_validate_integer_field(&value, sizeof(int32_t)));

    value = NIMCP_INT32_MIN;
    EXPECT_TRUE(nimcp_validate_integer_field(&value, sizeof(int32_t)));

    value = NIMCP_INT32_MAX;
    EXPECT_TRUE(nimcp_validate_integer_field(&value, sizeof(int32_t)));

    value = 0;
    EXPECT_TRUE(nimcp_validate_integer_field(&value, sizeof(int32_t)));
}

/**
 * WHAT: Test valid 64-bit integer within range
 * WHY: Verify int64_t validation with range checking
 */
TEST_F(IntegerFieldTest, Valid64Bit)
{
    int64_t value = 42;
    EXPECT_TRUE(nimcp_validate_integer_field(&value, sizeof(int64_t)));

    value = NIMCP_INT64_MIN;
    EXPECT_TRUE(nimcp_validate_integer_field(&value, sizeof(int64_t)));

    value = NIMCP_INT64_MAX;
    EXPECT_TRUE(nimcp_validate_integer_field(&value, sizeof(int64_t)));

    value = 0;
    EXPECT_TRUE(nimcp_validate_integer_field(&value, sizeof(int64_t)));
}

/**
 * WHAT: Test alignment requirements
 * WHY: Ensure misaligned data is rejected
 */
TEST_F(IntegerFieldTest, Alignment)
{
    // Allocate aligned buffer
    alignas(8) uint8_t buffer[16];

    // Test 2-byte alignment for int16_t
    int16_t* ptr16_aligned = reinterpret_cast<int16_t*>(&buffer[0]);
    *ptr16_aligned = 42;
    EXPECT_TRUE(nimcp_validate_integer_field(ptr16_aligned, sizeof(int16_t)));

    // Misaligned 2-byte access
    int16_t* ptr16_misaligned = reinterpret_cast<int16_t*>(&buffer[1]);
    // Don't write to misaligned pointer - UBSan will abort before we can test validation
    EXPECT_FALSE(nimcp_validate_integer_field(ptr16_misaligned, sizeof(int16_t)));

    // Test 4-byte alignment for int32_t
    int32_t* ptr32_aligned = reinterpret_cast<int32_t*>(&buffer[0]);
    *ptr32_aligned = 42;
    EXPECT_TRUE(nimcp_validate_integer_field(ptr32_aligned, sizeof(int32_t)));

    // Misaligned 4-byte access
    int32_t* ptr32_misaligned = reinterpret_cast<int32_t*>(&buffer[2]);
    // Don't write to misaligned pointer - UBSan will abort before we can test validation
    EXPECT_FALSE(nimcp_validate_integer_field(ptr32_misaligned, sizeof(int32_t)));
}

//=============================================================================
// Float Field Validation Tests
//=============================================================================

class FloatFieldTest : public ::testing::Test {
   protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * WHAT: Test NULL pointer handling
 * WHY: Verify proper error handling for NULL input
 */
TEST_F(FloatFieldTest, NullPointer)
{
    EXPECT_FALSE(nimcp_validate_float_field(nullptr, sizeof(float)));
}

/**
 * WHAT: Test invalid size handling
 * WHY: Ensure only float and double sizes are accepted
 */
TEST_F(FloatFieldTest, InvalidSize)
{
    float value = 3.14f;
    EXPECT_FALSE(nimcp_validate_float_field(&value, 2));  // Not float or double
    EXPECT_FALSE(nimcp_validate_float_field(&value, 3));
    EXPECT_FALSE(nimcp_validate_float_field(&value, 16));
}

/**
 * WHAT: Test valid float values
 * WHY: Verify float validation accepts normal values
 */
TEST_F(FloatFieldTest, ValidFloat)
{
    float value = 3.14f;
    EXPECT_TRUE(nimcp_validate_float_field(&value, sizeof(float)));

    value = -2.718f;
    EXPECT_TRUE(nimcp_validate_float_field(&value, sizeof(float)));

    value = 0.0f;
    EXPECT_TRUE(nimcp_validate_float_field(&value, sizeof(float)));

    value = NIMCP_FLOAT_MAX;
    EXPECT_TRUE(nimcp_validate_float_field(&value, sizeof(float)));

    value = -NIMCP_FLOAT_MAX;
    EXPECT_TRUE(nimcp_validate_float_field(&value, sizeof(float)));
}

/**
 * WHAT: Test valid double values
 * WHY: Verify double validation accepts normal values
 */
TEST_F(FloatFieldTest, ValidDouble)
{
    double value = 3.14159265358979;
    EXPECT_TRUE(nimcp_validate_float_field(&value, sizeof(double)));

    value = -2.718281828459045;
    EXPECT_TRUE(nimcp_validate_float_field(&value, sizeof(double)));

    value = 0.0;
    EXPECT_TRUE(nimcp_validate_float_field(&value, sizeof(double)));

    value = NIMCP_DOUBLE_MAX;
    EXPECT_TRUE(nimcp_validate_float_field(&value, sizeof(double)));

    value = -NIMCP_DOUBLE_MAX;
    EXPECT_TRUE(nimcp_validate_float_field(&value, sizeof(double)));
}

/**
 * WHAT: Test NaN rejection for float
 * WHY: Ensure NaN values are properly rejected
 */
TEST_F(FloatFieldTest, FloatNaN)
{
    float value = std::nanf("");
    EXPECT_FALSE(nimcp_validate_float_field(&value, sizeof(float)));

    value = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(nimcp_validate_float_field(&value, sizeof(float)));
}

/**
 * WHAT: Test NaN rejection for double
 * WHY: Ensure NaN values are properly rejected
 */
TEST_F(FloatFieldTest, DoubleNaN)
{
    double value = std::nan("");
    EXPECT_FALSE(nimcp_validate_float_field(&value, sizeof(double)));

    value = std::numeric_limits<double>::quiet_NaN();
    EXPECT_FALSE(nimcp_validate_float_field(&value, sizeof(double)));
}

/**
 * WHAT: Test infinity rejection for float
 * WHY: Ensure infinity values are properly rejected
 */
TEST_F(FloatFieldTest, FloatInfinity)
{
    float value = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(nimcp_validate_float_field(&value, sizeof(float)));

    value = -std::numeric_limits<float>::infinity();
    EXPECT_FALSE(nimcp_validate_float_field(&value, sizeof(float)));
}

/**
 * WHAT: Test infinity rejection for double
 * WHY: Ensure infinity values are properly rejected
 */
TEST_F(FloatFieldTest, DoubleInfinity)
{
    double value = std::numeric_limits<double>::infinity();
    EXPECT_FALSE(nimcp_validate_float_field(&value, sizeof(double)));

    value = -std::numeric_limits<double>::infinity();
    EXPECT_FALSE(nimcp_validate_float_field(&value, sizeof(double)));
}

/**
 * WHAT: Test out-of-range float values
 * WHY: Ensure values exceeding NIMCP_FLOAT_MAX are rejected
 */
TEST_F(FloatFieldTest, FloatOutOfRange)
{
    float value = NIMCP_FLOAT_MAX * 2.0f;
    EXPECT_FALSE(nimcp_validate_float_field(&value, sizeof(float)));

    value = -NIMCP_FLOAT_MAX * 2.0f;
    EXPECT_FALSE(nimcp_validate_float_field(&value, sizeof(float)));
}

/**
 * WHAT: Test out-of-range double values
 * WHY: Ensure values exceeding NIMCP_DOUBLE_MAX are rejected
 */
TEST_F(FloatFieldTest, DoubleOutOfRange)
{
    double value = NIMCP_DOUBLE_MAX * 2.0;
    EXPECT_FALSE(nimcp_validate_float_field(&value, sizeof(double)));

    value = -NIMCP_DOUBLE_MAX * 2.0;
    EXPECT_FALSE(nimcp_validate_float_field(&value, sizeof(double)));
}

/**
 * WHAT: Test alignment requirements for float
 * WHY: Ensure misaligned data is rejected
 */
TEST_F(FloatFieldTest, FloatAlignment)
{
    alignas(8) uint8_t buffer[16];

    // Aligned float
    float* ptr_aligned = reinterpret_cast<float*>(&buffer[0]);
    *ptr_aligned = 3.14f;
    EXPECT_TRUE(nimcp_validate_float_field(ptr_aligned, sizeof(float)));

    // Misaligned float (at offset 1)
    float* ptr_misaligned = reinterpret_cast<float*>(&buffer[1]);
    // Don't write to misaligned pointer - UBSan will abort before we can test validation
    EXPECT_FALSE(nimcp_validate_float_field(ptr_misaligned, sizeof(float)));
}

/**
 * WHAT: Test alignment requirements for double
 * WHY: Ensure misaligned data is rejected
 */
TEST_F(FloatFieldTest, DoubleAlignment)
{
    alignas(8) uint8_t buffer[16];

    // Aligned double
    double* ptr_aligned = reinterpret_cast<double*>(&buffer[0]);
    *ptr_aligned = 3.14159265;
    EXPECT_TRUE(nimcp_validate_float_field(ptr_aligned, sizeof(double)));

    // Misaligned double (at offset 4)
    double* ptr_misaligned = reinterpret_cast<double*>(&buffer[4]);
    // Don't write to misaligned pointer - UBSan will abort before we can test validation
    EXPECT_FALSE(nimcp_validate_float_field(ptr_misaligned, sizeof(double)));
}

//=============================================================================
// String Field Validation Tests
//=============================================================================

class StringFieldTest : public ::testing::Test {
   protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * WHAT: Test NULL pointer handling
 * WHY: Verify proper error handling for NULL input
 */
TEST_F(StringFieldTest, NullPointer)
{
    EXPECT_FALSE(nimcp_validate_string_field(nullptr, 10));
}

/**
 * WHAT: Test zero size handling
 * WHY: Verify proper error handling for zero size
 */
TEST_F(StringFieldTest, ZeroSize)
{
    char buffer[10] = "test";
    EXPECT_FALSE(nimcp_validate_string_field(buffer, 0));
}

/**
 * WHAT: Test valid ASCII string
 * WHY: Verify basic string validation works
 */
TEST_F(StringFieldTest, ValidAsciiString)
{
    char buffer[32] = "Hello, World!";
    EXPECT_TRUE(nimcp_validate_string_field(buffer, sizeof(buffer)));

    strcpy(buffer, "");  // Empty string
    EXPECT_TRUE(nimcp_validate_string_field(buffer, sizeof(buffer)));

    strcpy(buffer, "A");  // Single character
    EXPECT_TRUE(nimcp_validate_string_field(buffer, sizeof(buffer)));
}

/**
 * WHAT: Test NULL termination requirement
 * WHY: Ensure strings must be NULL-terminated
 */
TEST_F(StringFieldTest, NullTermination)
{
    char buffer[10];
    memset(buffer, 'A', sizeof(buffer));  // No NULL terminator
    EXPECT_FALSE(nimcp_validate_string_field(buffer, sizeof(buffer)));

    // Properly terminated
    buffer[9] = '\0';
    EXPECT_TRUE(nimcp_validate_string_field(buffer, sizeof(buffer)));
}

/**
 * WHAT: Test allowed control characters
 * WHY: Verify newline and tab are accepted
 */
TEST_F(StringFieldTest, AllowedControlCharacters)
{
    char buffer[32] = "Line1\nLine2";
    EXPECT_TRUE(nimcp_validate_string_field(buffer, sizeof(buffer)));

    strcpy(buffer, "Col1\tCol2");
    EXPECT_TRUE(nimcp_validate_string_field(buffer, sizeof(buffer)));

    strcpy(buffer, "Mixed\n\tText");
    EXPECT_TRUE(nimcp_validate_string_field(buffer, sizeof(buffer)));
}

/**
 * WHAT: Test rejected control characters
 * WHY: Ensure dangerous control characters are rejected
 */
TEST_F(StringFieldTest, RejectedControlCharacters)
{
    char buffer[32];

    // Bell character
    strcpy(buffer, "Test");
    buffer[2] = '\x07';
    EXPECT_FALSE(nimcp_validate_string_field(buffer, sizeof(buffer)));

    // Escape character
    strcpy(buffer, "Test");
    buffer[2] = '\x1B';
    EXPECT_FALSE(nimcp_validate_string_field(buffer, sizeof(buffer)));

    // Backspace
    strcpy(buffer, "Test");
    buffer[2] = '\x08';
    EXPECT_FALSE(nimcp_validate_string_field(buffer, sizeof(buffer)));
}

/**
 * WHAT: Test valid UTF-8 sequences
 * WHY: Verify UTF-8 validation accepts valid encodings
 */
TEST_F(StringFieldTest, ValidUtf8)
{
    // 2-byte UTF-8 (é = 0xC3 0xA9)
    char buffer[32] = "Caf\xC3\xA9";
    EXPECT_TRUE(nimcp_validate_string_field(buffer, sizeof(buffer)));

    // 3-byte UTF-8 (€ = 0xE2 0x82 0xAC)
    strcpy(buffer, "Price: 10");
    buffer[7] = '\xE2';
    buffer[8] = '\x82';
    buffer[9] = '\xAC';
    buffer[10] = '\0';
    EXPECT_TRUE(nimcp_validate_string_field(buffer, sizeof(buffer)));
}

/**
 * WHAT: Test invalid UTF-8 sequences
 * WHY: Ensure malformed UTF-8 is rejected
 */
TEST_F(StringFieldTest, InvalidUtf8)
{
    // Invalid continuation byte
    char buffer[32] = "Test";
    buffer[2] = '\xC0';  // Start of 2-byte sequence
    buffer[3] = '\x20';  // Invalid continuation (should be 10xxxxxx)
    buffer[4] = '\0';
    EXPECT_FALSE(nimcp_validate_string_field(buffer, sizeof(buffer)));

    // Truncated sequence
    strcpy(buffer, "Test");
    buffer[2] = '\xC3';  // Start of 2-byte sequence
    buffer[3] = '\0';    // Terminated before continuation byte
    EXPECT_FALSE(nimcp_validate_string_field(buffer, sizeof(buffer)));
}

/**
 * WHAT: Test maximum string length
 * WHY: Ensure strings exceeding max length are rejected
 */
TEST_F(StringFieldTest, MaximumLength)
{
    // Create a string at the limit
    size_t len = NIMCP_STRING_MAX_LENGTH;
    char* buffer = new char[len + 2];  // +1 for null, +1 extra
    memset(buffer, 'A', len);
    buffer[len] = '\0';

    EXPECT_TRUE(nimcp_validate_string_field(buffer, len + 1));

    // Exceed the limit
    buffer[len] = 'A';
    buffer[len + 1] = '\0';
    EXPECT_FALSE(nimcp_validate_string_field(buffer, len + 2));

    delete[] buffer;
}

//=============================================================================
// Array Field Validation Tests
//=============================================================================

class ArrayFieldTest : public ::testing::Test {
   protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * WHAT: Test NULL pointer handling
 * WHY: Verify proper error handling for NULL input
 */
TEST_F(ArrayFieldTest, NullPointer)
{
    EXPECT_FALSE(nimcp_validate_array_field(nullptr, 100));
}

/**
 * WHAT: Test size too small for header
 * WHY: Ensure size must accommodate header
 */
TEST_F(ArrayFieldTest, SizeTooSmall)
{
    uint8_t buffer[4];
    EXPECT_FALSE(nimcp_validate_array_field(buffer, sizeof(buffer)));
}

/**
 * WHAT: Test valid integer array
 * WHY: Verify integer array validation works
 */
TEST_F(ArrayFieldTest, ValidIntegerArray)
{
    alignas(8) uint8_t buffer[128];
    NimcpArrayHeader* header = reinterpret_cast<NimcpArrayHeader*>(buffer);

    header->element_type = NIMCP_ARRAY_INTEGER;
    header->element_size = sizeof(int32_t);
    header->element_count = 5;

    // Fill with valid integers
    int32_t* elements = reinterpret_cast<int32_t*>(buffer + sizeof(NimcpArrayHeader));
    for (int i = 0; i < 5; i++) {
        elements[i] = i * 10;
    }

    size_t total_size = sizeof(NimcpArrayHeader) + (5 * sizeof(int32_t));
    EXPECT_TRUE(nimcp_validate_array_field(buffer, total_size));
}

/**
 * WHAT: Test valid float array
 * WHY: Verify float array validation works
 */
TEST_F(ArrayFieldTest, ValidFloatArray)
{
    alignas(8) uint8_t buffer[128];
    NimcpArrayHeader* header = reinterpret_cast<NimcpArrayHeader*>(buffer);

    header->element_type = NIMCP_ARRAY_FLOAT;
    header->element_size = sizeof(float);
    header->element_count = 4;

    // Fill with valid floats
    float* elements = reinterpret_cast<float*>(buffer + sizeof(NimcpArrayHeader));
    for (int i = 0; i < 4; i++) {
        elements[i] = static_cast<float>(i) * 1.5f;
    }

    size_t total_size = sizeof(NimcpArrayHeader) + (4 * sizeof(float));
    EXPECT_TRUE(nimcp_validate_array_field(buffer, total_size));
}

/**
 * WHAT: Test valid string array
 * WHY: Verify string array validation works
 */
TEST_F(ArrayFieldTest, ValidStringArray)
{
    alignas(8) uint8_t buffer[256];
    NimcpArrayHeader* header = reinterpret_cast<NimcpArrayHeader*>(buffer);

    header->element_type = NIMCP_ARRAY_STRING;
    header->element_size = 32;  // Each string is 32 bytes
    header->element_count = 3;

    // Fill with valid strings
    char* elements = reinterpret_cast<char*>(buffer + sizeof(NimcpArrayHeader));
    strcpy(&elements[0 * 32], "String1");
    strcpy(&elements[1 * 32], "String2");
    strcpy(&elements[2 * 32], "String3");

    size_t total_size = sizeof(NimcpArrayHeader) + (3 * 32);
    EXPECT_TRUE(nimcp_validate_array_field(buffer, total_size));
}

/**
 * WHAT: Test zero element count
 * WHY: Ensure empty arrays are rejected
 */
TEST_F(ArrayFieldTest, ZeroElementCount)
{
    alignas(8) uint8_t buffer[128];
    NimcpArrayHeader* header = reinterpret_cast<NimcpArrayHeader*>(buffer);

    header->element_type = NIMCP_ARRAY_INTEGER;
    header->element_size = sizeof(int32_t);
    header->element_count = 0;  // Invalid

    EXPECT_FALSE(nimcp_validate_array_field(buffer, sizeof(buffer)));
}

/**
 * WHAT: Test zero element size
 * WHY: Ensure zero-size elements are rejected
 */
TEST_F(ArrayFieldTest, ZeroElementSize)
{
    alignas(8) uint8_t buffer[128];
    NimcpArrayHeader* header = reinterpret_cast<NimcpArrayHeader*>(buffer);

    header->element_type = NIMCP_ARRAY_INTEGER;
    header->element_size = 0;  // Invalid
    header->element_count = 5;

    EXPECT_FALSE(nimcp_validate_array_field(buffer, sizeof(buffer)));
}

/**
 * WHAT: Test element count exceeds maximum
 * WHY: Ensure arrays with too many elements are rejected
 */
TEST_F(ArrayFieldTest, TooManyElements)
{
    alignas(8) uint8_t buffer[128];
    NimcpArrayHeader* header = reinterpret_cast<NimcpArrayHeader*>(buffer);

    header->element_type = NIMCP_ARRAY_INTEGER;
    header->element_size = sizeof(int32_t);
    header->element_count = NIMCP_ARRAY_MAX_ELEMENTS + 1;  // Exceeds limit

    EXPECT_FALSE(nimcp_validate_array_field(buffer, sizeof(buffer)));
}

/**
 * WHAT: Test array size exceeds field size
 * WHY: Ensure calculated size fits within provided buffer
 */
TEST_F(ArrayFieldTest, ArraySizeExceedsFieldSize)
{
    alignas(8) uint8_t buffer[128];
    NimcpArrayHeader* header = reinterpret_cast<NimcpArrayHeader*>(buffer);

    header->element_type = NIMCP_ARRAY_INTEGER;
    header->element_size = sizeof(int32_t);
    header->element_count = 100;  // Would require more than buffer size

    EXPECT_FALSE(nimcp_validate_array_field(buffer, sizeof(buffer)));
}

/**
 * WHAT: Test invalid element type
 * WHY: Ensure unknown element types are rejected
 */
TEST_F(ArrayFieldTest, InvalidElementType)
{
    alignas(8) uint8_t buffer[128];
    NimcpArrayHeader* header = reinterpret_cast<NimcpArrayHeader*>(buffer);

    header->element_type = static_cast<NimcpArrayElementType>(999);  // Invalid type
    header->element_size = sizeof(int32_t);
    header->element_count = 5;

    size_t total_size = sizeof(NimcpArrayHeader) + (5 * sizeof(int32_t));
    EXPECT_FALSE(nimcp_validate_array_field(buffer, total_size));
}

/**
 * WHAT: Test array with invalid element (NaN in float array)
 * WHY: Ensure element validation catches invalid values
 */
TEST_F(ArrayFieldTest, InvalidFloatElement)
{
    alignas(8) uint8_t buffer[128];
    NimcpArrayHeader* header = reinterpret_cast<NimcpArrayHeader*>(buffer);

    header->element_type = NIMCP_ARRAY_FLOAT;
    header->element_size = sizeof(float);
    header->element_count = 3;

    float* elements = reinterpret_cast<float*>(buffer + sizeof(NimcpArrayHeader));
    elements[0] = 1.0f;
    elements[1] = std::nanf("");  // Invalid NaN value
    elements[2] = 3.0f;

    size_t total_size = sizeof(NimcpArrayHeader) + (3 * sizeof(float));
    EXPECT_FALSE(nimcp_validate_array_field(buffer, total_size));
}

//=============================================================================
// State Field Validation Tests
//=============================================================================

class StateFieldTest : public ::testing::Test {
   protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * WHAT: Test NULL pointer handling
 * WHY: Verify proper error handling for NULL input
 */
TEST_F(StateFieldTest, NullPointer)
{
    EXPECT_FALSE(nimcp_validate_state_fields(nullptr, 100));
}

/**
 * WHAT: Test size too small for header
 * WHY: Ensure size must accommodate header
 */
TEST_F(StateFieldTest, SizeTooSmall)
{
    uint8_t buffer[4];
    EXPECT_FALSE(nimcp_validate_state_fields(buffer, sizeof(buffer)));
}

/**
 * WHAT: Test invalid magic number
 * WHY: Ensure magic number validation works
 */
TEST_F(StateFieldTest, InvalidMagic)
{
    uint8_t buffer[256];
    NimcpStateHeader* header = reinterpret_cast<NimcpStateHeader*>(buffer);

    header->magic = 0x12345678;  // Wrong magic
    header->field_count = 1;

    EXPECT_FALSE(nimcp_validate_state_fields(buffer, sizeof(buffer)));
}

/**
 * WHAT: Test zero field count
 * WHY: Ensure states must have at least one field
 */
TEST_F(StateFieldTest, ZeroFieldCount)
{
    uint8_t buffer[256];
    NimcpStateHeader* header = reinterpret_cast<NimcpStateHeader*>(buffer);

    header->magic = NIMCP_STATE_MAGIC;
    header->field_count = 0;  // Invalid

    EXPECT_FALSE(nimcp_validate_state_fields(buffer, sizeof(buffer)));
}

/**
 * WHAT: Test field count exceeds maximum
 * WHY: Ensure states with too many fields are rejected
 */
TEST_F(StateFieldTest, TooManyFields)
{
    uint8_t buffer[256];
    NimcpStateHeader* header = reinterpret_cast<NimcpStateHeader*>(buffer);

    header->magic = NIMCP_STATE_MAGIC;
    header->field_count = NIMCP_MAX_FIELDS + 1;  // Exceeds limit

    EXPECT_FALSE(nimcp_validate_state_fields(buffer, sizeof(buffer)));
}

/**
 * WHAT: Test valid single integer field state
 * WHY: Verify basic state validation works
 */
TEST_F(StateFieldTest, ValidSingleIntegerField)
{
    uint8_t buffer[256];
    memset(buffer, 0, sizeof(buffer));

    NimcpStateHeader* header = reinterpret_cast<NimcpStateHeader*>(buffer);
    header->magic = NIMCP_STATE_MAGIC;
    header->field_count = 1;

    NimcpStateField* fields = reinterpret_cast<NimcpStateField*>(buffer + sizeof(NimcpStateHeader));
    size_t data_offset = sizeof(NimcpStateHeader) + sizeof(NimcpStateField);

    fields[0].type = NIMCP_FIELD_INTEGER;
    fields[0].offset = data_offset;
    fields[0].size = sizeof(int32_t);

    // Add actual data
    int32_t* data = reinterpret_cast<int32_t*>(buffer + data_offset);
    *data = 42;

    size_t total_size = data_offset + sizeof(int32_t);
    EXPECT_TRUE(nimcp_validate_state_fields(buffer, total_size));
}

/**
 * WHAT: Test valid multiple fields state
 * WHY: Verify state with multiple fields validates correctly
 */
TEST_F(StateFieldTest, ValidMultipleFields)
{
    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));

    NimcpStateHeader* header = reinterpret_cast<NimcpStateHeader*>(buffer);
    header->magic = NIMCP_STATE_MAGIC;
    header->field_count = 3;

    NimcpStateField* fields = reinterpret_cast<NimcpStateField*>(buffer + sizeof(NimcpStateHeader));
    size_t base_offset = sizeof(NimcpStateHeader) + (3 * sizeof(NimcpStateField));

    // Field 0: int32_t
    fields[0].type = NIMCP_FIELD_INTEGER;
    fields[0].offset = base_offset;
    fields[0].size = sizeof(int32_t);
    *reinterpret_cast<int32_t*>(buffer + fields[0].offset) = 100;

    // Field 1: float
    fields[1].type = NIMCP_FIELD_FLOAT;
    fields[1].offset = base_offset + sizeof(int32_t);
    fields[1].size = sizeof(float);
    *reinterpret_cast<float*>(buffer + fields[1].offset) = 3.14f;

    // Field 2: string
    fields[2].type = NIMCP_FIELD_STRING;
    fields[2].offset = base_offset + sizeof(int32_t) + sizeof(float);
    fields[2].size = 32;
    strcpy(reinterpret_cast<char*>(buffer + fields[2].offset), "Test");

    size_t total_size = fields[2].offset + fields[2].size;
    EXPECT_TRUE(nimcp_validate_state_fields(buffer, total_size));
}

/**
 * WHAT: Test field exceeds state boundaries
 * WHY: Ensure out-of-bounds fields are rejected
 */
TEST_F(StateFieldTest, FieldExceedsBoundaries)
{
    uint8_t buffer[256];
    memset(buffer, 0, sizeof(buffer));

    NimcpStateHeader* header = reinterpret_cast<NimcpStateHeader*>(buffer);
    header->magic = NIMCP_STATE_MAGIC;
    header->field_count = 1;

    NimcpStateField* fields = reinterpret_cast<NimcpStateField*>(buffer + sizeof(NimcpStateHeader));

    fields[0].type = NIMCP_FIELD_INTEGER;
    fields[0].offset = 200;
    fields[0].size = sizeof(int32_t);

    // Field extends beyond buffer
    EXPECT_FALSE(nimcp_validate_state_fields(buffer, 100));
}

/**
 * WHAT: Test overlapping fields
 * WHY: Ensure overlapping fields are detected and rejected
 */
TEST_F(StateFieldTest, OverlappingFields)
{
    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));

    NimcpStateHeader* header = reinterpret_cast<NimcpStateHeader*>(buffer);
    header->magic = NIMCP_STATE_MAGIC;
    header->field_count = 2;

    NimcpStateField* fields = reinterpret_cast<NimcpStateField*>(buffer + sizeof(NimcpStateHeader));
    size_t base_offset = sizeof(NimcpStateHeader) + (2 * sizeof(NimcpStateField));

    // Field 0: offset 100, size 20
    fields[0].type = NIMCP_FIELD_INTEGER;
    fields[0].offset = base_offset;
    fields[0].size = 20;

    // Field 1: overlaps with field 0 (starts before field 0 ends)
    fields[1].type = NIMCP_FIELD_INTEGER;
    fields[1].offset = base_offset + 10;  // Overlaps!
    fields[1].size = 20;

    EXPECT_FALSE(nimcp_validate_state_fields(buffer, sizeof(buffer)));
}

/**
 * WHAT: Test invalid field type
 * WHY: Ensure unknown field types are rejected
 */
TEST_F(StateFieldTest, InvalidFieldType)
{
    uint8_t buffer[256];
    memset(buffer, 0, sizeof(buffer));

    NimcpStateHeader* header = reinterpret_cast<NimcpStateHeader*>(buffer);
    header->magic = NIMCP_STATE_MAGIC;
    header->field_count = 1;

    NimcpStateField* fields = reinterpret_cast<NimcpStateField*>(buffer + sizeof(NimcpStateHeader));
    size_t data_offset = sizeof(NimcpStateHeader) + sizeof(NimcpStateField);

    fields[0].type = static_cast<NimcpFieldType>(999);  // Invalid type
    fields[0].offset = data_offset;
    fields[0].size = sizeof(int32_t);

    EXPECT_FALSE(nimcp_validate_state_fields(buffer, sizeof(buffer)));
}
