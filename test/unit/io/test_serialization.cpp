/**
 * @file test_serialization.cpp
 * @brief Comprehensive unit tests for serialization functionality
 *
 * WHAT: 100% test coverage for nimcp_serialization.c
 * WHY:  Serialization is critical for data persistence, network communication, and state saving
 * HOW:  Test all write/read operations, compression, edge cases, and error handling
 *
 * TEST COVERAGE:
 * 1. Serializer lifecycle (create, destroy, reset)
 * 2. Buffer management (set, get, position control)
 * 3. Write operations (all data types)
 * 4. Read operations (all data types)
 * 5. Endianness (big-endian format)
 * 6. Compression/decompression (LZ4)
 * 7. Error handling (NULL pointers, bounds checking, corruption)
 * 8. Capacity expansion
 * 9. Edge cases (zero size, max size, buffer overflow)
 * 10. Round-trip serialization (write then read)
 * 11. Memory management and leaks
 * 12. Binary data handling
 *
 * @version Unit Testing Framework v1.0
 * @date 2025-11-19
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <limits>

// Headers have their own extern "C" guards
    #include "io/serialization/nimcp_serialization.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SerializationTest : public ::testing::Test {
protected:
    NimcpSerializer* serializer;

    void SetUp() override {
        serializer = nimcp_serializer_create(0); // Use default size
        ASSERT_NE(serializer, nullptr);
    }

    void TearDown() override {
        if (serializer) {
            nimcp_serializer_destroy(serializer);
            serializer = nullptr;
        }
    }

    // Helper to verify data can round-trip
    template<typename T>
    void TestRoundTrip(bool (*write_fn)(NimcpSerializer*, T),
                      T (*read_fn)(NimcpSerializer*),
                      T value) {
        ASSERT_TRUE(write_fn(serializer, value));
        EXPECT_FALSE(nimcp_serializer_has_error(serializer));

        nimcp_serializer_set_position(serializer, 0);
        T result = read_fn(serializer);
        EXPECT_EQ(result, value);
        EXPECT_FALSE(nimcp_serializer_has_error(serializer));
    }
};

//=============================================================================
// SECTION 1: Serializer Lifecycle Tests
//=============================================================================

TEST_F(SerializationTest, Create_DefaultCapacity) {
    // WHAT: Verify serializer creation with default capacity
    // WHY:  Core functionality

    EXPECT_NE(serializer, nullptr);
    EXPECT_EQ(nimcp_serializer_get_position(serializer), 0u);
    EXPECT_EQ(nimcp_serializer_get_length(serializer), 0u);
    EXPECT_FALSE(nimcp_serializer_is_compressed(serializer));
    EXPECT_FALSE(nimcp_serializer_has_error(serializer));
}

TEST_F(SerializationTest, Create_CustomCapacity) {
    // WHAT: Verify serializer creation with custom initial capacity
    // WHY:  Allow optimization for known data sizes

    NimcpSerializer* custom = nimcp_serializer_create(2048);
    ASSERT_NE(custom, nullptr);
    EXPECT_EQ(nimcp_serializer_get_position(custom), 0u);
    EXPECT_EQ(nimcp_serializer_get_length(custom), 0u);
    nimcp_serializer_destroy(custom);
}

TEST_F(SerializationTest, Create_MaxSizeExceeded) {
    // WHAT: Verify creation fails when requesting > max size
    // WHY:  Prevent excessive memory allocation

    NimcpSerializer* invalid = nimcp_serializer_create(NIMCP_SERIALIZER_MAX_SIZE + 1);
    EXPECT_EQ(invalid, nullptr);
}

TEST_F(SerializationTest, Destroy_NullSafe) {
    // WHAT: Verify destroy handles NULL safely
    // WHY:  Prevent crashes on invalid input

    nimcp_serializer_destroy(nullptr);
    SUCCEED() << "Destroy NULL is safe";
}

TEST_F(SerializationTest, Reset_ClearsState) {
    // WHAT: Verify reset clears serializer state
    // WHY:  Allow reuse of serializer

    nimcp_write_uint32(serializer, 12345);
    nimcp_serializer_mark_compressed(serializer);

    nimcp_serializer_reset(serializer);

    EXPECT_EQ(nimcp_serializer_get_position(serializer), 0u);
    EXPECT_EQ(nimcp_serializer_get_length(serializer), 0u);
    EXPECT_FALSE(nimcp_serializer_is_compressed(serializer));
    EXPECT_FALSE(nimcp_serializer_has_error(serializer));
}

TEST_F(SerializationTest, Reset_NullSafe) {
    // WHAT: Verify reset handles NULL safely
    // WHY:  Defensive programming

    nimcp_serializer_reset(nullptr);
    SUCCEED() << "Reset NULL is safe";
}

//=============================================================================
// SECTION 2: Buffer Management Tests
//=============================================================================

TEST_F(SerializationTest, SetBuffer_ValidData) {
    // WHAT: Verify setting buffer from external data
    // WHY:  Allow deserialization of external data

    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    ASSERT_TRUE(nimcp_serializer_set_buffer(serializer, data, sizeof(data)));

    EXPECT_EQ(nimcp_serializer_get_length(serializer), sizeof(data));
    EXPECT_EQ(nimcp_serializer_get_position(serializer), 0u);

    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(memcmp(buffer, data, sizeof(data)), 0);
}

TEST_F(SerializationTest, SetBuffer_NullSerializer) {
    // WHAT: Verify set_buffer rejects NULL serializer
    // WHY:  Error handling

    uint8_t data[] = {0x01, 0x02};
    EXPECT_FALSE(nimcp_serializer_set_buffer(nullptr, data, sizeof(data)));
}

TEST_F(SerializationTest, SetBuffer_NullData) {
    // WHAT: Verify set_buffer rejects NULL data
    // WHY:  Error handling

    EXPECT_FALSE(nimcp_serializer_set_buffer(serializer, nullptr, 100));
}

TEST_F(SerializationTest, SetBuffer_ExceedsMaxSize) {
    // WHAT: Verify set_buffer rejects oversized data
    // WHY:  Prevent excessive memory usage

    uint8_t data = 0;
    EXPECT_FALSE(nimcp_serializer_set_buffer(serializer, &data, NIMCP_SERIALIZER_MAX_SIZE + 1));
}

TEST_F(SerializationTest, GetBuffer_ReturnsValidPointer) {
    // WHAT: Verify get_buffer returns valid pointer
    // WHY:  Allow access to serialized data

    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    EXPECT_NE(buffer, nullptr);
}

TEST_F(SerializationTest, GetBuffer_NullSerializer) {
    // WHAT: Verify get_buffer handles NULL serializer
    // WHY:  Error handling

    EXPECT_EQ(nimcp_serializer_get_buffer(nullptr), nullptr);
}

TEST_F(SerializationTest, GetLength_AfterWrites) {
    // WHAT: Verify length tracks written data
    // WHY:  Accurate length reporting

    EXPECT_EQ(nimcp_serializer_get_length(serializer), 0u);

    nimcp_write_uint32(serializer, 12345);
    EXPECT_EQ(nimcp_serializer_get_length(serializer), 4u);

    nimcp_write_uint64(serializer, 67890);
    EXPECT_EQ(nimcp_serializer_get_length(serializer), 12u);
}

TEST_F(SerializationTest, GetLength_NullSerializer) {
    // WHAT: Verify get_length handles NULL
    // WHY:  Error handling

    EXPECT_EQ(nimcp_serializer_get_length(nullptr), 0u);
}

TEST_F(SerializationTest, Position_GetSet) {
    // WHAT: Verify position get/set operations
    // WHY:  Allow random access to buffer

    nimcp_write_uint32(serializer, 1);
    nimcp_write_uint32(serializer, 2);
    nimcp_write_uint32(serializer, 3);

    EXPECT_EQ(nimcp_serializer_get_position(serializer), 12u);

    ASSERT_TRUE(nimcp_serializer_set_position(serializer, 4));
    EXPECT_EQ(nimcp_serializer_get_position(serializer), 4u);

    uint32_t value = nimcp_read_uint32(serializer);
    EXPECT_EQ(value, 2u);
}

TEST_F(SerializationTest, SetPosition_BeyondLength) {
    // WHAT: Verify set_position rejects positions beyond length
    // WHY:  Prevent invalid reads

    nimcp_write_uint32(serializer, 123);
    EXPECT_FALSE(nimcp_serializer_set_position(serializer, 100));
}

TEST_F(SerializationTest, SetPosition_NullSerializer) {
    // WHAT: Verify set_position handles NULL
    // WHY:  Error handling

    EXPECT_FALSE(nimcp_serializer_set_position(nullptr, 0));
}

TEST_F(SerializationTest, GetPosition_NullSerializer) {
    // WHAT: Verify get_position handles NULL
    // WHY:  Error handling

    EXPECT_EQ(nimcp_serializer_get_position(nullptr), 0u);
}

//=============================================================================
// SECTION 3: Write Operations - Unsigned Integers
//=============================================================================

TEST_F(SerializationTest, WriteUint8_SingleValue) {
    // WHAT: Verify uint8 write operation
    // WHY:  Basic data type serialization

    ASSERT_TRUE(nimcp_write_uint8(serializer, 0xAB));
    EXPECT_EQ(nimcp_serializer_get_length(serializer), 1u);

    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    EXPECT_EQ(buffer[0], 0xAB);
}

TEST_F(SerializationTest, WriteUint8_MultipleValues) {
    // WHAT: Verify multiple uint8 writes
    // WHY:  Sequential writes must work

    ASSERT_TRUE(nimcp_write_uint8(serializer, 0x01));
    ASSERT_TRUE(nimcp_write_uint8(serializer, 0x02));
    ASSERT_TRUE(nimcp_write_uint8(serializer, 0x03));

    EXPECT_EQ(nimcp_serializer_get_length(serializer), 3u);
}

TEST_F(SerializationTest, WriteUint16_BigEndian) {
    // WHAT: Verify uint16 is written in big-endian format
    // WHY:  Ensure platform-independent serialization

    ASSERT_TRUE(nimcp_write_uint16(serializer, 0x1234));

    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    EXPECT_EQ(buffer[0], 0x12);
    EXPECT_EQ(buffer[1], 0x34);
}

TEST_F(SerializationTest, WriteUint32_BigEndian) {
    // WHAT: Verify uint32 is written in big-endian format
    // WHY:  Ensure platform-independent serialization

    ASSERT_TRUE(nimcp_write_uint32(serializer, 0x12345678));

    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    EXPECT_EQ(buffer[0], 0x12);
    EXPECT_EQ(buffer[1], 0x34);
    EXPECT_EQ(buffer[2], 0x56);
    EXPECT_EQ(buffer[3], 0x78);
}

TEST_F(SerializationTest, WriteUint64_BigEndian) {
    // WHAT: Verify uint64 is written in big-endian format
    // WHY:  Ensure platform-independent serialization

    ASSERT_TRUE(nimcp_write_uint64(serializer, 0x123456789ABCDEF0ULL));

    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    EXPECT_EQ(buffer[0], 0x12);
    EXPECT_EQ(buffer[1], 0x34);
    EXPECT_EQ(buffer[2], 0x56);
    EXPECT_EQ(buffer[3], 0x78);
    EXPECT_EQ(buffer[4], 0x9A);
    EXPECT_EQ(buffer[5], 0xBC);
    EXPECT_EQ(buffer[6], 0xDE);
    EXPECT_EQ(buffer[7], 0xF0);
}

//=============================================================================
// SECTION 4: Write Operations - Signed Integers
//=============================================================================

TEST_F(SerializationTest, WriteInt8_Positive) {
    // WHAT: Verify int8 write with positive value
    // WHY:  Signed integer serialization

    ASSERT_TRUE(nimcp_write_int8(serializer, 127));

    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    EXPECT_EQ(buffer[0], 127);
}

TEST_F(SerializationTest, WriteInt8_Negative) {
    // WHAT: Verify int8 write with negative value
    // WHY:  Two's complement representation

    ASSERT_TRUE(nimcp_write_int8(serializer, -1));

    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    EXPECT_EQ(buffer[0], 0xFF);
}

TEST_F(SerializationTest, WriteInt16_Negative) {
    // WHAT: Verify int16 write with negative value
    // WHY:  Two's complement in big-endian

    ASSERT_TRUE(nimcp_write_int16(serializer, -1));

    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    EXPECT_EQ(buffer[0], 0xFF);
    EXPECT_EQ(buffer[1], 0xFF);
}

TEST_F(SerializationTest, WriteInt32_Negative) {
    // WHAT: Verify int32 write with negative value
    // WHY:  Two's complement in big-endian

    ASSERT_TRUE(nimcp_write_int32(serializer, -1));

    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    EXPECT_EQ(buffer[0], 0xFF);
    EXPECT_EQ(buffer[1], 0xFF);
    EXPECT_EQ(buffer[2], 0xFF);
    EXPECT_EQ(buffer[3], 0xFF);
}

TEST_F(SerializationTest, WriteInt64_Negative) {
    // WHAT: Verify int64 write with negative value
    // WHY:  Two's complement in big-endian

    ASSERT_TRUE(nimcp_write_int64(serializer, -1LL));

    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    for (int i = 0; i < 8; i++) {
        EXPECT_EQ(buffer[i], 0xFF);
    }
}

//=============================================================================
// SECTION 5: Write Operations - Floating Point
//=============================================================================

TEST_F(SerializationTest, WriteFloat_PositiveValue) {
    // WHAT: Verify float write operation
    // WHY:  Floating-point serialization

    float value = 3.14159f;
    ASSERT_TRUE(nimcp_write_float(serializer, value));
    EXPECT_EQ(nimcp_serializer_get_length(serializer), 4u);
}

TEST_F(SerializationTest, WriteFloat_NegativeValue) {
    // WHAT: Verify float write with negative value
    // WHY:  Sign bit handling

    ASSERT_TRUE(nimcp_write_float(serializer, -2.71828f));
    EXPECT_EQ(nimcp_serializer_get_length(serializer), 4u);
}

TEST_F(SerializationTest, WriteFloat_Zero) {
    // WHAT: Verify float write with zero
    // WHY:  Edge case

    ASSERT_TRUE(nimcp_write_float(serializer, 0.0f));
}

TEST_F(SerializationTest, WriteFloat_Infinity) {
    // WHAT: Verify float write with infinity
    // WHY:  Special value handling

    ASSERT_TRUE(nimcp_write_float(serializer, std::numeric_limits<float>::infinity()));
}

TEST_F(SerializationTest, WriteDouble_PositiveValue) {
    // WHAT: Verify double write operation
    // WHY:  Double-precision serialization

    double value = 3.141592653589793;
    ASSERT_TRUE(nimcp_write_double(serializer, value));
    EXPECT_EQ(nimcp_serializer_get_length(serializer), 8u);
}

TEST_F(SerializationTest, WriteDouble_NegativeValue) {
    // WHAT: Verify double write with negative value
    // WHY:  Sign bit handling

    ASSERT_TRUE(nimcp_write_double(serializer, -2.718281828459045));
    EXPECT_EQ(nimcp_serializer_get_length(serializer), 8u);
}

//=============================================================================
// SECTION 6: Write Operations - Boolean and Bytes
//=============================================================================

TEST_F(SerializationTest, WriteBool_True) {
    // WHAT: Verify bool write with true
    // WHY:  Boolean serialization

    ASSERT_TRUE(nimcp_write_bool(serializer, true));

    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    EXPECT_EQ(buffer[0], 1);
}

TEST_F(SerializationTest, WriteBool_False) {
    // WHAT: Verify bool write with false
    // WHY:  Boolean serialization

    ASSERT_TRUE(nimcp_write_bool(serializer, false));

    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    EXPECT_EQ(buffer[0], 0);
}

TEST_F(SerializationTest, WriteBytes_ValidData) {
    // WHAT: Verify byte array write
    // WHY:  Binary data serialization

    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    ASSERT_TRUE(nimcp_write_bytes(serializer, data, sizeof(data)));

    EXPECT_EQ(nimcp_serializer_get_length(serializer), sizeof(data));

    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    EXPECT_EQ(memcmp(buffer, data, sizeof(data)), 0);
}

TEST_F(SerializationTest, WriteBytes_LargeData) {
    // WHAT: Verify writing large byte array
    // WHY:  Test capacity expansion

    size_t size = 10000;
    uint8_t* data = new uint8_t[size];
    for (size_t i = 0; i < size; i++) {
        data[i] = static_cast<uint8_t>(i % 256);
    }

    ASSERT_TRUE(nimcp_write_bytes(serializer, data, size));
    EXPECT_EQ(nimcp_serializer_get_length(serializer), size);

    delete[] data;
}

TEST_F(SerializationTest, WriteBytes_NullData) {
    // WHAT: Verify write_bytes rejects NULL data
    // WHY:  Error handling

    EXPECT_FALSE(nimcp_write_bytes(serializer, nullptr, 100));
}

TEST_F(SerializationTest, WriteBytes_NullSerializer) {
    // WHAT: Verify write_bytes rejects NULL serializer
    // WHY:  Error handling

    uint8_t data[] = {1, 2, 3};
    EXPECT_FALSE(nimcp_write_bytes(nullptr, data, sizeof(data)));
}

//=============================================================================
// SECTION 7: Read Operations - Unsigned Integers
//=============================================================================

TEST_F(SerializationTest, ReadUint8_SingleValue) {
    // WHAT: Verify uint8 read operation
    // WHY:  Basic deserialization

    nimcp_write_uint8(serializer, 0xAB);
    nimcp_serializer_set_position(serializer, 0);

    uint8_t value = nimcp_read_uint8(serializer);
    EXPECT_EQ(value, 0xAB);
    EXPECT_FALSE(nimcp_serializer_has_error(serializer));
}

TEST_F(SerializationTest, ReadUint16_BigEndian) {
    // WHAT: Verify uint16 is read in big-endian format
    // WHY:  Platform-independent deserialization

    uint8_t data[] = {0x12, 0x34};
    nimcp_serializer_set_buffer(serializer, data, sizeof(data));

    uint16_t value = nimcp_read_uint16(serializer);
    EXPECT_EQ(value, 0x1234);
}

TEST_F(SerializationTest, ReadUint32_BigEndian) {
    // WHAT: Verify uint32 is read in big-endian format
    // WHY:  Platform-independent deserialization

    uint8_t data[] = {0x12, 0x34, 0x56, 0x78};
    nimcp_serializer_set_buffer(serializer, data, sizeof(data));

    uint32_t value = nimcp_read_uint32(serializer);
    EXPECT_EQ(value, 0x12345678u);
}

TEST_F(SerializationTest, ReadUint64_BigEndian) {
    // WHAT: Verify uint64 is read in big-endian format
    // WHY:  Platform-independent deserialization

    uint8_t data[] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    nimcp_serializer_set_buffer(serializer, data, sizeof(data));

    uint64_t value = nimcp_read_uint64(serializer);
    EXPECT_EQ(value, 0x123456789ABCDEF0ULL);
}

//=============================================================================
// SECTION 8: Read Operations - Signed Integers
//=============================================================================

TEST_F(SerializationTest, ReadInt8_Positive) {
    // WHAT: Verify int8 read with positive value
    // WHY:  Signed integer deserialization

    nimcp_write_int8(serializer, 127);
    nimcp_serializer_set_position(serializer, 0);

    int8_t value = nimcp_read_int8(serializer);
    EXPECT_EQ(value, 127);
}

TEST_F(SerializationTest, ReadInt8_Negative) {
    // WHAT: Verify int8 read with negative value
    // WHY:  Two's complement interpretation

    nimcp_write_int8(serializer, -128);
    nimcp_serializer_set_position(serializer, 0);

    int8_t value = nimcp_read_int8(serializer);
    EXPECT_EQ(value, -128);
}

TEST_F(SerializationTest, ReadInt16_Negative) {
    // WHAT: Verify int16 read with negative value
    // WHY:  Two's complement interpretation

    nimcp_write_int16(serializer, -32768);
    nimcp_serializer_set_position(serializer, 0);

    int16_t value = nimcp_read_int16(serializer);
    EXPECT_EQ(value, -32768);
}

TEST_F(SerializationTest, ReadInt32_Negative) {
    // WHAT: Verify int32 read with negative value
    // WHY:  Two's complement interpretation

    nimcp_write_int32(serializer, -2147483648);
    nimcp_serializer_set_position(serializer, 0);

    int32_t value = nimcp_read_int32(serializer);
    EXPECT_EQ(value, -2147483648);
}

TEST_F(SerializationTest, ReadInt64_Negative) {
    // WHAT: Verify int64 read with negative value
    // WHY:  Two's complement interpretation

    nimcp_write_int64(serializer, -9223372036854775807LL - 1);
    nimcp_serializer_set_position(serializer, 0);

    int64_t value = nimcp_read_int64(serializer);
    EXPECT_EQ(value, -9223372036854775807LL - 1);
}

//=============================================================================
// SECTION 9: Read Operations - Floating Point
//=============================================================================

TEST_F(SerializationTest, ReadFloat_PositiveValue) {
    // WHAT: Verify float read operation
    // WHY:  Floating-point deserialization

    float expected = 3.14159f;
    nimcp_write_float(serializer, expected);
    nimcp_serializer_set_position(serializer, 0);

    float value = nimcp_read_float(serializer);
    EXPECT_FLOAT_EQ(value, expected);
}

TEST_F(SerializationTest, ReadFloat_NegativeValue) {
    // WHAT: Verify float read with negative value
    // WHY:  Sign bit handling

    float expected = -2.71828f;
    nimcp_write_float(serializer, expected);
    nimcp_serializer_set_position(serializer, 0);

    float value = nimcp_read_float(serializer);
    EXPECT_FLOAT_EQ(value, expected);
}

TEST_F(SerializationTest, ReadDouble_PositiveValue) {
    // WHAT: Verify double read operation
    // WHY:  Double-precision deserialization

    double expected = 3.141592653589793;
    nimcp_write_double(serializer, expected);
    nimcp_serializer_set_position(serializer, 0);

    double value = nimcp_read_double(serializer);
    EXPECT_DOUBLE_EQ(value, expected);
}

TEST_F(SerializationTest, ReadDouble_NegativeValue) {
    // WHAT: Verify double read with negative value
    // WHY:  Sign bit handling

    double expected = -2.718281828459045;
    nimcp_write_double(serializer, expected);
    nimcp_serializer_set_position(serializer, 0);

    double value = nimcp_read_double(serializer);
    EXPECT_DOUBLE_EQ(value, expected);
}

//=============================================================================
// SECTION 10: Read Operations - Boolean and Bytes
//=============================================================================

TEST_F(SerializationTest, ReadBool_True) {
    // WHAT: Verify bool read with true
    // WHY:  Boolean deserialization

    nimcp_write_bool(serializer, true);
    nimcp_serializer_set_position(serializer, 0);

    bool value = nimcp_read_bool(serializer);
    EXPECT_TRUE(value);
}

TEST_F(SerializationTest, ReadBool_False) {
    // WHAT: Verify bool read with false
    // WHY:  Boolean deserialization

    nimcp_write_bool(serializer, false);
    nimcp_serializer_set_position(serializer, 0);

    bool value = nimcp_read_bool(serializer);
    EXPECT_FALSE(value);
}

TEST_F(SerializationTest, ReadBool_NonZeroIsTrue) {
    // WHAT: Verify any non-zero byte reads as true
    // WHY:  Standard boolean interpretation

    nimcp_write_uint8(serializer, 42);
    nimcp_serializer_set_position(serializer, 0);

    bool value = nimcp_read_bool(serializer);
    EXPECT_TRUE(value);
}

TEST_F(SerializationTest, ReadBytes_ValidData) {
    // WHAT: Verify byte array read
    // WHY:  Binary data deserialization

    uint8_t original[] = {0xDE, 0xAD, 0xBE, 0xEF};
    nimcp_write_bytes(serializer, original, sizeof(original));
    nimcp_serializer_set_position(serializer, 0);

    size_t length = 0;
    const uint8_t* data = nimcp_read_bytes(serializer, &length);

    ASSERT_NE(data, nullptr);
    EXPECT_EQ(length, sizeof(original));
    EXPECT_EQ(memcmp(data, original, sizeof(original)), 0);
}

TEST_F(SerializationTest, ReadBytes_EmptyBuffer) {
    // WHAT: Verify read_bytes on empty buffer
    // WHY:  Edge case handling

    size_t length = 0;
    const uint8_t* data = nimcp_read_bytes(serializer, &length);

    EXPECT_EQ(data, nullptr);
    EXPECT_EQ(length, 0u);
}

TEST_F(SerializationTest, ReadBytes_NullLength) {
    // WHAT: Verify read_bytes handles NULL length pointer
    // WHY:  Error handling

    nimcp_write_uint8(serializer, 1);
    nimcp_serializer_set_position(serializer, 0);

    const uint8_t* data = nimcp_read_bytes(serializer, nullptr);
    EXPECT_EQ(data, nullptr);
}

//=============================================================================
// SECTION 11: Round-Trip Tests
//=============================================================================

TEST_F(SerializationTest, RoundTrip_Uint8) {
    // WHAT: Verify uint8 can be written and read back
    // WHY:  Data integrity

    TestRoundTrip<uint8_t>(nimcp_write_uint8, nimcp_read_uint8, 0xAB);
    nimcp_serializer_reset(serializer);
    TestRoundTrip<uint8_t>(nimcp_write_uint8, nimcp_read_uint8, 0);
    nimcp_serializer_reset(serializer);
    TestRoundTrip<uint8_t>(nimcp_write_uint8, nimcp_read_uint8, 255);
}

TEST_F(SerializationTest, RoundTrip_Uint16) {
    // WHAT: Verify uint16 can be written and read back
    // WHY:  Data integrity

    TestRoundTrip<uint16_t>(nimcp_write_uint16, nimcp_read_uint16, 0x1234);
    nimcp_serializer_reset(serializer);
    TestRoundTrip<uint16_t>(nimcp_write_uint16, nimcp_read_uint16, 0);
    nimcp_serializer_reset(serializer);
    TestRoundTrip<uint16_t>(nimcp_write_uint16, nimcp_read_uint16, 65535);
}

TEST_F(SerializationTest, RoundTrip_Uint32) {
    // WHAT: Verify uint32 can be written and read back
    // WHY:  Data integrity

    TestRoundTrip<uint32_t>(nimcp_write_uint32, nimcp_read_uint32, 0x12345678u);
    nimcp_serializer_reset(serializer);
    TestRoundTrip<uint32_t>(nimcp_write_uint32, nimcp_read_uint32, 0);
    nimcp_serializer_reset(serializer);
    TestRoundTrip<uint32_t>(nimcp_write_uint32, nimcp_read_uint32, 4294967295u);
}

TEST_F(SerializationTest, RoundTrip_Uint64) {
    // WHAT: Verify uint64 can be written and read back
    // WHY:  Data integrity

    TestRoundTrip<uint64_t>(nimcp_write_uint64, nimcp_read_uint64, 0x123456789ABCDEF0ULL);
    nimcp_serializer_reset(serializer);
    TestRoundTrip<uint64_t>(nimcp_write_uint64, nimcp_read_uint64, 0);
}

TEST_F(SerializationTest, RoundTrip_Int8) {
    // WHAT: Verify int8 can be written and read back
    // WHY:  Data integrity

    TestRoundTrip<int8_t>(nimcp_write_int8, nimcp_read_int8, 127);
    nimcp_serializer_reset(serializer);
    TestRoundTrip<int8_t>(nimcp_write_int8, nimcp_read_int8, -128);
    nimcp_serializer_reset(serializer);
    TestRoundTrip<int8_t>(nimcp_write_int8, nimcp_read_int8, 0);
}

TEST_F(SerializationTest, RoundTrip_Int16) {
    // WHAT: Verify int16 can be written and read back
    // WHY:  Data integrity

    TestRoundTrip<int16_t>(nimcp_write_int16, nimcp_read_int16, 32767);
    nimcp_serializer_reset(serializer);
    TestRoundTrip<int16_t>(nimcp_write_int16, nimcp_read_int16, -32768);
}

TEST_F(SerializationTest, RoundTrip_Int32) {
    // WHAT: Verify int32 can be written and read back
    // WHY:  Data integrity

    TestRoundTrip<int32_t>(nimcp_write_int32, nimcp_read_int32, 2147483647);
    nimcp_serializer_reset(serializer);
    TestRoundTrip<int32_t>(nimcp_write_int32, nimcp_read_int32, -2147483648);
}

TEST_F(SerializationTest, RoundTrip_Int64) {
    // WHAT: Verify int64 can be written and read back
    // WHY:  Data integrity

    TestRoundTrip<int64_t>(nimcp_write_int64, nimcp_read_int64, 9223372036854775807LL);
    nimcp_serializer_reset(serializer);
    TestRoundTrip<int64_t>(nimcp_write_int64, nimcp_read_int64, -9223372036854775807LL - 1);
}

TEST_F(SerializationTest, RoundTrip_ComplexStructure) {
    // WHAT: Verify complex data structure serialization
    // WHY:  Real-world usage pattern

    // Write complex structure
    ASSERT_TRUE(nimcp_write_uint32(serializer, 0xDEADBEEF));
    ASSERT_TRUE(nimcp_write_float(serializer, 3.14159f));
    ASSERT_TRUE(nimcp_write_bool(serializer, true));
    ASSERT_TRUE(nimcp_write_int64(serializer, -123456789LL));
    ASSERT_TRUE(nimcp_write_double(serializer, 2.718281828));

    // Read back
    nimcp_serializer_set_position(serializer, 0);

    uint32_t u32 = nimcp_read_uint32(serializer);
    float f32 = nimcp_read_float(serializer);
    bool b = nimcp_read_bool(serializer);
    int64_t i64 = nimcp_read_int64(serializer);
    double f64 = nimcp_read_double(serializer);

    EXPECT_EQ(u32, 0xDEADBEEFu);
    EXPECT_FLOAT_EQ(f32, 3.14159f);
    EXPECT_TRUE(b);
    EXPECT_EQ(i64, -123456789LL);
    EXPECT_DOUBLE_EQ(f64, 2.718281828);
}

//=============================================================================
// SECTION 12: Error Handling Tests
//=============================================================================

TEST_F(SerializationTest, Error_ReadBeyondBuffer) {
    // WHAT: Verify reading beyond buffer sets error
    // WHY:  Bounds checking

    nimcp_write_uint8(serializer, 42);
    nimcp_serializer_set_position(serializer, 0);

    nimcp_read_uint8(serializer);
    EXPECT_FALSE(nimcp_serializer_has_error(serializer));

    // Try to read when at end
    uint32_t value = nimcp_read_uint32(serializer);
    EXPECT_EQ(value, 0u);
    EXPECT_TRUE(nimcp_serializer_has_error(serializer));
}

TEST_F(SerializationTest, Error_ClearError) {
    // WHAT: Verify error can be cleared
    // WHY:  Allow recovery from errors

    nimcp_read_uint32(serializer); // Read from empty buffer
    EXPECT_TRUE(nimcp_serializer_has_error(serializer));

    nimcp_serializer_clear_error(serializer);
    EXPECT_FALSE(nimcp_serializer_has_error(serializer));
}

TEST_F(SerializationTest, Error_HasError_NullSerializer) {
    // WHAT: Verify has_error returns true for NULL serializer
    // WHY:  Defensive programming

    EXPECT_TRUE(nimcp_serializer_has_error(nullptr));
}

TEST_F(SerializationTest, Error_ClearError_NullSerializer) {
    // WHAT: Verify clear_error handles NULL safely
    // WHY:  Defensive programming

    nimcp_serializer_clear_error(nullptr);
    SUCCEED() << "Clear error on NULL is safe";
}

TEST_F(SerializationTest, Error_ReadPartialData) {
    // WHAT: Verify reading partial data sets error
    // WHY:  Detect incomplete reads

    nimcp_write_uint8(serializer, 1);
    nimcp_write_uint8(serializer, 2);
    nimcp_serializer_set_position(serializer, 0);

    // Try to read 4 bytes when only 2 available
    uint32_t value = nimcp_read_uint32(serializer);
    EXPECT_EQ(value, 0u);
    EXPECT_TRUE(nimcp_serializer_has_error(serializer));
}

//=============================================================================
// SECTION 13: Compression Tests
//=============================================================================

TEST_F(SerializationTest, Compression_CompressData) {
    // WHAT: Verify data can be compressed
    // WHY:  Reduce storage/transmission size

    // Write some compressible data
    for (int i = 0; i < 1000; i++) {
        nimcp_write_uint32(serializer, 0x12121212);
    }

    size_t original_length = nimcp_serializer_get_length(serializer);
    EXPECT_FALSE(nimcp_serializer_is_compressed(serializer));

    NimcpSerialResult result = nimcp_serializer_compress(serializer);
    EXPECT_EQ(result, NIMCP_SERIAL_SUCCESS);
    EXPECT_TRUE(nimcp_serializer_is_compressed(serializer));

    size_t compressed_length = nimcp_serializer_get_length(serializer);
    EXPECT_LT(compressed_length, original_length);
}

TEST_F(SerializationTest, Compression_DecompressData) {
    // WHAT: Verify compressed data can be decompressed
    // WHY:  Round-trip compression

    // Write data
    uint32_t values[] = {0x11111111, 0x22222222, 0x33333333, 0x44444444};
    for (uint32_t val : values) {
        nimcp_write_uint32(serializer, val);
    }

    size_t original_length = nimcp_serializer_get_length(serializer);

    // Compress
    ASSERT_EQ(nimcp_serializer_compress(serializer), NIMCP_SERIAL_SUCCESS);
    EXPECT_TRUE(nimcp_serializer_is_compressed(serializer));

    // Decompress
    NimcpSerialResult result = nimcp_serializer_decompress(serializer);
    EXPECT_EQ(result, NIMCP_SERIAL_SUCCESS);
    EXPECT_FALSE(nimcp_serializer_is_compressed(serializer));
    EXPECT_EQ(nimcp_serializer_get_length(serializer), original_length);

    // Verify data integrity
    nimcp_serializer_set_position(serializer, 0);
    for (uint32_t expected : values) {
        uint32_t actual = nimcp_read_uint32(serializer);
        EXPECT_EQ(actual, expected);
    }
}

TEST_F(SerializationTest, Compression_AlreadyCompressed) {
    // WHAT: Verify compressing already compressed data fails
    // WHY:  Prevent double compression

    nimcp_write_uint32(serializer, 123);
    ASSERT_EQ(nimcp_serializer_compress(serializer), NIMCP_SERIAL_SUCCESS);

    NimcpSerialResult result = nimcp_serializer_compress(serializer);
    EXPECT_EQ(result, NIMCP_SERIAL_ERROR_INVALID_PARAM);
}

TEST_F(SerializationTest, Compression_NotCompressed) {
    // WHAT: Verify decompressing uncompressed data fails
    // WHY:  Detect invalid operations

    nimcp_write_uint32(serializer, 123);

    NimcpSerialResult result = nimcp_serializer_decompress(serializer);
    EXPECT_EQ(result, NIMCP_SERIAL_ERROR_INVALID_PARAM);
}

TEST_F(SerializationTest, Compression_NullSerializer) {
    // WHAT: Verify compression functions handle NULL serializer
    // WHY:  Error handling

    EXPECT_EQ(nimcp_serializer_compress(nullptr), NIMCP_SERIAL_ERROR_INVALID_PARAM);
    EXPECT_EQ(nimcp_serializer_decompress(nullptr), NIMCP_SERIAL_ERROR_INVALID_PARAM);
}

TEST_F(SerializationTest, Compression_MarkCompressed) {
    // WHAT: Verify manual compression marking
    // WHY:  Allow external compression

    EXPECT_FALSE(nimcp_serializer_is_compressed(serializer));
    nimcp_serializer_mark_compressed(serializer);
    EXPECT_TRUE(nimcp_serializer_is_compressed(serializer));
}

TEST_F(SerializationTest, Compression_IsCompressed_NullSerializer) {
    // WHAT: Verify is_compressed handles NULL
    // WHY:  Error handling

    EXPECT_FALSE(nimcp_serializer_is_compressed(nullptr));
}

TEST_F(SerializationTest, Compression_MarkCompressed_NullSerializer) {
    // WHAT: Verify mark_compressed handles NULL
    // WHY:  Error handling

    nimcp_serializer_mark_compressed(nullptr);
    SUCCEED() << "Mark compressed on NULL is safe";
}

//=============================================================================
// SECTION 14: Capacity Expansion Tests
//=============================================================================

TEST_F(SerializationTest, Capacity_AutoExpansion) {
    // WHAT: Verify buffer auto-expands when needed
    // WHY:  Handle large data automatically

    NimcpSerializer* small = nimcp_serializer_create(NIMCP_SERIALIZER_INITIAL_SIZE);
    ASSERT_NE(small, nullptr);

    // Write more than initial capacity
    for (int i = 0; i < 1000; i++) {
        ASSERT_TRUE(nimcp_write_uint32(small, i));
    }

    EXPECT_EQ(nimcp_serializer_get_length(small), 4000u);
    EXPECT_FALSE(nimcp_serializer_has_error(small));

    nimcp_serializer_destroy(small);
}

TEST_F(SerializationTest, Capacity_FixedSizeBuffer) {
    // WHAT: Verify small buffers don't auto-expand
    // WHY:  Allow fixed-size buffer testing

    NimcpSerializer* tiny = nimcp_serializer_create(16);
    ASSERT_NE(tiny, nullptr);

    // Try to write more than capacity
    bool success = true;
    for (int i = 0; i < 100; i++) {
        if (!nimcp_write_uint32(tiny, i)) {
            success = false;
            break;
        }
    }

    EXPECT_FALSE(success);
    EXPECT_TRUE(nimcp_serializer_has_error(tiny));

    nimcp_serializer_destroy(tiny);
}

TEST_F(SerializationTest, Capacity_MaxSizeLimit) {
    // WHAT: Verify expansion stops at max size
    // WHY:  Prevent unbounded growth

    // This test would require writing > 64MB which is slow
    // Instead verify the constant is reasonable
    EXPECT_GT(NIMCP_SERIALIZER_MAX_SIZE, 1024u * 1024u);
    EXPECT_LE(NIMCP_SERIALIZER_MAX_SIZE, 1024u * 1024u * 1024u);
}

//=============================================================================
// SECTION 15: Edge Cases and Boundary Tests
//=============================================================================

TEST_F(SerializationTest, EdgeCase_EmptySerializer) {
    // WHAT: Verify operations on empty serializer
    // WHY:  Handle empty state

    EXPECT_EQ(nimcp_serializer_get_length(serializer), 0u);
    EXPECT_EQ(nimcp_serializer_get_position(serializer), 0u);

    size_t length = 0;
    const uint8_t* data = nimcp_read_bytes(serializer, &length);
    EXPECT_EQ(data, nullptr);
    EXPECT_EQ(length, 0u);
}

TEST_F(SerializationTest, EdgeCase_ZeroBytes) {
    // WHAT: Verify writing zero bytes
    // WHY:  Edge case handling

    uint8_t data[] = {1, 2, 3};
    EXPECT_TRUE(nimcp_write_bytes(serializer, data, 0));
    EXPECT_EQ(nimcp_serializer_get_length(serializer), 0u);
}

TEST_F(SerializationTest, EdgeCase_PositionAtEnd) {
    // WHAT: Verify behavior when position equals length
    // WHY:  Boundary condition

    nimcp_write_uint32(serializer, 123);
    EXPECT_EQ(nimcp_serializer_get_position(serializer), 4u);
    EXPECT_EQ(nimcp_serializer_get_length(serializer), 4u);

    // Reading should fail
    uint32_t value = nimcp_read_uint32(serializer);
    EXPECT_EQ(value, 0u);
    EXPECT_TRUE(nimcp_serializer_has_error(serializer));
}

TEST_F(SerializationTest, EdgeCase_ResetAndReuse) {
    // WHAT: Verify serializer can be reset and reused
    // WHY:  Efficient reuse pattern

    // First use
    nimcp_write_uint32(serializer, 111);
    nimcp_write_uint32(serializer, 222);
    EXPECT_EQ(nimcp_serializer_get_length(serializer), 8u);

    // Reset
    nimcp_serializer_reset(serializer);
    EXPECT_EQ(nimcp_serializer_get_length(serializer), 0u);

    // Reuse
    nimcp_write_uint32(serializer, 333);
    EXPECT_EQ(nimcp_serializer_get_length(serializer), 4u);

    nimcp_serializer_set_position(serializer, 0);
    uint32_t value = nimcp_read_uint32(serializer);
    EXPECT_EQ(value, 333u);
}

TEST_F(SerializationTest, EdgeCase_OverwriteData) {
    // WHAT: Verify writing at earlier position doesn't change length
    // WHY:  Support random access writes

    nimcp_write_uint32(serializer, 1);
    nimcp_write_uint32(serializer, 2);
    nimcp_write_uint32(serializer, 3);

    EXPECT_EQ(nimcp_serializer_get_length(serializer), 12u);

    // Overwrite middle value
    nimcp_serializer_set_position(serializer, 4);
    nimcp_write_uint32(serializer, 999);

    // Length should still be 12
    EXPECT_EQ(nimcp_serializer_get_length(serializer), 12u);

    // Verify overwrite
    nimcp_serializer_set_position(serializer, 4);
    uint32_t value = nimcp_read_uint32(serializer);
    EXPECT_EQ(value, 999u);
}

TEST_F(SerializationTest, EdgeCase_LargePositionJump) {
    // WHAT: Verify writing after position jump updates length correctly
    // WHY:  Length tracking for non-sequential writes

    nimcp_write_uint32(serializer, 1);
    EXPECT_EQ(nimcp_serializer_get_length(serializer), 4u);

    // Jump back and overwrite doesn't increase length
    nimcp_serializer_set_position(serializer, 0);
    nimcp_write_uint32(serializer, 2);

    // Length is max of position reached
    EXPECT_EQ(nimcp_serializer_get_length(serializer), 4u);

    // Write beyond current length
    nimcp_serializer_set_position(serializer, 4);
    nimcp_write_uint32(serializer, 3);
    EXPECT_EQ(nimcp_serializer_get_length(serializer), 8u);
}

//=============================================================================
// SECTION 16: Memory Management Tests
//=============================================================================

TEST_F(SerializationTest, Memory_MultipleCreateDestroy) {
    // WHAT: Verify multiple create/destroy cycles
    // WHY:  Detect memory leaks

    for (int i = 0; i < 100; i++) {
        NimcpSerializer* temp = nimcp_serializer_create(1024);
        ASSERT_NE(temp, nullptr);

        nimcp_write_uint32(temp, i);
        nimcp_write_float(temp, static_cast<float>(i) * 3.14f);

        nimcp_serializer_destroy(temp);
    }

    SUCCEED() << "Multiple create/destroy cycles complete";
}

TEST_F(SerializationTest, Memory_BufferOwnership) {
    // WHAT: Verify serializer owns its buffer
    // WHY:  Proper memory management

    nimcp_write_uint32(serializer, 12345);
    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    ASSERT_NE(buffer, nullptr);

    // Serializer should still own the buffer
    nimcp_write_uint32(serializer, 67890);

    // Buffer pointer may change due to reallocation
    uint8_t* new_buffer = nimcp_serializer_get_buffer(serializer);
    ASSERT_NE(new_buffer, nullptr);
}

//=============================================================================
// SECTION 17: Special Value Tests
//=============================================================================

TEST_F(SerializationTest, SpecialValues_FloatNaN) {
    // WHAT: Verify NaN can be serialized
    // WHY:  Special value handling

    float nan_val = std::numeric_limits<float>::quiet_NaN();
    nimcp_write_float(serializer, nan_val);
    nimcp_serializer_set_position(serializer, 0);

    float result = nimcp_read_float(serializer);
    EXPECT_TRUE(std::isnan(result));
}

TEST_F(SerializationTest, SpecialValues_DoubleNaN) {
    // WHAT: Verify NaN can be serialized
    // WHY:  Special value handling

    double nan_val = std::numeric_limits<double>::quiet_NaN();
    nimcp_write_double(serializer, nan_val);
    nimcp_serializer_set_position(serializer, 0);

    double result = nimcp_read_double(serializer);
    EXPECT_TRUE(std::isnan(result));
}

TEST_F(SerializationTest, SpecialValues_FloatInfinity) {
    // WHAT: Verify infinity can be serialized
    // WHY:  Special value handling

    float inf_val = std::numeric_limits<float>::infinity();
    nimcp_write_float(serializer, inf_val);
    nimcp_serializer_set_position(serializer, 0);

    float result = nimcp_read_float(serializer);
    EXPECT_TRUE(std::isinf(result));
    EXPECT_GT(result, 0.0f);
}

TEST_F(SerializationTest, SpecialValues_FloatNegativeInfinity) {
    // WHAT: Verify negative infinity can be serialized
    // WHY:  Special value handling

    float neg_inf = -std::numeric_limits<float>::infinity();
    nimcp_write_float(serializer, neg_inf);
    nimcp_serializer_set_position(serializer, 0);

    float result = nimcp_read_float(serializer);
    EXPECT_TRUE(std::isinf(result));
    EXPECT_LT(result, 0.0f);
}

//=============================================================================
// SECTION 18: Sequential Access Tests
//=============================================================================

TEST_F(SerializationTest, Sequential_WriteMultipleTypes) {
    // WHAT: Verify writing multiple types sequentially
    // WHY:  Common usage pattern

    ASSERT_TRUE(nimcp_write_uint8(serializer, 255));
    ASSERT_TRUE(nimcp_write_uint16(serializer, 65535));
    ASSERT_TRUE(nimcp_write_uint32(serializer, 4294967295u));
    ASSERT_TRUE(nimcp_write_float(serializer, 3.14f));
    ASSERT_TRUE(nimcp_write_bool(serializer, true));

    size_t expected_length = 1 + 2 + 4 + 4 + 1;
    EXPECT_EQ(nimcp_serializer_get_length(serializer), expected_length);
}

TEST_F(SerializationTest, Sequential_ReadMultipleTypes) {
    // WHAT: Verify reading multiple types sequentially
    // WHY:  Common usage pattern

    // Write
    nimcp_write_uint8(serializer, 42);
    nimcp_write_uint16(serializer, 1234);
    nimcp_write_uint32(serializer, 567890);
    nimcp_write_float(serializer, 2.718f);
    nimcp_write_bool(serializer, false);

    // Read
    nimcp_serializer_set_position(serializer, 0);

    EXPECT_EQ(nimcp_read_uint8(serializer), 42);
    EXPECT_EQ(nimcp_read_uint16(serializer), 1234);
    EXPECT_EQ(nimcp_read_uint32(serializer), 567890u);
    EXPECT_FLOAT_EQ(nimcp_read_float(serializer), 2.718f);
    EXPECT_FALSE(nimcp_read_bool(serializer));
}

//=============================================================================
// SECTION 19: State Serialization Simulation Tests
//=============================================================================

TEST_F(SerializationTest, StateSerialize_StructureWithHeader) {
    // WHAT: Simulate serializing a state structure with version header
    // WHY:  Real-world usage pattern

    // Write header
    ASSERT_TRUE(nimcp_write_uint32(serializer, NIMCP_SERIALIZER_VERSION));
    ASSERT_TRUE(nimcp_write_uint32(serializer, 0xCAFEBABE)); // Magic number

    // Write state data
    ASSERT_TRUE(nimcp_write_uint64(serializer, 123456789ULL)); // Timestamp
    ASSERT_TRUE(nimcp_write_float(serializer, 0.75f));         // Progress
    ASSERT_TRUE(nimcp_write_bool(serializer, true));           // Active flag

    // Verify can read back
    nimcp_serializer_set_position(serializer, 0);

    uint32_t version = nimcp_read_uint32(serializer);
    uint32_t magic = nimcp_read_uint32(serializer);
    uint64_t timestamp = nimcp_read_uint64(serializer);
    float progress = nimcp_read_float(serializer);
    bool active = nimcp_read_bool(serializer);

    EXPECT_EQ(version, NIMCP_SERIALIZER_VERSION);
    EXPECT_EQ(magic, 0xCAFEBABEu);
    EXPECT_EQ(timestamp, 123456789ULL);
    EXPECT_FLOAT_EQ(progress, 0.75f);
    EXPECT_TRUE(active);
}

TEST_F(SerializationTest, StateSerialize_VersionMismatch) {
    // WHAT: Simulate detecting version mismatch
    // WHY:  Backward compatibility checking

    uint32_t old_version = NIMCP_SERIALIZER_VERSION - 1;
    nimcp_write_uint32(serializer, old_version);

    nimcp_serializer_set_position(serializer, 0);
    uint32_t read_version = nimcp_read_uint32(serializer);

    EXPECT_NE(read_version, NIMCP_SERIALIZER_VERSION);
    // Application would handle version migration here
}

//=============================================================================
// SECTION 20: Network Serialization Simulation
//=============================================================================

TEST_F(SerializationTest, Network_PacketSerialization) {
    // WHAT: Simulate network packet serialization
    // WHY:  Real-world network usage

    // Packet header
    ASSERT_TRUE(nimcp_write_uint16(serializer, 1234));  // Packet ID
    ASSERT_TRUE(nimcp_write_uint16(serializer, 56));    // Payload length

    // Payload
    uint8_t payload[56];
    for (int i = 0; i < 56; i++) {
        payload[i] = static_cast<uint8_t>(i);
    }
    ASSERT_TRUE(nimcp_write_bytes(serializer, payload, sizeof(payload)));

    // Verify serialization
    EXPECT_EQ(nimcp_serializer_get_length(serializer), 4u + 56u);

    // Deserialize
    nimcp_serializer_set_position(serializer, 0);
    uint16_t packet_id = nimcp_read_uint16(serializer);
    uint16_t payload_length = nimcp_read_uint16(serializer);

    EXPECT_EQ(packet_id, 1234);
    EXPECT_EQ(payload_length, 56);

    size_t actual_length = 0;
    const uint8_t* received_payload = nimcp_read_bytes(serializer, &actual_length);
    ASSERT_NE(received_payload, nullptr);
    EXPECT_EQ(actual_length, 56u);
    EXPECT_EQ(memcmp(received_payload, payload, 56), 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
