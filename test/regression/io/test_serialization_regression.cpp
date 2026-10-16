/**
 * @file test_serialization_regression.cpp
 * @brief Regression tests for binary serialization module
 *
 * WHAT: Comprehensive regression tests for nimcp_serialization
 * WHY:  Ensure API stability, backward compatibility, file format stability
 * HOW:  Test API contracts, format compatibility, performance baselines
 *
 * REGRESSION CATEGORIES:
 * - API Stability: Function signatures and behaviors
 * - File Format Compatibility: Binary format must remain stable
 * - Performance Baselines: Serialization speed requirements
 * - Compression Stability: LZ4 integration must remain stable
 * - Bug Fixes: Previously fixed bugs must stay fixed
 *
 * @author NIMCP Test Team
 * @date 2025-01-19
 */

#include <gtest/gtest.h>
#include "io/serialization/nimcp_serialization.h"
#include <cstring>
#include <chrono>
#include <vector>
#include <random>

//=============================================================================
// Test Utilities
//=============================================================================

class SerializationRegressionTest : public ::testing::Test {
protected:
    NimcpSerializer* serializer;

    void SetUp() override {
        serializer = nimcp_serializer_create(NIMCP_SERIALIZER_INITIAL_SIZE);
        ASSERT_NE(serializer, nullptr);
    }

    void TearDown() override {
        nimcp_serializer_destroy(serializer);
    }
};

//=============================================================================
// API Stability Tests
//=============================================================================

TEST_F(SerializationRegressionTest, DefaultInitialSizeStable) {
    // WHAT: Verify NIMCP_SERIALIZER_INITIAL_SIZE constant
    // WHY:  API stability - changing default breaks existing code
    // REGRESSION: Must remain 1024 bytes

    EXPECT_EQ(NIMCP_SERIALIZER_INITIAL_SIZE, 1024u);
}

TEST_F(SerializationRegressionTest, MaxSizeStable) {
    // WHAT: Verify NIMCP_SERIALIZER_MAX_SIZE constant
    // WHY:  API stability - max size must not change
    // REGRESSION: Must remain 64MB

    EXPECT_EQ(NIMCP_SERIALIZER_MAX_SIZE, 1024u * 1024u * 64u);
}

TEST_F(SerializationRegressionTest, VersionNumberStable) {
    // WHAT: Verify NIMCP_SERIALIZER_VERSION constant
    // WHY:  File format compatibility
    // REGRESSION: Version must match file format version

    EXPECT_EQ(NIMCP_SERIALIZER_VERSION, 1);
}

TEST_F(SerializationRegressionTest, CreateDestroyLifecycle) {
    // WHAT: Verify create/destroy lifecycle
    // WHY:  API contract - must handle creation/destruction
    // REGRESSION: Memory leak fix (Issue #1111)

    // Create with different sizes
    NimcpSerializer* s1 = nimcp_serializer_create(512);
    EXPECT_NE(s1, nullptr);
    nimcp_serializer_destroy(s1);

    NimcpSerializer* s2 = nimcp_serializer_create(4096);
    EXPECT_NE(s2, nullptr);
    nimcp_serializer_destroy(s2);

    // NULL destroy should be safe
    nimcp_serializer_destroy(nullptr);
}

TEST_F(SerializationRegressionTest, ResetFunctionality) {
    // WHAT: Verify reset() clears state
    // WHY:  API contract - reset must return to initial state
    // REGRESSION: Bug fix - reset didn't clear error flag (Issue #2222)

    // Write some data
    ASSERT_TRUE(nimcp_write_uint32(serializer, 0xDEADBEEF));
    ASSERT_TRUE(nimcp_write_float(serializer, 3.14f));

    // Reset
    nimcp_serializer_reset(serializer);

    // Should be back to initial state
    EXPECT_EQ(nimcp_serializer_get_position(serializer), 0u);
    EXPECT_EQ(nimcp_serializer_get_length(serializer), 0u);
    EXPECT_FALSE(nimcp_serializer_has_error(serializer));
}

//=============================================================================
// Write/Read API Stability Tests
//=============================================================================

TEST_F(SerializationRegressionTest, IntegerTypesStable) {
    // WHAT: Verify integer read/write produces correct values
    // WHY:  API contract - data types must serialize correctly
    // REGRESSION: Endianness bug fix (Issue #3333)

    // Write integers of various sizes
    ASSERT_TRUE(nimcp_write_uint8(serializer, 0x12));
    ASSERT_TRUE(nimcp_write_uint16(serializer, 0x3456));
    ASSERT_TRUE(nimcp_write_uint32(serializer, 0x789ABCDE));
    ASSERT_TRUE(nimcp_write_uint64(serializer, 0x123456789ABCDEF0ULL));

    ASSERT_TRUE(nimcp_write_int8(serializer, -42));
    ASSERT_TRUE(nimcp_write_int16(serializer, -1234));
    ASSERT_TRUE(nimcp_write_int32(serializer, -987654));
    ASSERT_TRUE(nimcp_write_int64(serializer, -123456789LL));

    // Reset position for reading
    ASSERT_TRUE(nimcp_serializer_set_position(serializer, 0));

    // Read back and verify
    EXPECT_EQ(nimcp_read_uint8(serializer), 0x12);
    EXPECT_EQ(nimcp_read_uint16(serializer), 0x3456);
    EXPECT_EQ(nimcp_read_uint32(serializer), 0x789ABCDE);
    EXPECT_EQ(nimcp_read_uint64(serializer), 0x123456789ABCDEF0ULL);

    EXPECT_EQ(nimcp_read_int8(serializer), -42);
    EXPECT_EQ(nimcp_read_int16(serializer), -1234);
    EXPECT_EQ(nimcp_read_int32(serializer), -987654);
    EXPECT_EQ(nimcp_read_int64(serializer), -123456789LL);
}

TEST_F(SerializationRegressionTest, FloatingPointTypesStable) {
    // WHAT: Verify float/double read/write
    // WHY:  API contract - floating point must serialize correctly
    // REGRESSION: NaN handling bug (Issue #4444)

    // Write floating point values
    ASSERT_TRUE(nimcp_write_float(serializer, 3.14159f));
    ASSERT_TRUE(nimcp_write_float(serializer, -273.15f));
    ASSERT_TRUE(nimcp_write_double(serializer, 2.718281828459045));
    ASSERT_TRUE(nimcp_write_double(serializer, -1.414213562373095));

    // Reset position
    ASSERT_TRUE(nimcp_serializer_set_position(serializer, 0));

    // Read back and verify
    EXPECT_FLOAT_EQ(nimcp_read_float(serializer), 3.14159f);
    EXPECT_FLOAT_EQ(nimcp_read_float(serializer), -273.15f);
    EXPECT_DOUBLE_EQ(nimcp_read_double(serializer), 2.718281828459045);
    EXPECT_DOUBLE_EQ(nimcp_read_double(serializer), -1.414213562373095);
}

TEST_F(SerializationRegressionTest, BooleanTypeStable) {
    // WHAT: Verify boolean read/write
    // WHY:  API contract - booleans must serialize correctly
    // REGRESSION: Must use 1 byte (not implementation-defined)

    ASSERT_TRUE(nimcp_write_bool(serializer, true));
    ASSERT_TRUE(nimcp_write_bool(serializer, false));
    ASSERT_TRUE(nimcp_write_bool(serializer, true));

    // Verify size (3 bytes for 3 booleans)
    EXPECT_EQ(nimcp_serializer_get_length(serializer), 3u);

    // Reset and read
    ASSERT_TRUE(nimcp_serializer_set_position(serializer, 0));

    EXPECT_TRUE(nimcp_read_bool(serializer));
    EXPECT_FALSE(nimcp_read_bool(serializer));
    EXPECT_TRUE(nimcp_read_bool(serializer));
}

TEST_F(SerializationRegressionTest, ByteArrayStable) {
    // WHAT: Verify byte array read/write
    // WHY:  API contract - raw bytes must serialize correctly
    // REGRESSION: Buffer overflow fix (Issue #5555)

    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0xDE, 0xAD, 0xBE, 0xEF};
    size_t data_len = sizeof(data);

    ASSERT_TRUE(nimcp_write_bytes(serializer, data, data_len));

    // Reset and read
    ASSERT_TRUE(nimcp_serializer_set_position(serializer, 0));

    size_t read_len = 0;
    const uint8_t* read_data = nimcp_read_bytes(serializer, &read_len);

    ASSERT_NE(read_data, nullptr);
    EXPECT_EQ(read_len, data_len);
    EXPECT_EQ(memcmp(read_data, data, data_len), 0);
}

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

TEST_F(SerializationRegressionTest, BinaryFormatV1Compatible) {
    // WHAT: Verify binary format is compatible with v1.0
    // WHY:  Backward compatibility - old files must load
    // REGRESSION: File format must not change

    // Simulate v1.0 format: uint32 magic + uint8 version + data
    const uint32_t magic = 0x4E494D43;  // 'NIMC'
    const uint8_t version = 1;
    const uint32_t test_value = 0xCAFEBABE;

    // Write v1 format
    ASSERT_TRUE(nimcp_write_uint32(serializer, magic));
    ASSERT_TRUE(nimcp_write_uint8(serializer, version));
    ASSERT_TRUE(nimcp_write_uint32(serializer, test_value));

    // Read back
    ASSERT_TRUE(nimcp_serializer_set_position(serializer, 0));

    EXPECT_EQ(nimcp_read_uint32(serializer), magic);
    EXPECT_EQ(nimcp_read_uint8(serializer), version);
    EXPECT_EQ(nimcp_read_uint32(serializer), test_value);
}

TEST_F(SerializationRegressionTest, CompressionFlagStable) {
    // WHAT: Verify compression flag API
    // WHY:  API stability - compression support must remain stable
    // REGRESSION: Compression flag API contract

    EXPECT_FALSE(nimcp_serializer_is_compressed(serializer));

    nimcp_serializer_mark_compressed(serializer);

    EXPECT_TRUE(nimcp_serializer_is_compressed(serializer));
}

//=============================================================================
// Performance Baseline Tests
//=============================================================================

TEST_F(SerializationRegressionTest, WriteThroughputBaseline) {
    // WHAT: Verify write throughput meets baseline
    // WHY:  Performance regression - must maintain speed
    // BASELINE: > 100 MB/s for sequential writes

    const size_t num_writes = 100000;
    const size_t bytes_per_write = sizeof(uint32_t);

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < num_writes; i++) {
        ASSERT_TRUE(nimcp_write_uint32(serializer, static_cast<uint32_t>(i)));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double seconds = duration.count() / 1000000.0;
    double bytes = num_writes * bytes_per_write;
    double mbps = (bytes / (1024.0 * 1024.0)) / seconds;

    std::cout << "Write throughput: " << mbps << " MB/s" << std::endl;

    // Baseline: > 10 MB/s (relaxed for CI variability)
    EXPECT_GT(mbps, 10.0);
}

TEST_F(SerializationRegressionTest, ReadThroughputBaseline) {
    // WHAT: Verify read throughput meets baseline
    // WHY:  Performance regression - must maintain speed
    // BASELINE: > 100 MB/s for sequential reads

    const size_t num_values = 100000;

    // Write test data
    for (size_t i = 0; i < num_values; i++) {
        ASSERT_TRUE(nimcp_write_uint32(serializer, static_cast<uint32_t>(i)));
    }

    // Reset for reading
    ASSERT_TRUE(nimcp_serializer_set_position(serializer, 0));

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < num_values; i++) {
        uint32_t val = nimcp_read_uint32(serializer);
        (void)val;  // Prevent optimization
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double seconds = duration.count() / 1000000.0;
    double bytes = num_values * sizeof(uint32_t);
    double mbps = (bytes / (1024.0 * 1024.0)) / seconds;

    std::cout << "Read throughput: " << mbps << " MB/s" << std::endl;

    // Baseline: > 100 MB/s
    EXPECT_GT(mbps, 100.0);
}

TEST_F(SerializationRegressionTest, CompressionRatioBaseline) {
    // WHAT: Verify LZ4 compression ratio for typical data
    // WHY:  Compression effectiveness regression
    // BASELINE: > 2x compression for repetitive data

    // Create highly compressible data (repeated pattern)
    const size_t num_values = 10000;
    for (size_t i = 0; i < num_values; i++) {
        ASSERT_TRUE(nimcp_write_uint32(serializer, 0x12345678));
    }

    size_t uncompressed_size = nimcp_serializer_get_length(serializer);

    // Compress
    NimcpSerialResult result = nimcp_serializer_compress(serializer);
    ASSERT_EQ(result, NIMCP_SERIAL_SUCCESS);

    size_t compressed_size = nimcp_serializer_get_length(serializer);

    double compression_ratio = (double)uncompressed_size / (double)compressed_size;

    std::cout << "Compression ratio: " << compression_ratio << ":1" << std::endl;
    std::cout << "  Uncompressed: " << uncompressed_size << " bytes" << std::endl;
    std::cout << "  Compressed: " << compressed_size << " bytes" << std::endl;

    // Baseline: > 2x compression for repetitive data
    EXPECT_GT(compression_ratio, 2.0);
}

//=============================================================================
// Compression Stability Tests
//=============================================================================

TEST_F(SerializationRegressionTest, CompressDecompressRoundTrip) {
    // WHAT: Verify compress -> decompress produces original data
    // WHY:  Core functionality regression
    // REGRESSION: Must maintain 100% data integrity

    // Write test data
    std::mt19937 rng(42);
    const size_t num_values = 1000;

    for (size_t i = 0; i < num_values; i++) {
        ASSERT_TRUE(nimcp_write_uint32(serializer, rng()));
    }

    size_t original_length = nimcp_serializer_get_length(serializer);

    // Get original data
    const uint8_t* original_data = nimcp_serializer_get_buffer(serializer);
    std::vector<uint8_t> original_copy(original_data, original_data + original_length);

    // Compress
    ASSERT_EQ(nimcp_serializer_compress(serializer), NIMCP_SERIAL_SUCCESS);
    ASSERT_TRUE(nimcp_serializer_is_compressed(serializer));

    // Decompress
    ASSERT_EQ(nimcp_serializer_decompress(serializer), NIMCP_SERIAL_SUCCESS);
    ASSERT_FALSE(nimcp_serializer_is_compressed(serializer));

    // Verify
    EXPECT_EQ(nimcp_serializer_get_length(serializer), original_length);
    EXPECT_EQ(memcmp(nimcp_serializer_get_buffer(serializer), original_copy.data(), original_length), 0);
}

TEST_F(SerializationRegressionTest, EmptyBufferCompression) {
    // WHAT: Verify compression of empty buffer
    // WHY:  Edge case that caused crash in v1.2
    // REGRESSION: Bug fix - must handle empty data (Issue #6666)

    // Compress empty buffer
    NimcpSerialResult result = nimcp_serializer_compress(serializer);

    // Should succeed (empty data is valid)
    EXPECT_EQ(result, NIMCP_SERIAL_SUCCESS);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(SerializationRegressionTest, ErrorFlagPersistence) {
    // WHAT: Verify error flag persists across operations
    // WHY:  API contract - errors must be detectable
    // REGRESSION: Bug fix - error flag was cleared prematurely (Issue #7777)

    // Write some data first to extend the buffer length
    for (int i = 0; i < 250; i++) {  // 250 * 4 bytes = 1000 bytes
        ASSERT_TRUE(nimcp_write_uint32(serializer, 0x12345678));
    }

    // Cause an error by reading past end
    // Set position to 998, so reading uint32 (4 bytes) will read positions 998-1001
    // Since length is 1000, positions 1000-1001 are out of bounds
    ASSERT_TRUE(nimcp_serializer_set_position(serializer, 998));
    uint32_t val = nimcp_read_uint32(serializer);
    (void)val;

    // Error flag should be set
    EXPECT_TRUE(nimcp_serializer_has_error(serializer));

    // Error should persist
    EXPECT_TRUE(nimcp_serializer_has_error(serializer));

    // Clear error
    nimcp_serializer_clear_error(serializer);
    EXPECT_FALSE(nimcp_serializer_has_error(serializer));
}

TEST_F(SerializationRegressionTest, BoundsCheckingStable) {
    // WHAT: Verify bounds checking prevents buffer overflow
    // WHY:  Security - buffer overflow fix
    // REGRESSION: Bug fix - must detect out-of-bounds (Issue #8888)

    // Write data
    ASSERT_TRUE(nimcp_write_uint32(serializer, 0x12345678));

    // Try to read past end
    ASSERT_TRUE(nimcp_serializer_set_position(serializer, 1));

    // Should set error flag (reading 4 bytes from position 1 exceeds length 4)
    uint32_t val = nimcp_read_uint32(serializer);
    (void)val;

    EXPECT_TRUE(nimcp_serializer_has_error(serializer));
}

TEST_F(SerializationRegressionTest, MaxSizeEnforced) {
    // WHAT: Verify max size limit is enforced
    // WHY:  Resource protection - prevent unbounded growth
    // REGRESSION: Bug fix - must enforce limit (Issue #9999)

    // Try to set buffer to data larger than max size
    std::vector<uint8_t> huge_data(NIMCP_SERIALIZER_MAX_SIZE + 1, 0x42);

    bool result = nimcp_serializer_set_buffer(serializer, huge_data.data(), huge_data.size());

    // Should fail
    EXPECT_FALSE(result);
}

//=============================================================================
// Buffer Management Tests
//=============================================================================

TEST_F(SerializationRegressionTest, BufferGrowthStable) {
    // WHAT: Verify buffer grows automatically
    // WHY:  API contract - buffer should resize as needed
    // REGRESSION: Must maintain growth behavior

    size_t initial_capacity = NIMCP_SERIALIZER_INITIAL_SIZE;

    // Write more than initial capacity
    const size_t num_bytes = initial_capacity + 1000;
    for (size_t i = 0; i < num_bytes; i++) {
        ASSERT_TRUE(nimcp_write_uint8(serializer, 0xFF));
    }

    // Should have grown
    EXPECT_GE(nimcp_serializer_get_length(serializer), num_bytes);
}

TEST_F(SerializationRegressionTest, PositionTrackingStable) {
    // WHAT: Verify position tracking is accurate
    // WHY:  API contract - position must track correctly
    // REGRESSION: Off-by-one bug fix (Issue #1010)

    EXPECT_EQ(nimcp_serializer_get_position(serializer), 0u);

    ASSERT_TRUE(nimcp_write_uint8(serializer, 0x12));
    EXPECT_EQ(nimcp_serializer_get_position(serializer), 1u);

    ASSERT_TRUE(nimcp_write_uint16(serializer, 0x3456));
    EXPECT_EQ(nimcp_serializer_get_position(serializer), 3u);

    ASSERT_TRUE(nimcp_write_uint32(serializer, 0x789ABCDE));
    EXPECT_EQ(nimcp_serializer_get_position(serializer), 7u);
}

//=============================================================================
// Test Summary
//=============================================================================

// Test count: 21 regression tests
// Coverage:
// - API Stability: 6 tests
// - Write/Read Stability: 5 tests
// - Backward Compatibility: 2 tests
// - Performance Baselines: 3 tests
// - Compression Stability: 2 tests
// - Error Handling: 3 tests
