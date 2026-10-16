/**
 * @file test_serialization.cpp
 * @brief Comprehensive unit tests for nimcp_serialization utilities
 *
 * WHAT: Tests for binary serialization including write/read operations and compression
 * WHY: Ensure serialization functions correctly handle all data types and edge cases
 * HOW: GoogleTest framework with fixture classes for organized testing
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "io/serialization/nimcp_serialization.h"

//=============================================================================
// Serializer Creation and Destruction Tests
//=============================================================================

class SerializerBasicTest : public ::testing::Test {
   protected:
    NimcpSerializer* serializer;

    void SetUp() override
    {
        serializer = nullptr;
    }

    void TearDown() override
    {
        if (serializer) {
            nimcp_serializer_destroy(serializer);
        }
    }
};

/**
 * WHAT: Test serializer creation with default capacity
 * WHY: Verify basic initialization works
 */
TEST_F(SerializerBasicTest, CreateWithDefaultCapacity)
{
    serializer = nimcp_serializer_create(0);
    ASSERT_NE(serializer, nullptr);

    EXPECT_EQ(nimcp_serializer_get_length(serializer), 0);
    EXPECT_EQ(nimcp_serializer_get_position(serializer), 0);
    EXPECT_NE(nimcp_serializer_get_buffer(serializer), nullptr);
}

/**
 * WHAT: Test serializer creation with custom capacity
 * WHY: Verify custom initialization works
 */
TEST_F(SerializerBasicTest, CreateWithCustomCapacity)
{
    serializer = nimcp_serializer_create(2048);
    ASSERT_NE(serializer, nullptr);

    EXPECT_EQ(nimcp_serializer_get_length(serializer), 0);
    EXPECT_EQ(nimcp_serializer_get_position(serializer), 0);
}

/**
 * WHAT: Test serializer creation with max size
 * WHY: Verify size limit enforcement
 */
TEST_F(SerializerBasicTest, CreateWithExcessiveSize)
{
    serializer = nimcp_serializer_create(NIMCP_SERIALIZER_MAX_SIZE + 1);
    EXPECT_EQ(serializer, nullptr);
}

/**
 * WHAT: Test serializer destruction with NULL
 * WHY: Verify NULL safety
 */
TEST_F(SerializerBasicTest, DestroyNullSerializer)
{
    nimcp_serializer_destroy(nullptr);
    // Should not crash
    SUCCEED();
}

/**
 * WHAT: Test serializer reset
 * WHY: Verify state is properly reset
 */
TEST_F(SerializerBasicTest, ResetSerializer)
{
    serializer = nimcp_serializer_create(0);
    ASSERT_NE(serializer, nullptr);

    // Write some data
    EXPECT_TRUE(nimcp_write_uint32(serializer, 12345));
    EXPECT_GT(nimcp_serializer_get_length(serializer), 0);

    // Reset
    nimcp_serializer_reset(serializer);

    EXPECT_EQ(nimcp_serializer_get_length(serializer), 0);
    EXPECT_EQ(nimcp_serializer_get_position(serializer), 0);
    EXPECT_FALSE(nimcp_serializer_has_error(serializer));
}

//=============================================================================
// Buffer Management Tests
//=============================================================================

class SerializerBufferTest : public ::testing::Test {
   protected:
    NimcpSerializer* serializer;

    void SetUp() override
    {
        serializer = nimcp_serializer_create(0);
    }

    void TearDown() override
    {
        nimcp_serializer_destroy(serializer);
    }
};

/**
 * WHAT: Test set buffer operation
 * WHY: Verify buffer can be set externally
 */
TEST_F(SerializerBufferTest, SetBuffer)
{
    uint8_t data[] = {1, 2, 3, 4, 5};
    EXPECT_TRUE(nimcp_serializer_set_buffer(serializer, data, sizeof(data)));

    EXPECT_EQ(nimcp_serializer_get_length(serializer), sizeof(data));
    EXPECT_EQ(nimcp_serializer_get_position(serializer), 0);

    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(memcmp(buffer, data, sizeof(data)), 0);
}

/**
 * WHAT: Test set buffer with NULL data
 * WHY: Verify error handling
 */
TEST_F(SerializerBufferTest, SetBufferWithNull)
{
    EXPECT_FALSE(nimcp_serializer_set_buffer(serializer, nullptr, 10));
    EXPECT_FALSE(nimcp_serializer_set_buffer(nullptr, (uint8_t*) "data", 4));
}

/**
 * WHAT: Test set buffer with excessive size
 * WHY: Verify size limit enforcement
 */
TEST_F(SerializerBufferTest, SetBufferExcessiveSize)
{
    uint8_t data[100];
    EXPECT_FALSE(nimcp_serializer_set_buffer(serializer, data, NIMCP_SERIALIZER_MAX_SIZE + 1));
}

/**
 * WHAT: Test get buffer from NULL serializer
 * WHY: Verify NULL safety
 */
TEST_F(SerializerBufferTest, GetBufferFromNull)
{
    EXPECT_EQ(nimcp_serializer_get_buffer(nullptr), nullptr);
    EXPECT_EQ(nimcp_serializer_get_length(nullptr), 0);
    EXPECT_EQ(nimcp_serializer_get_position(nullptr), 0);
}

/**
 * WHAT: Test position management
 * WHY: Verify position can be set and retrieved
 */
TEST_F(SerializerBufferTest, PositionManagement)
{
    uint8_t data[100];
    memset(data, 0, sizeof(data));
    EXPECT_TRUE(nimcp_serializer_set_buffer(serializer, data, sizeof(data)));

    EXPECT_TRUE(nimcp_serializer_set_position(serializer, 50));
    EXPECT_EQ(nimcp_serializer_get_position(serializer), 50);

    // Try to set position beyond length
    EXPECT_FALSE(nimcp_serializer_set_position(serializer, 101));

    // NULL serializer
    EXPECT_FALSE(nimcp_serializer_set_position(nullptr, 0));
}

//=============================================================================
// Write Operations Tests
//=============================================================================

class SerializerWriteTest : public ::testing::Test {
   protected:
    NimcpSerializer* serializer;

    void SetUp() override
    {
        serializer = nimcp_serializer_create(0);
    }

    void TearDown() override
    {
        nimcp_serializer_destroy(serializer);
    }
};

/**
 * WHAT: Test writing uint8 values
 * WHY: Verify smallest integer type serialization
 */
TEST_F(SerializerWriteTest, WriteUint8)
{
    EXPECT_TRUE(nimcp_write_uint8(serializer, 0));
    EXPECT_TRUE(nimcp_write_uint8(serializer, 127));
    EXPECT_TRUE(nimcp_write_uint8(serializer, 255));

    EXPECT_EQ(nimcp_serializer_get_length(serializer), 3);
    EXPECT_EQ(nimcp_serializer_get_position(serializer), 3);

    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    EXPECT_EQ(buffer[0], 0);
    EXPECT_EQ(buffer[1], 127);
    EXPECT_EQ(buffer[2], 255);
}

/**
 * WHAT: Test writing uint16 values
 * WHY: Verify 16-bit integer serialization with endianness
 */
TEST_F(SerializerWriteTest, WriteUint16)
{
    EXPECT_TRUE(nimcp_write_uint16(serializer, 0x1234));
    EXPECT_EQ(nimcp_serializer_get_length(serializer), 2);

    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    // Big-endian format
    EXPECT_EQ(buffer[0], 0x12);
    EXPECT_EQ(buffer[1], 0x34);
}

/**
 * WHAT: Test writing uint32 values
 * WHY: Verify 32-bit integer serialization
 */
TEST_F(SerializerWriteTest, WriteUint32)
{
    EXPECT_TRUE(nimcp_write_uint32(serializer, 0x12345678));
    EXPECT_EQ(nimcp_serializer_get_length(serializer), 4);

    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    // Big-endian format
    EXPECT_EQ(buffer[0], 0x12);
    EXPECT_EQ(buffer[1], 0x34);
    EXPECT_EQ(buffer[2], 0x56);
    EXPECT_EQ(buffer[3], 0x78);
}

/**
 * WHAT: Test writing uint64 values
 * WHY: Verify 64-bit integer serialization
 */
TEST_F(SerializerWriteTest, WriteUint64)
{
    EXPECT_TRUE(nimcp_write_uint64(serializer, 0x123456789ABCDEF0ULL));
    EXPECT_EQ(nimcp_serializer_get_length(serializer), 8);

    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    // Big-endian format
    EXPECT_EQ(buffer[0], 0x12);
    EXPECT_EQ(buffer[1], 0x34);
    EXPECT_EQ(buffer[2], 0x56);
    EXPECT_EQ(buffer[3], 0x78);
    EXPECT_EQ(buffer[4], 0x9A);
    EXPECT_EQ(buffer[5], 0xBC);
    EXPECT_EQ(buffer[6], 0xDE);
    EXPECT_EQ(buffer[7], 0xF0);
}

/**
 * WHAT: Test writing signed integers
 * WHY: Verify signed integer serialization
 */
TEST_F(SerializerWriteTest, WriteSignedIntegers)
{
    EXPECT_TRUE(nimcp_write_int8(serializer, -128));
    EXPECT_TRUE(nimcp_write_int16(serializer, -32768));
    EXPECT_TRUE(nimcp_write_int32(serializer, -2147483648));
    EXPECT_TRUE(nimcp_write_int64(serializer, -9223372036854775807LL - 1));

    EXPECT_EQ(nimcp_serializer_get_length(serializer), 1 + 2 + 4 + 8);
}

/**
 * WHAT: Test writing float values
 * WHY: Verify floating-point serialization
 */
TEST_F(SerializerWriteTest, WriteFloat)
{
    EXPECT_TRUE(nimcp_write_float(serializer, 3.14159f));
    EXPECT_EQ(nimcp_serializer_get_length(serializer), 4);
}

/**
 * WHAT: Test writing double values
 * WHY: Verify double-precision floating-point serialization
 */
TEST_F(SerializerWriteTest, WriteDouble)
{
    EXPECT_TRUE(nimcp_write_double(serializer, 3.141592653589793));
    EXPECT_EQ(nimcp_serializer_get_length(serializer), 8);
}

/**
 * WHAT: Test writing boolean values
 * WHY: Verify boolean serialization
 */
TEST_F(SerializerWriteTest, WriteBool)
{
    EXPECT_TRUE(nimcp_write_bool(serializer, true));
    EXPECT_TRUE(nimcp_write_bool(serializer, false));
    EXPECT_EQ(nimcp_serializer_get_length(serializer), 2);

    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    EXPECT_EQ(buffer[0], 1);
    EXPECT_EQ(buffer[1], 0);
}

/**
 * WHAT: Test writing byte arrays
 * WHY: Verify raw byte serialization
 */
TEST_F(SerializerWriteTest, WriteBytes)
{
    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    EXPECT_TRUE(nimcp_write_bytes(serializer, data, sizeof(data)));
    EXPECT_EQ(nimcp_serializer_get_length(serializer), sizeof(data));

    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    EXPECT_EQ(memcmp(buffer, data, sizeof(data)), 0);
}

/**
 * WHAT: Test writing with NULL serializer
 * WHY: Verify error handling
 */
TEST_F(SerializerWriteTest, WriteWithNullSerializer)
{
    EXPECT_FALSE(nimcp_write_uint8(nullptr, 0));
    EXPECT_FALSE(nimcp_write_uint16(nullptr, 0));
    EXPECT_FALSE(nimcp_write_uint32(nullptr, 0));
    EXPECT_FALSE(nimcp_write_bytes(nullptr, (uint8_t*) "data", 4));
}

//=============================================================================
// Read Operations Tests
//=============================================================================

class SerializerReadTest : public ::testing::Test {
   protected:
    NimcpSerializer* serializer;

    void SetUp() override
    {
        serializer = nimcp_serializer_create(0);
    }

    void TearDown() override
    {
        nimcp_serializer_destroy(serializer);
    }
};

/**
 * WHAT: Test reading uint8 values
 * WHY: Verify deserialization matches serialization
 */
TEST_F(SerializerReadTest, ReadUint8)
{
    nimcp_write_uint8(serializer, 42);
    nimcp_write_uint8(serializer, 255);

    nimcp_serializer_set_position(serializer, 0);

    EXPECT_EQ(nimcp_read_uint8(serializer), 42);
    EXPECT_EQ(nimcp_read_uint8(serializer), 255);
    EXPECT_EQ(nimcp_serializer_get_position(serializer), 2);
}

/**
 * WHAT: Test reading uint16 values
 * WHY: Verify 16-bit deserialization with endianness
 */
TEST_F(SerializerReadTest, ReadUint16)
{
    nimcp_write_uint16(serializer, 0x1234);
    nimcp_serializer_set_position(serializer, 0);

    EXPECT_EQ(nimcp_read_uint16(serializer), 0x1234);
}

/**
 * WHAT: Test reading uint32 values
 * WHY: Verify 32-bit deserialization
 */
TEST_F(SerializerReadTest, ReadUint32)
{
    nimcp_write_uint32(serializer, 0x12345678);
    nimcp_serializer_set_position(serializer, 0);

    EXPECT_EQ(nimcp_read_uint32(serializer), 0x12345678);
}

/**
 * WHAT: Test reading uint64 values
 * WHY: Verify 64-bit deserialization
 */
TEST_F(SerializerReadTest, ReadUint64)
{
    nimcp_write_uint64(serializer, 0x123456789ABCDEF0ULL);
    nimcp_serializer_set_position(serializer, 0);

    EXPECT_EQ(nimcp_read_uint64(serializer), 0x123456789ABCDEF0ULL);
}

/**
 * WHAT: Test reading signed integers
 * WHY: Verify signed integer deserialization
 */
TEST_F(SerializerReadTest, ReadSignedIntegers)
{
    nimcp_write_int8(serializer, -42);
    nimcp_write_int16(serializer, -1234);
    nimcp_write_int32(serializer, -123456);
    nimcp_write_int64(serializer, -123456789012LL);

    nimcp_serializer_set_position(serializer, 0);

    EXPECT_EQ(nimcp_read_int8(serializer), -42);
    EXPECT_EQ(nimcp_read_int16(serializer), -1234);
    EXPECT_EQ(nimcp_read_int32(serializer), -123456);
    EXPECT_EQ(nimcp_read_int64(serializer), -123456789012LL);
}

/**
 * WHAT: Test reading float values
 * WHY: Verify floating-point deserialization
 */
TEST_F(SerializerReadTest, ReadFloat)
{
    float original = 3.14159f;
    nimcp_write_float(serializer, original);
    nimcp_serializer_set_position(serializer, 0);

    float read_value = nimcp_read_float(serializer);
    EXPECT_FLOAT_EQ(read_value, original);
}

/**
 * WHAT: Test reading double values
 * WHY: Verify double-precision deserialization
 */
TEST_F(SerializerReadTest, ReadDouble)
{
    double original = 3.141592653589793;
    nimcp_write_double(serializer, original);
    nimcp_serializer_set_position(serializer, 0);

    double read_value = nimcp_read_double(serializer);
    EXPECT_DOUBLE_EQ(read_value, original);
}

/**
 * WHAT: Test reading boolean values
 * WHY: Verify boolean deserialization
 */
TEST_F(SerializerReadTest, ReadBool)
{
    nimcp_write_bool(serializer, true);
    nimcp_write_bool(serializer, false);
    nimcp_serializer_set_position(serializer, 0);

    EXPECT_TRUE(nimcp_read_bool(serializer));
    EXPECT_FALSE(nimcp_read_bool(serializer));
}

/**
 * WHAT: Test reading byte arrays
 * WHY: Verify raw byte deserialization
 */
TEST_F(SerializerReadTest, ReadBytes)
{
    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    nimcp_write_bytes(serializer, data, sizeof(data));
    nimcp_serializer_set_position(serializer, 0);

    size_t length = 0;
    const uint8_t* read_data = nimcp_read_bytes(serializer, &length);

    ASSERT_NE(read_data, nullptr);
    EXPECT_EQ(length, sizeof(data));
    EXPECT_EQ(memcmp(read_data, data, sizeof(data)), 0);
}

/**
 * WHAT: Test reading beyond buffer bounds
 * WHY: Verify bounds checking
 */
TEST_F(SerializerReadTest, ReadBeyondBounds)
{
    nimcp_write_uint8(serializer, 42);
    nimcp_serializer_set_position(serializer, 0);

    // Read the valid byte
    EXPECT_EQ(nimcp_read_uint8(serializer), 42);
    EXPECT_FALSE(nimcp_serializer_has_error(serializer));

    // Try to read beyond
    uint16_t value = nimcp_read_uint16(serializer);
    EXPECT_TRUE(nimcp_serializer_has_error(serializer));

    // Clear error
    nimcp_serializer_clear_error(serializer);
    EXPECT_FALSE(nimcp_serializer_has_error(serializer));
}

//=============================================================================
// Compression Tests
//=============================================================================

class SerializerCompressionTest : public ::testing::Test {
   protected:
    NimcpSerializer* serializer;

    void SetUp() override
    {
        serializer = nimcp_serializer_create(0);
    }

    void TearDown() override
    {
        nimcp_serializer_destroy(serializer);
    }
};

/**
 * WHAT: Test compression of data
 * WHY: Verify LZ4 compression works
 */
TEST_F(SerializerCompressionTest, CompressData)
{
    // Write repetitive data (highly compressible)
    for (int i = 0; i < 1000; i++) {
        nimcp_write_uint32(serializer, 0x12345678);
    }

    size_t original_length = nimcp_serializer_get_length(serializer);
    EXPECT_EQ(original_length, 4000);

    NimcpSerialResult result = nimcp_serializer_compress(serializer);
    EXPECT_EQ(result, NIMCP_SERIAL_SUCCESS);

    size_t compressed_length = nimcp_serializer_get_length(serializer);
    EXPECT_LT(compressed_length, original_length);
}

/**
 * WHAT: Test decompression of data
 * WHY: Verify round-trip compression/decompression
 */
TEST_F(SerializerCompressionTest, CompressAndDecompress)
{
    // Write test data
    for (int i = 0; i < 100; i++) {
        nimcp_write_uint32(serializer, i);
    }

    size_t original_length = nimcp_serializer_get_length(serializer);

    // Compress
    EXPECT_EQ(nimcp_serializer_compress(serializer), NIMCP_SERIAL_SUCCESS);

    // Decompress
    EXPECT_EQ(nimcp_serializer_decompress(serializer), NIMCP_SERIAL_SUCCESS);

    EXPECT_EQ(nimcp_serializer_get_length(serializer), original_length);

    // Verify data integrity
    nimcp_serializer_set_position(serializer, 0);
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(nimcp_read_uint32(serializer), (uint32_t) i);
    }
}

/**
 * WHAT: Test compression of already compressed data
 * WHY: Verify error handling
 */
TEST_F(SerializerCompressionTest, CompressAlreadyCompressed)
{
    nimcp_write_uint32(serializer, 12345);

    EXPECT_EQ(nimcp_serializer_compress(serializer), NIMCP_SERIAL_SUCCESS);
    EXPECT_EQ(nimcp_serializer_compress(serializer), NIMCP_SERIAL_ERROR_INVALID_PARAM);
}

/**
 * WHAT: Test decompression of uncompressed data
 * WHY: Verify error handling
 */
TEST_F(SerializerCompressionTest, DecompressUncompressed)
{
    nimcp_write_uint32(serializer, 12345);

    EXPECT_EQ(nimcp_serializer_decompress(serializer), NIMCP_SERIAL_ERROR_INVALID_PARAM);
}

/**
 * WHAT: Test compression with NULL serializer
 * WHY: Verify error handling
 */
TEST_F(SerializerCompressionTest, CompressWithNull)
{
    EXPECT_EQ(nimcp_serializer_compress(nullptr), NIMCP_SERIAL_ERROR_INVALID_PARAM);
    EXPECT_EQ(nimcp_serializer_decompress(nullptr), NIMCP_SERIAL_ERROR_INVALID_PARAM);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

class SerializerErrorTest : public ::testing::Test {
   protected:
    NimcpSerializer* serializer;

    void SetUp() override
    {
        serializer = nimcp_serializer_create(0);
    }

    void TearDown() override
    {
        nimcp_serializer_destroy(serializer);
    }
};

/**
 * WHAT: Test error state tracking
 * WHY: Verify error state is properly maintained
 */
TEST_F(SerializerErrorTest, ErrorStateTracking)
{
    EXPECT_FALSE(nimcp_serializer_has_error(serializer));

    // Write some data
    nimcp_write_uint8(serializer, 42);
    nimcp_serializer_set_position(serializer, 0);

    // Read successfully
    nimcp_read_uint8(serializer);
    EXPECT_FALSE(nimcp_serializer_has_error(serializer));

    // Read beyond bounds
    nimcp_read_uint32(serializer);
    EXPECT_TRUE(nimcp_serializer_has_error(serializer));

    // Clear error
    nimcp_serializer_clear_error(serializer);
    EXPECT_FALSE(nimcp_serializer_has_error(serializer));
}

/**
 * WHAT: Test error check with NULL serializer
 * WHY: Verify NULL safety
 */
TEST_F(SerializerErrorTest, ErrorCheckWithNull)
{
    EXPECT_TRUE(nimcp_serializer_has_error(nullptr));

    // Clear should not crash
    nimcp_serializer_clear_error(nullptr);
    SUCCEED();
}

//=============================================================================
// Integration Tests
//=============================================================================

class SerializerIntegrationTest : public ::testing::Test {
   protected:
    NimcpSerializer* serializer;

    void SetUp() override
    {
        serializer = nimcp_serializer_create(0);
    }

    void TearDown() override
    {
        nimcp_serializer_destroy(serializer);
    }
};

/**
 * WHAT: Test complex data structure serialization
 * WHY: Verify real-world usage scenario
 */
TEST_F(SerializerIntegrationTest, ComplexStructureSerialization)
{
    // Serialize a complex structure
    nimcp_write_uint8(serializer, 1);         // Version
    nimcp_write_uint32(serializer, 12345);    // ID
    nimcp_write_double(serializer, 3.14159);  // Value
    nimcp_write_bool(serializer, true);       // Flag

    uint8_t name[] = "TestName";
    nimcp_write_bytes(serializer, name, sizeof(name));

    // Reset position for reading
    nimcp_serializer_set_position(serializer, 0);

    // Deserialize
    EXPECT_EQ(nimcp_read_uint8(serializer), 1);
    EXPECT_EQ(nimcp_read_uint32(serializer), 12345);
    EXPECT_DOUBLE_EQ(nimcp_read_double(serializer), 3.14159);
    EXPECT_TRUE(nimcp_read_bool(serializer));

    size_t length = 0;
    const uint8_t* read_name = nimcp_read_bytes(serializer, &length);
    EXPECT_EQ(length, sizeof(name));
    EXPECT_EQ(memcmp(read_name, name, sizeof(name)), 0);
}

/**
 * WHAT: Test serialization with compression
 * WHY: Verify compressed serialization workflow
 */
TEST_F(SerializerIntegrationTest, SerializeCompressDeserialize)
{
    // Write compressible data
    for (int i = 0; i < 100; i++) {
        nimcp_write_uint64(serializer, 0xDEADBEEFDEADBEEFULL);
    }

    // Compress
    EXPECT_EQ(nimcp_serializer_compress(serializer), NIMCP_SERIAL_SUCCESS);

    // Simulate sending over network (get buffer)
    size_t compressed_size = nimcp_serializer_get_length(serializer);
    uint8_t* compressed_data = new uint8_t[compressed_size];
    memcpy(compressed_data, nimcp_serializer_get_buffer(serializer), compressed_size);

    // Create new serializer for "receiving" end
    NimcpSerializer* recv_serializer = nimcp_serializer_create(0);
    nimcp_serializer_set_buffer(recv_serializer, compressed_data, compressed_size);
    nimcp_serializer_mark_compressed(recv_serializer);  // Mark as compressed before decompression

    // Decompress
    EXPECT_EQ(nimcp_serializer_decompress(recv_serializer), NIMCP_SERIAL_SUCCESS);

    // Verify data
    nimcp_serializer_set_position(recv_serializer, 0);
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(nimcp_read_uint64(recv_serializer), 0xDEADBEEFDEADBEEFULL);
    }

    delete[] compressed_data;
    nimcp_serializer_destroy(recv_serializer);
}

/**
 * WHAT: Test multiple write/read cycles
 * WHY: Verify serializer can be reused
 */
TEST_F(SerializerIntegrationTest, MultipleWriteReadCycles)
{
    for (int cycle = 0; cycle < 5; cycle++) {
        nimcp_serializer_reset(serializer);

        // Write
        nimcp_write_uint32(serializer, cycle * 100);
        nimcp_write_float(serializer, cycle * 1.5f);

        // Read
        nimcp_serializer_set_position(serializer, 0);
        EXPECT_EQ(nimcp_read_uint32(serializer), (uint32_t) (cycle * 100));
        EXPECT_FLOAT_EQ(nimcp_read_float(serializer), cycle * 1.5f);
    }
}

/**
 * WHAT: Test automatic buffer expansion
 * WHY: Verify buffer grows as needed
 */
TEST_F(SerializerIntegrationTest, AutomaticBufferExpansion)
{
    // Write more data than initial capacity
    for (int i = 0; i < 10000; i++) {
        EXPECT_TRUE(nimcp_write_uint32(serializer, i));
    }

    EXPECT_EQ(nimcp_serializer_get_length(serializer), 40000);

    // Verify data
    nimcp_serializer_set_position(serializer, 0);
    for (int i = 0; i < 10000; i++) {
        EXPECT_EQ(nimcp_read_uint32(serializer), (uint32_t) i);
    }
}
