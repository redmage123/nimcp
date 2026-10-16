/**
 * @file test_nlp_header.cpp
 * @brief Unit tests for Neural Link Protocol header operations
 *
 * WHAT: Tests for NLP header serialization, field accessors, CRC, byte order
 * WHY:  Ensure header structure integrity and correct field manipulation
 * HOW:  Use GoogleTest framework with comprehensive validation
 *
 * TEST COVERAGE: 12 tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <string.h>
#include <cstdint>
#include <arpa/inet.h>

// Headers have their own extern "C" guards
#include "networking/nlp/nimcp_neural_link_protocol.h"

//=============================================================================
// Test Constants
//=============================================================================

static const uint32_t TEST_SENDER_ID = 0x12345678;
static const uint32_t TEST_DEST_ID = 0x87654321;
static const uint32_t TEST_TIMESTAMP = 1701234567;
static const uint16_t TEST_SEQUENCE = 42;

//=============================================================================
// Test Fixture
//=============================================================================

class NLPHeaderTest : public ::testing::Test {
protected:
    nlp_header_t header;

    void SetUp() override
    {
        memset(&header, 0, sizeof(nlp_header_t));
    }

    void TearDown() override
    {
        // Nothing to clean up
    }

    /**
     * WHAT: Create a populated test header
     * WHY:  Provide consistent test data
     */
    void populate_test_header()
    {
        NLP_SET_VERSION(&header, NLP_VERSION);
        NLP_SET_MODE(&header, NLP_MODE_STANDARD);
        NLP_SET_PRIORITY(&header, NLP_PRIORITY_NORMAL);
        NLP_SET_KEY_SLOT(&header, 3);
        NLP_SET_FLAGS(&header, NLP_FLAG_ENCRYPTED | NLP_FLAG_ACK_REQUIRED);

        header.msg_type = htons(NLP_MSG_SPIKE_BATCH);
        header.sender_id = htonl(TEST_SENDER_ID);
        header.timestamp = htonl(TEST_TIMESTAMP);
        header.sequence = htons(TEST_SEQUENCE);
        header.ack_sequence = htons(TEST_SEQUENCE - 1);
        header.dest_id = htonl(TEST_DEST_ID);
        header.payload_len = htons(1024);

        // Generate unique nonce
        for (int i = 0; i < NLP_NONCE_SIZE; i++) {
            header.nonce[i] = static_cast<uint8_t>(i);
        }

        // Calculate CRC (set to 0 first)
        header.header_crc = 0;
        header.header_crc = nlp_header_crc(&header);
    }
};

//=============================================================================
// Header Size Tests
//=============================================================================

/**
 * WHAT: Verify header size is exactly 36 bytes
 * WHY:  Protocol requires fixed-size header for parsing
 */
TEST_F(NLPHeaderTest, HeaderSizeConstant)
{
    EXPECT_EQ(sizeof(nlp_header_t), NLP_HEADER_SIZE);
    EXPECT_EQ(sizeof(nlp_header_t), 36);
}

/**
 * WHAT: Verify header is packed (no padding)
 * WHY:  Ensure consistent serialization across platforms
 */
TEST_F(NLPHeaderTest, HeaderIsPacked)
{
    // Calculate expected size manually
    size_t expected_size =
        1 +  // version_mode_priority
        1 +  // key_slot_flags
        2 +  // msg_type
        4 +  // sender_id
        4 +  // timestamp
        12 + // nonce
        2 +  // sequence
        2 +  // ack_sequence
        4 +  // dest_id
        2 +  // payload_len
        2;   // header_crc

    EXPECT_EQ(sizeof(nlp_header_t), expected_size);
    EXPECT_EQ(sizeof(nlp_header_t), 36);
}

//=============================================================================
// Field Accessor Tests
//=============================================================================

/**
 * WHAT: Test version field accessor macros
 * WHY:  Verify version can be set and retrieved correctly
 */
TEST_F(NLPHeaderTest, HeaderFieldAccessors_Version)
{
    NLP_SET_VERSION(&header, 1);
    EXPECT_EQ(NLP_GET_VERSION(&header), 1);

    NLP_SET_VERSION(&header, 15); // Max 4 bits
    EXPECT_EQ(NLP_GET_VERSION(&header), 15);

    NLP_SET_VERSION(&header, 0);
    EXPECT_EQ(NLP_GET_VERSION(&header), 0);
}

/**
 * WHAT: Test mode field accessor macros
 * WHY:  Verify operating mode can be set and retrieved
 */
TEST_F(NLPHeaderTest, HeaderFieldAccessors_Mode)
{
    NLP_SET_MODE(&header, NLP_MODE_STANDARD);
    EXPECT_EQ(NLP_GET_MODE(&header), NLP_MODE_STANDARD);

    NLP_SET_MODE(&header, NLP_MODE_TACTICAL);
    EXPECT_EQ(NLP_GET_MODE(&header), NLP_MODE_TACTICAL);

    NLP_SET_MODE(&header, NLP_MODE_STEALTH);
    EXPECT_EQ(NLP_GET_MODE(&header), NLP_MODE_STEALTH);
}

/**
 * WHAT: Test priority field accessor macros
 * WHY:  Verify message priority can be set and retrieved
 */
TEST_F(NLPHeaderTest, HeaderFieldAccessors_Priority)
{
    NLP_SET_PRIORITY(&header, NLP_PRIORITY_LOW);
    EXPECT_EQ(NLP_GET_PRIORITY(&header), NLP_PRIORITY_LOW);

    NLP_SET_PRIORITY(&header, NLP_PRIORITY_CRITICAL);
    EXPECT_EQ(NLP_GET_PRIORITY(&header), NLP_PRIORITY_CRITICAL);
}

/**
 * WHAT: Test key slot field accessor macros
 * WHY:  Verify key slot selection works correctly
 */
TEST_F(NLPHeaderTest, HeaderFieldAccessors_KeySlot)
{
    NLP_SET_KEY_SLOT(&header, 0);
    EXPECT_EQ(NLP_GET_KEY_SLOT(&header), 0);

    NLP_SET_KEY_SLOT(&header, 7); // Max 3 bits
    EXPECT_EQ(NLP_GET_KEY_SLOT(&header), 7);

    NLP_SET_KEY_SLOT(&header, 3);
    EXPECT_EQ(NLP_GET_KEY_SLOT(&header), 3);
}

/**
 * WHAT: Test flags field accessor macros
 * WHY:  Verify message flags can be set and retrieved
 */
TEST_F(NLPHeaderTest, HeaderFieldAccessors_Flags)
{
    NLP_SET_FLAGS(&header, 0);
    EXPECT_EQ(NLP_GET_FLAGS(&header), 0);

    NLP_SET_FLAGS(&header, NLP_FLAG_ENCRYPTED);
    EXPECT_EQ(NLP_GET_FLAGS(&header), NLP_FLAG_ENCRYPTED);

    NLP_SET_FLAGS(&header, NLP_FLAG_ENCRYPTED | NLP_FLAG_COMPRESSED);
    EXPECT_EQ(NLP_GET_FLAGS(&header), NLP_FLAG_ENCRYPTED | NLP_FLAG_COMPRESSED);

    NLP_SET_FLAGS(&header, 0x1F); // All 5 bits set
    EXPECT_EQ(NLP_GET_FLAGS(&header), 0x1F);
}

/**
 * WHAT: Test field independence
 * WHY:  Verify setting one field doesn't affect others
 */
TEST_F(NLPHeaderTest, HeaderFieldAccessors_Independence)
{
    NLP_SET_VERSION(&header, 1);
    NLP_SET_MODE(&header, NLP_MODE_TACTICAL);
    NLP_SET_PRIORITY(&header, NLP_PRIORITY_HIGH);

    EXPECT_EQ(NLP_GET_VERSION(&header), 1);
    EXPECT_EQ(NLP_GET_MODE(&header), NLP_MODE_TACTICAL);
    EXPECT_EQ(NLP_GET_PRIORITY(&header), NLP_PRIORITY_HIGH);

    // Change one field, verify others unchanged
    NLP_SET_MODE(&header, NLP_MODE_STEALTH);

    EXPECT_EQ(NLP_GET_VERSION(&header), 1);
    EXPECT_EQ(NLP_GET_MODE(&header), NLP_MODE_STEALTH);
    EXPECT_EQ(NLP_GET_PRIORITY(&header), NLP_PRIORITY_HIGH);
}

//=============================================================================
// Network Byte Order Tests
//=============================================================================

/**
 * WHAT: Test network byte order conversion for multi-byte fields
 * WHY:  Ensure cross-platform compatibility
 */
TEST_F(NLPHeaderTest, HeaderNetworkByteOrder)
{
    populate_test_header();

    // Convert from network to host byte order
    uint16_t msg_type = ntohs(header.msg_type);
    uint32_t sender_id = ntohl(header.sender_id);
    uint32_t timestamp = ntohl(header.timestamp);
    uint16_t sequence = ntohs(header.sequence);
    uint32_t dest_id = ntohl(header.dest_id);
    uint16_t payload_len = ntohs(header.payload_len);

    // Verify values match original
    EXPECT_EQ(msg_type, NLP_MSG_SPIKE_BATCH);
    EXPECT_EQ(sender_id, TEST_SENDER_ID);
    EXPECT_EQ(timestamp, TEST_TIMESTAMP);
    EXPECT_EQ(sequence, TEST_SEQUENCE);
    EXPECT_EQ(dest_id, TEST_DEST_ID);
    EXPECT_EQ(payload_len, 1024);
}

//=============================================================================
// CRC Tests
//=============================================================================

/**
 * WHAT: Test CRC-16 calculation
 * WHY:  Verify header integrity can be validated
 */
TEST_F(NLPHeaderTest, HeaderCRC_Calculation)
{
    populate_test_header();

    // Save original CRC
    uint16_t original_crc = header.header_crc;

    // Recalculate CRC
    header.header_crc = 0;
    uint16_t calculated_crc = nlp_header_crc(&header);

    // Verify match
    EXPECT_EQ(calculated_crc, original_crc);
    EXPECT_NE(calculated_crc, 0); // CRC shouldn't be zero for valid data
}

/**
 * WHAT: Test CRC detects corruption
 * WHY:  Verify integrity checking works
 */
TEST_F(NLPHeaderTest, HeaderCRC_DetectsCorruption)
{
    populate_test_header();

    uint16_t original_crc = header.header_crc;

    // Corrupt a field
    header.sender_id ^= htonl(0x00000001);

    // Recalculate CRC
    header.header_crc = 0;
    uint16_t new_crc = nlp_header_crc(&header);

    // CRC should be different
    EXPECT_NE(new_crc, original_crc);
}

/**
 * WHAT: Test CRC validation function
 * WHY:  Ensure we can validate received headers
 */
TEST_F(NLPHeaderTest, HeaderCRC_Validation)
{
    populate_test_header();

    // Store correct CRC
    uint16_t correct_crc = header.header_crc;

    // Validate with correct CRC
    header.header_crc = 0;
    uint16_t calculated = nlp_header_crc(&header);
    header.header_crc = correct_crc;

    EXPECT_EQ(calculated, correct_crc);

    // Validate with incorrect CRC
    header.header_crc = correct_crc ^ 0xFFFF;
    EXPECT_NE(nlp_header_crc(&header), header.header_crc);
}

//=============================================================================
// Serialization Tests
//=============================================================================

/**
 * WHAT: Test header serialization/deserialization round trip
 * WHY:  Verify headers can be sent over network
 */
TEST_F(NLPHeaderTest, HeaderSerializeDeserialize)
{
    // Create and populate original header
    nlp_header_t original;
    memset(&original, 0, sizeof(nlp_header_t));

    NLP_SET_VERSION(&original, NLP_VERSION);
    NLP_SET_MODE(&original, NLP_MODE_TACTICAL);
    NLP_SET_PRIORITY(&original, NLP_PRIORITY_HIGH);
    NLP_SET_KEY_SLOT(&original, 5);
    NLP_SET_FLAGS(&original, NLP_FLAG_ENCRYPTED | NLP_FLAG_ACK_REQUIRED);

    original.msg_type = htons(NLP_MSG_HEARTBEAT);
    original.sender_id = htonl(0xDEADBEEF);
    original.timestamp = htonl(1234567890);
    original.sequence = htons(999);
    original.ack_sequence = htons(998);
    original.dest_id = htonl(0xCAFEBABE);
    original.payload_len = htons(512);

    for (int i = 0; i < NLP_NONCE_SIZE; i++) {
        original.nonce[i] = static_cast<uint8_t>(i * 2);
    }

    original.header_crc = 0;
    original.header_crc = nlp_header_crc(&original);

    // Serialize to bytes
    uint8_t serialized[NLP_HEADER_SIZE];
    memcpy(serialized, &original, NLP_HEADER_SIZE);

    // Deserialize back
    nlp_header_t deserialized;
    memcpy(&deserialized, serialized, NLP_HEADER_SIZE);

    // Verify all fields match
    EXPECT_EQ(NLP_GET_VERSION(&deserialized), NLP_VERSION);
    EXPECT_EQ(NLP_GET_MODE(&deserialized), NLP_MODE_TACTICAL);
    EXPECT_EQ(NLP_GET_PRIORITY(&deserialized), NLP_PRIORITY_HIGH);
    EXPECT_EQ(NLP_GET_KEY_SLOT(&deserialized), 5);
    EXPECT_EQ(NLP_GET_FLAGS(&deserialized), NLP_FLAG_ENCRYPTED | NLP_FLAG_ACK_REQUIRED);

    EXPECT_EQ(ntohs(deserialized.msg_type), NLP_MSG_HEARTBEAT);
    EXPECT_EQ(ntohl(deserialized.sender_id), 0xDEADBEEF);
    EXPECT_EQ(ntohl(deserialized.timestamp), 1234567890);
    EXPECT_EQ(ntohs(deserialized.sequence), 999);
    EXPECT_EQ(ntohs(deserialized.ack_sequence), 998);
    EXPECT_EQ(ntohl(deserialized.dest_id), 0xCAFEBABE);
    EXPECT_EQ(ntohs(deserialized.payload_len), 512);
    EXPECT_EQ(deserialized.header_crc, original.header_crc);

    // Verify nonce
    EXPECT_EQ(memcmp(deserialized.nonce, original.nonce, NLP_NONCE_SIZE), 0);
}

/**
 * WHAT: Test header with all fields at extremes
 * WHY:  Verify boundary conditions
 */
TEST_F(NLPHeaderTest, HeaderExtremeValues)
{
    NLP_SET_VERSION(&header, 15);  // Max 4 bits
    NLP_SET_MODE(&header, 3);      // Max 2 bits
    NLP_SET_PRIORITY(&header, 3);  // Max 2 bits
    NLP_SET_KEY_SLOT(&header, 7);  // Max 3 bits
    NLP_SET_FLAGS(&header, 0x1F);  // Max 5 bits

    header.msg_type = htons(0xFFFF);
    header.sender_id = htonl(0xFFFFFFFF);
    header.timestamp = htonl(0xFFFFFFFF);
    header.sequence = htons(0xFFFF);
    header.ack_sequence = htons(0xFFFF);
    header.dest_id = htonl(0xFFFFFFFF);
    header.payload_len = htons(0xFFFF);

    memset(header.nonce, 0xFF, NLP_NONCE_SIZE);

    // Calculate CRC for extreme values
    header.header_crc = 0;
    header.header_crc = nlp_header_crc(&header);

    // Verify all fields
    EXPECT_EQ(NLP_GET_VERSION(&header), 15);
    EXPECT_EQ(NLP_GET_MODE(&header), 3);
    EXPECT_EQ(NLP_GET_PRIORITY(&header), 3);
    EXPECT_EQ(NLP_GET_KEY_SLOT(&header), 7);
    EXPECT_EQ(NLP_GET_FLAGS(&header), 0x1F);
}
