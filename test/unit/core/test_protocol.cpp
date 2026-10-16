/**
 * @file test_protocol.cpp
 * @brief Test suite for protocol message handling
 * @details Tests serialization, deserialization, validation and checksum calculation
 */

#include <string.h>
#include "networking/protocol/nimcp_protocol.h"
#include "test_helpers.h"

class ProtocolTest : public ::testing::Test {
   protected:
    uint8_t buffer[1024];
    uint8_t payload_buffer[1024];
    msg_header_t header;
    const char* test_payload = "test payload";
};

class ProtocolSerializationTest : public ProtocolTest {};
class ProtocolDeserializationTest : public ProtocolTest {};
class ProtocolValidationTest : public ProtocolTest {};
class ProtocolChecksumTest : public ProtocolTest {};
class ProtocolIntegrationTest : public ProtocolTest {};

// Test message serialization
TEST_F(ProtocolSerializationTest, BasicSerialize)
{
    int result = protocol_serialize_message(MSG_TYPE_HELLO, test_payload, strlen(test_payload),
                                            buffer, sizeof(buffer));

    ASSERT_GT(result, 0);
    ASSERT_EQ(result, sizeof(msg_header_t) + strlen(test_payload));

    // Verify header contents
    msg_header_t* header = (msg_header_t*) buffer;
    ASSERT_EQ(header->magic, PROTOCOL_MAGIC);
    ASSERT_EQ(header->version, PROTOCOL_VERSION);
    ASSERT_EQ(header->type, MSG_TYPE_HELLO);
    ASSERT_EQ(header->length, strlen(test_payload));
}

TEST_F(ProtocolSerializationTest, BufferTooSmall)
{
    uint8_t small_buffer[4];
    int result = protocol_serialize_message(MSG_TYPE_HELLO, test_payload, strlen(test_payload),
                                            small_buffer, sizeof(small_buffer));

    ASSERT_EQ(result, -1);
}

TEST_F(ProtocolSerializationTest, NullPayload)
{
    int result = protocol_serialize_message(MSG_TYPE_HELLO, nullptr, 0, buffer, sizeof(buffer));

    ASSERT_GT(result, 0);
    ASSERT_EQ(result, sizeof(msg_header_t));
}

// Test message deserialization
TEST_F(ProtocolDeserializationTest, BasicDeserialize)
{
    // First serialize
    int written = protocol_serialize_message(MSG_TYPE_HELLO, test_payload, strlen(test_payload),
                                             buffer, sizeof(buffer));

    ASSERT_GT(written, 0);

    // Then deserialize
    int result = protocol_deserialize_message(buffer, written, &header, payload_buffer,
                                              sizeof(payload_buffer));

    ASSERT_GT(result, 0);
    ASSERT_EQ(header.type, MSG_TYPE_HELLO);
    ASSERT_EQ(header.length, strlen(test_payload));
    ASSERT_EQ(memcmp(payload_buffer, test_payload, header.length), 0);
}

TEST_F(ProtocolDeserializationTest, InvalidMagic)
{
    msg_header_t* header_ptr = (msg_header_t*) buffer;

    // Create invalid header
    header_ptr->magic = 0x12345678;  // Wrong magic
    header_ptr->version = PROTOCOL_VERSION;
    header_ptr->type = MSG_TYPE_HELLO;
    header_ptr->length = 0;

    int result = protocol_deserialize_message(buffer, sizeof(msg_header_t), &header, payload_buffer,
                                              sizeof(payload_buffer));

    ASSERT_EQ(result, -1);
}

// Test header validation
TEST_F(ProtocolValidationTest, ValidHeader)
{
    msg_header_t test_header = {.magic = PROTOCOL_MAGIC,
                                .version = PROTOCOL_VERSION,
                                .type = MSG_TYPE_HELLO,
                                .length = 10,
                                .sequence = 1,
                                .checksum = 0};

    ASSERT_TRUE(protocol_validate_header(&test_header));
}

TEST_F(ProtocolValidationTest, InvalidMagic)
{
    msg_header_t test_header = {.magic = 0x12345678,  // Wrong magic
                                .version = PROTOCOL_VERSION,
                                .type = MSG_TYPE_HELLO,
                                .length = 10,
                                .sequence = 1,
                                .checksum = 0};

    ASSERT_FALSE(protocol_validate_header(&test_header));
}

TEST_F(ProtocolValidationTest, InvalidVersion)
{
    msg_header_t test_header = {.magic = PROTOCOL_MAGIC,
                                .version = 0xFF,  // Wrong version
                                .type = MSG_TYPE_HELLO,
                                .length = 10,
                                .sequence = 1,
                                .checksum = 0};

    ASSERT_FALSE(protocol_validate_header(&test_header));
}

TEST_F(ProtocolValidationTest, InvalidMessageType)
{
    msg_header_t test_header = {.magic = PROTOCOL_MAGIC,
                                .version = PROTOCOL_VERSION,
                                .type = MSG_TYPE_MAX,  // Invalid type
                                .length = 10,
                                .sequence = 1,
                                .checksum = 0};


    ASSERT_FALSE(protocol_validate_header(&test_header));
}

TEST_F(ProtocolValidationTest, PayloadTooLarge)
{
    msg_header_t test_header = {.magic = PROTOCOL_MAGIC,
                                .version = PROTOCOL_VERSION,
                                .type = MSG_TYPE_HELLO,
                                .length = MAX_PAYLOAD_SIZE + 1,  // Too large
                                .sequence = 1,
                                .checksum = 0};

    ASSERT_FALSE(protocol_validate_header(&test_header));
}

// Test checksum calculation
TEST_F(ProtocolChecksumTest, BasicChecksum)
{
    msg_header_t test_header = {
        .magic = PROTOCOL_MAGIC,
        .version = PROTOCOL_VERSION,
        .type = MSG_TYPE_HELLO,
        .length = static_cast<uint32_t>(strlen(test_payload)),  // Add explicit cast
        //        .length = strlen(test_payload),
        .sequence = 1,
        .checksum = 0};

    uint32_t checksum =
        protocol_calculate_checksum(&test_header, test_payload, strlen(test_payload));

    ASSERT_NE(checksum, 0);

    // Calculate again - should be the same
    uint32_t checksum2 =
        protocol_calculate_checksum(&test_header, test_payload, strlen(test_payload));

    ASSERT_EQ(checksum, checksum2);
}

TEST_F(ProtocolChecksumTest, DifferentPayloads)
{
    const char* payload1 = "test payload 1";
    const char* payload2 = "test payload 2";
    msg_header_t test_header = {.magic = PROTOCOL_MAGIC,
                                .version = PROTOCOL_VERSION,
                                .type = MSG_TYPE_HELLO,
                                .length = static_cast<uint32_t>(strlen(payload1)),
                                //      .length = strlen(payload1),
                                .sequence = 1,
                                .checksum = 0};

    uint32_t checksum1 = protocol_calculate_checksum(&test_header, payload1, strlen(payload1));

    test_header.length = strlen(payload2);
    uint32_t checksum2 = protocol_calculate_checksum(&test_header, payload2, strlen(payload2));

    ASSERT_NE(checksum1, checksum2);
}

// Integration test - full message roundtrip
TEST_F(ProtocolIntegrationTest, MessageRoundtrip)
{
    const char* roundtrip_payload = "test payload for roundtrip";

    // Serialize message
    int written = protocol_serialize_message(MSG_TYPE_HELLO, roundtrip_payload,
                                             strlen(roundtrip_payload), buffer, sizeof(buffer));

    ASSERT_GT(written, 0);

    // Deserialize message
    int read = protocol_deserialize_message(buffer, written, &header, payload_buffer,
                                            sizeof(payload_buffer));

    ASSERT_EQ(read, written);
    ASSERT_EQ(header.type, MSG_TYPE_HELLO);
    ASSERT_EQ(header.length, strlen(roundtrip_payload));
    ASSERT_EQ(memcmp(payload_buffer, roundtrip_payload, header.length), 0);
}
