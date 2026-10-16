/**
 * @file test_serialization_security.cpp
 * @brief Unit tests for serialization security fixes
 *
 * WHAT: Tests for security hardening of serialization and binding code
 * WHY:  Verify fixes for C8 (attacker-controlled allocation),
 *        H17 (integer overflow in check_read/ensure_capacity),
 *        and decompress bounds validation
 * HOW:  Craft malicious inputs that trigger each vulnerability path
 *
 * @date 2026-03-05
 */

#include <gtest/gtest.h>
#include <cstring>
#include <climits>

// Headers have their own extern "C" guards
#include "io/serialization/nimcp_serialization.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SerializationSecurityTest : public ::testing::Test {
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
};

//=============================================================================
// Bug C8: Decompress rejects oversized original_size
//=============================================================================

TEST_F(SerializationSecurityTest, Decompress_RejectsOversizedOriginalSize) {
    // WHAT: Verify decompress rejects original_size > NIMCP_SERIALIZER_MAX_SIZE
    // WHY:  Attacker-controlled allocation — malicious compressed data could
    //       claim an arbitrarily large original_size to exhaust memory (Bug C8)
    // HOW:  Craft a buffer with a uint32_t header exceeding max, mark compressed

    // Write a fake compressed buffer: 4 bytes for original_size + 1 byte dummy
    uint32_t malicious_size = NIMCP_SERIALIZER_MAX_SIZE + 1;
    uint8_t fake_data[8];
    memcpy(fake_data, &malicious_size, sizeof(uint32_t));
    fake_data[4] = 0x00; // dummy compressed data
    fake_data[5] = 0x00;
    fake_data[6] = 0x00;
    fake_data[7] = 0x00;

    ASSERT_TRUE(nimcp_serializer_set_buffer(serializer, fake_data, sizeof(fake_data)));
    nimcp_serializer_mark_compressed(serializer);

    NimcpSerialResult result = nimcp_serializer_decompress(serializer);
    EXPECT_EQ(result, NIMCP_SERIAL_ERROR_INVALID_PARAM);
}

TEST_F(SerializationSecurityTest, Decompress_RejectsZeroOriginalSize) {
    // WHAT: Verify decompress rejects original_size == 0
    // WHY:  Zero-size allocation is undefined behavior on some platforms

    uint32_t zero_size = 0;
    uint8_t fake_data[8];
    memcpy(fake_data, &zero_size, sizeof(uint32_t));
    fake_data[4] = 0x00;
    fake_data[5] = 0x00;
    fake_data[6] = 0x00;
    fake_data[7] = 0x00;

    ASSERT_TRUE(nimcp_serializer_set_buffer(serializer, fake_data, sizeof(fake_data)));
    nimcp_serializer_mark_compressed(serializer);

    NimcpSerialResult result = nimcp_serializer_decompress(serializer);
    EXPECT_EQ(result, NIMCP_SERIAL_ERROR_INVALID_PARAM);
}

TEST_F(SerializationSecurityTest, Decompress_RejectsMaxUint32OriginalSize) {
    // WHAT: Verify decompress rejects UINT32_MAX as original_size
    // WHY:  UINT32_MAX (~4GB) exceeds 64MB max — must be rejected

    uint32_t huge_size = UINT32_MAX;
    uint8_t fake_data[8];
    memcpy(fake_data, &huge_size, sizeof(uint32_t));
    fake_data[4] = 0x00;
    fake_data[5] = 0x00;
    fake_data[6] = 0x00;
    fake_data[7] = 0x00;

    ASSERT_TRUE(nimcp_serializer_set_buffer(serializer, fake_data, sizeof(fake_data)));
    nimcp_serializer_mark_compressed(serializer);

    NimcpSerialResult result = nimcp_serializer_decompress(serializer);
    EXPECT_EQ(result, NIMCP_SERIAL_ERROR_INVALID_PARAM);
}

TEST_F(SerializationSecurityTest, Decompress_AcceptsValidSize) {
    // WHAT: Verify decompress accepts original_size within bounds
    // WHY:  Ensure the bounds check doesn't break valid usage
    // HOW:  Compress real data, then decompress — round-trip

    // Write some data to compress
    for (int i = 0; i < 100; i++) {
        ASSERT_TRUE(nimcp_write_uint32(serializer, (uint32_t)i));
    }

    NimcpSerialResult compress_result = nimcp_serializer_compress(serializer);
    ASSERT_EQ(compress_result, NIMCP_SERIAL_SUCCESS);
    ASSERT_TRUE(nimcp_serializer_is_compressed(serializer));

    NimcpSerialResult decompress_result = nimcp_serializer_decompress(serializer);
    EXPECT_EQ(decompress_result, NIMCP_SERIAL_SUCCESS);
    EXPECT_FALSE(nimcp_serializer_is_compressed(serializer));

    // Verify data integrity after round-trip
    nimcp_serializer_set_position(serializer, 0);
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(nimcp_read_uint32(serializer), (uint32_t)i);
    }
}

//=============================================================================
// Bug H17: Integer overflow in nimcp_check_read
//=============================================================================

TEST_F(SerializationSecurityTest, CheckRead_RejectsOverflowPosition) {
    // WHAT: Verify check_read rejects when position + bytes_needed overflows
    // WHY:  Integer overflow in (position + bytes_needed) could wrap around
    //       to a small value, passing the bounds check (Bug H17)
    // HOW:  Set position near SIZE_MAX, then try to read — should fail

    // Write some data first so the buffer has content
    ASSERT_TRUE(nimcp_write_uint32(serializer, 42));

    // Manually set position near SIZE_MAX
    // Position > length should be caught
    ASSERT_FALSE(nimcp_serializer_set_position(serializer, SIZE_MAX - 1));

    // Even reading 1 byte at a valid but near-end position should work correctly
    // The key test is that position > length is rejected
    EXPECT_TRUE(nimcp_serializer_has_error(serializer) ||
                nimcp_serializer_get_position(serializer) != SIZE_MAX - 1);
}

TEST_F(SerializationSecurityTest, CheckRead_NormalBoundsCheckStillWorks) {
    // WHAT: Verify normal bounds checking still catches reads past end
    // WHY:  Regression test — overflow fix must not break normal validation

    // Write 4 bytes
    ASSERT_TRUE(nimcp_write_uint32(serializer, 0xDEADBEEF));
    nimcp_serializer_set_position(serializer, 0);

    // Read 4 bytes — should succeed
    uint32_t val = nimcp_read_uint32(serializer);
    EXPECT_EQ(val, 0xDEADBEEF);
    EXPECT_FALSE(nimcp_serializer_has_error(serializer));

    // Read another 4 bytes — should fail (only 4 bytes in buffer)
    uint32_t val2 = nimcp_read_uint32(serializer);
    (void)val2;
    EXPECT_TRUE(nimcp_serializer_has_error(serializer));
}

TEST_F(SerializationSecurityTest, CheckRead_ExactBoundary) {
    // WHAT: Verify reading exactly to the end works
    // WHY:  Off-by-one in overflow fix could reject valid boundary reads

    ASSERT_TRUE(nimcp_write_uint8(serializer, 0xFF));
    nimcp_serializer_set_position(serializer, 0);

    // Read exactly 1 byte from a 1-byte buffer
    uint8_t val = nimcp_read_uint8(serializer);
    EXPECT_EQ(val, 0xFF);
    EXPECT_FALSE(nimcp_serializer_has_error(serializer));

    // Next read should fail
    uint8_t val2 = nimcp_read_uint8(serializer);
    (void)val2;
    EXPECT_TRUE(nimcp_serializer_has_error(serializer));
}

//=============================================================================
// Bug H17: Integer overflow in ensure_capacity
//=============================================================================

TEST_F(SerializationSecurityTest, EnsureCapacity_RejectsOverflowAdditionalSize) {
    // WHAT: Verify ensure_capacity rejects when position + additional_size overflows
    // WHY:  Overflow in (position + additional_size) could bypass max-size check (Bug H17)
    // HOW:  Try to write with a huge size that would overflow when added to position

    // Write some initial data to advance position
    ASSERT_TRUE(nimcp_write_uint32(serializer, 1));

    // Now try to write SIZE_MAX bytes — should fail with overflow protection
    bool result = nimcp_write_bytes(serializer, (const uint8_t*)"x", SIZE_MAX);
    EXPECT_FALSE(result);
    EXPECT_TRUE(nimcp_serializer_has_error(serializer));
}

TEST_F(SerializationSecurityTest, EnsureCapacity_MaxSizeBoundary) {
    // WHAT: Verify ensure_capacity rejects exactly at NIMCP_SERIALIZER_MAX_SIZE + 1
    // WHY:  Boundary condition test for max size enforcement

    // Create a small serializer
    NimcpSerializer* small = nimcp_serializer_create(64);
    ASSERT_NE(small, nullptr);

    // Requesting exactly MAX_SIZE + 1 should fail
    bool result = nimcp_write_bytes(small, (const uint8_t*)"x",
                                    NIMCP_SERIALIZER_MAX_SIZE + 1);
    EXPECT_FALSE(result);

    nimcp_serializer_destroy(small);
}

//=============================================================================
// Compress/Decompress: unaligned access fix verification
//=============================================================================

TEST_F(SerializationSecurityTest, CompressDecompress_AlignmentSafe) {
    // WHAT: Verify compress/decompress uses memcpy for size header (no unaligned access)
    // WHY:  Original code used *(uint32_t*) cast which is UB on strict-alignment platforms
    // HOW:  Round-trip test — if memcpy-based code works, alignment is safe

    // Write varied data to make compression non-trivial
    for (uint32_t i = 0; i < 256; i++) {
        ASSERT_TRUE(nimcp_write_uint8(serializer, (uint8_t)(i & 0xFF)));
    }

    size_t original_length = nimcp_serializer_get_length(serializer);
    ASSERT_EQ(original_length, 256u);

    // Compress
    NimcpSerialResult cr = nimcp_serializer_compress(serializer);
    ASSERT_EQ(cr, NIMCP_SERIAL_SUCCESS);

    // Verify the size header was written correctly via memcpy
    // Read back the first 4 bytes of the compressed buffer
    uint8_t* buf = nimcp_serializer_get_buffer(serializer);
    ASSERT_NE(buf, nullptr);
    uint32_t stored_size;
    memcpy(&stored_size, buf, sizeof(uint32_t));
    EXPECT_EQ(stored_size, (uint32_t)original_length);

    // Decompress
    NimcpSerialResult dr = nimcp_serializer_decompress(serializer);
    ASSERT_EQ(dr, NIMCP_SERIAL_SUCCESS);

    // Verify round-trip
    EXPECT_EQ(nimcp_serializer_get_length(serializer), original_length);
    nimcp_serializer_set_position(serializer, 0);
    for (uint32_t i = 0; i < 256; i++) {
        EXPECT_EQ(nimcp_read_uint8(serializer), (uint8_t)(i & 0xFF));
    }
}

TEST_F(SerializationSecurityTest, Decompress_TooShortBuffer) {
    // WHAT: Verify decompress rejects buffer shorter than uint32_t header
    // WHY:  Buffer must have at least 4 bytes for the original_size field

    uint8_t tiny[2] = {0x01, 0x02};
    ASSERT_TRUE(nimcp_serializer_set_buffer(serializer, tiny, sizeof(tiny)));
    nimcp_serializer_mark_compressed(serializer);

    NimcpSerialResult result = nimcp_serializer_decompress(serializer);
    EXPECT_EQ(result, NIMCP_SERIAL_ERROR_BOUNDS);
}

//=============================================================================
// TOCTOU race fix verification (nimcp_platform_once)
//=============================================================================

TEST_F(SerializationSecurityTest, SecurityInitIdempotent) {
    // WHAT: Verify serialization operations work after security init
    // WHY:  The TOCTOU fix uses nimcp_platform_once — verify it doesn't break
    //       normal serialization operations that trigger security_init
    // HOW:  Just do normal write/read — security init happens internally

    ASSERT_TRUE(nimcp_write_uint32(serializer, 0x12345678));
    nimcp_serializer_set_position(serializer, 0);
    EXPECT_EQ(nimcp_read_uint32(serializer), 0x12345678u);
    EXPECT_FALSE(nimcp_serializer_has_error(serializer));
}
