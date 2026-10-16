/**
 * @file test_nimcp_msg_framing.cpp
 * @brief Unit tests for message framing protocol
 *
 * Tests cover:
 * - Header initialization and validation
 * - Fast message creation and serialization
 * - Deserialization and round-trip consistency
 * - Edge cases and error handling
 * - Statistics tracking
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "networking/protocol/nimcp_msg_framing.h"

/*=============================================================================
 * Test Fixtures
 *===========================================================================*/

class MsgFramingTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_msg_reset_stats();
    }

    void TearDown() override {
        // Clean up
    }

    // Helper to compare floats with tolerance
    bool floatEqual(float a, float b, float epsilon = 0.0001f) {
        return std::fabs(a - b) < epsilon;
    }
};

/*=============================================================================
 * Header Tests
 *===========================================================================*/

TEST_F(MsgFramingTest, HeaderInit_ValidParams_InitializesCorrectly) {
    nimcp_msg_header_t header;

    nimcp_msg_header_init(&header, MSG_TYPE_HEARTBEAT, MSG_FLAG_BROADCAST, 16);

    EXPECT_EQ(header.magic[0], NIMCP_MSG_MAGIC_0);
    EXPECT_EQ(header.magic[1], NIMCP_MSG_MAGIC_1);
    EXPECT_EQ(header.version, NIMCP_MSG_VERSION);
    EXPECT_EQ(header.flags, MSG_FLAG_BROADCAST);
    EXPECT_EQ(header.msg_type, MSG_TYPE_HEARTBEAT);
    EXPECT_EQ(header.payload_len, 16);
}

TEST_F(MsgFramingTest, HeaderInit_NullHeader_DoesNotCrash) {
    // Should not crash with null pointer
    nimcp_msg_header_init(nullptr, MSG_TYPE_HEARTBEAT, 0, 16);
    SUCCEED();
}

TEST_F(MsgFramingTest, HeaderValidate_ValidHeader_ReturnsTrue) {
    nimcp_msg_header_t header;
    nimcp_msg_header_init(&header, MSG_TYPE_HEARTBEAT, 0, NIMCP_MSG_FAST_PAYLOAD);

    EXPECT_TRUE(nimcp_msg_header_validate(&header));
}

TEST_F(MsgFramingTest, HeaderValidate_InvalidMagic_ReturnsFalse) {
    nimcp_msg_header_t header;
    nimcp_msg_header_init(&header, MSG_TYPE_HEARTBEAT, 0, NIMCP_MSG_FAST_PAYLOAD);

    header.magic[0] = 0x00;  // Corrupt magic
    EXPECT_FALSE(nimcp_msg_header_validate(&header));
}

TEST_F(MsgFramingTest, HeaderValidate_InvalidVersion_ReturnsFalse) {
    nimcp_msg_header_t header;
    nimcp_msg_header_init(&header, MSG_TYPE_HEARTBEAT, 0, NIMCP_MSG_FAST_PAYLOAD);

    header.version = 99;  // Invalid version
    EXPECT_FALSE(nimcp_msg_header_validate(&header));
}

TEST_F(MsgFramingTest, HeaderValidate_ExcessivePayloadLen_WrapsUint16) {
    nimcp_msg_header_t header;
    nimcp_msg_header_init(&header, MSG_TYPE_NEURAL_STATE, MSG_FLAG_PROTOBUF, 0);

    // NIMCP_MSG_MAX_PAYLOAD is 65535 (uint16_t max), so +1 wraps to 0
    // The uint16_t field cannot hold a value exceeding the max payload size
    header.payload_len = NIMCP_MSG_MAX_PAYLOAD + 1;
    EXPECT_TRUE(nimcp_msg_header_validate(&header));  // Wraps to 0, which is valid
}

TEST_F(MsgFramingTest, HeaderValidate_FastPathWrongPayloadLen_ReturnsFalse) {
    nimcp_msg_header_t header;
    nimcp_msg_header_init(&header, MSG_TYPE_HEARTBEAT, 0, 32);  // Should be 16

    EXPECT_FALSE(nimcp_msg_header_validate(&header));
}

TEST_F(MsgFramingTest, HeaderValidate_NullHeader_ReturnsFalse) {
    EXPECT_FALSE(nimcp_msg_header_validate(nullptr));
}

/*=============================================================================
 * Flag Helper Tests
 *===========================================================================*/

TEST_F(MsgFramingTest, IsFastPath_NoProtobufFlag_ReturnsTrue) {
    nimcp_msg_header_t header;
    nimcp_msg_header_init(&header, MSG_TYPE_HEARTBEAT, 0, NIMCP_MSG_FAST_PAYLOAD);

    EXPECT_TRUE(nimcp_msg_is_fast_path(&header));
    EXPECT_FALSE(nimcp_msg_is_protobuf(&header));
}

TEST_F(MsgFramingTest, IsProtobuf_ProtobufFlag_ReturnsTrue) {
    nimcp_msg_header_t header;
    nimcp_msg_header_init(&header, MSG_TYPE_NEURAL_STATE, MSG_FLAG_PROTOBUF, 100);

    EXPECT_TRUE(nimcp_msg_is_protobuf(&header));
    EXPECT_FALSE(nimcp_msg_is_fast_path(&header));
}

TEST_F(MsgFramingTest, IsCompressed_CompressedFlag_ReturnsTrue) {
    nimcp_msg_header_t header;
    nimcp_msg_header_init(&header, MSG_TYPE_NEURAL_STATE, MSG_FLAG_PROTOBUF | MSG_FLAG_COMPRESSED, 100);

    EXPECT_TRUE(nimcp_msg_is_compressed(&header));
}

TEST_F(MsgFramingTest, IsEncrypted_EncryptedFlag_ReturnsTrue) {
    nimcp_msg_header_t header;
    nimcp_msg_header_init(&header, MSG_TYPE_NEURAL_STATE, MSG_FLAG_PROTOBUF | MSG_FLAG_ENCRYPTED, 100);

    EXPECT_TRUE(nimcp_msg_is_encrypted(&header));
}

TEST_F(MsgFramingTest, IsReliable_ReliableFlag_ReturnsTrue) {
    nimcp_msg_header_t header;
    nimcp_msg_header_init(&header, MSG_TYPE_SHARED_GOAL, MSG_FLAG_PROTOBUF | MSG_FLAG_RELIABLE, 100);

    EXPECT_TRUE(nimcp_msg_is_reliable(&header));
}

TEST_F(MsgFramingTest, IsBroadcast_BroadcastFlag_ReturnsTrue) {
    nimcp_msg_header_t header;
    nimcp_msg_header_init(&header, MSG_TYPE_DANGER, MSG_FLAG_BROADCAST, NIMCP_MSG_FAST_PAYLOAD);

    EXPECT_TRUE(nimcp_msg_is_broadcast(&header));
}

/*=============================================================================
 * Fast Message Tests
 *===========================================================================*/

TEST_F(MsgFramingTest, FastMsgInit_ValidParams_InitializesCorrectly) {
    nimcp_fast_msg_t msg;

    nimcp_fast_msg_init(&msg, MSG_TYPE_SYNC, MSG_FLAG_BROADCAST);

    EXPECT_EQ(msg.header.msg_type, MSG_TYPE_SYNC);
    EXPECT_EQ(msg.header.flags, MSG_FLAG_BROADCAST);  // PROTOBUF flag cleared
    EXPECT_EQ(msg.header.payload_len, NIMCP_MSG_FAST_PAYLOAD);
    EXPECT_TRUE(nimcp_msg_is_fast_path(&msg.header));
}

TEST_F(MsgFramingTest, FastMsgInit_ProtobufFlagCleared_ReturnsFastPath) {
    nimcp_fast_msg_t msg;

    // Try to set PROTOBUF flag - should be cleared
    nimcp_fast_msg_init(&msg, MSG_TYPE_HEARTBEAT, MSG_FLAG_PROTOBUF | MSG_FLAG_BROADCAST);

    EXPECT_FALSE(msg.header.flags & MSG_FLAG_PROTOBUF);
    EXPECT_TRUE(nimcp_msg_is_fast_path(&msg.header));
}

TEST_F(MsgFramingTest, FastMsgHeartbeat_ValidParams_PacksCorrectly) {
    nimcp_fast_msg_t msg;
    uint32_t brain_id = 0x12345678;
    float atp_level = 0.75f;
    float load = 0.5f;

    nimcp_fast_msg_heartbeat(&msg, brain_id, atp_level, load);

    EXPECT_EQ(msg.header.msg_type, MSG_TYPE_HEARTBEAT);

    // Verify brain_id in payload (big-endian)
    uint32_t extracted_id = ((uint32_t)msg.payload[0] << 24) |
                            ((uint32_t)msg.payload[1] << 16) |
                            ((uint32_t)msg.payload[2] << 8) |
                            msg.payload[3];
    EXPECT_EQ(extracted_id, brain_id);
}

TEST_F(MsgFramingTest, FastMsgSync_ValidParams_PacksCorrectly) {
    nimcp_fast_msg_t msg;
    uint32_t brain_id = 0xDEADBEEF;
    float gamma_power = 0.8f;
    float gamma_phase = 3.14159f;

    nimcp_fast_msg_sync(&msg, brain_id, gamma_power, gamma_phase);

    EXPECT_EQ(msg.header.msg_type, MSG_TYPE_SYNC);

    // Verify brain_id
    uint32_t extracted_id = ((uint32_t)msg.payload[0] << 24) |
                            ((uint32_t)msg.payload[1] << 16) |
                            ((uint32_t)msg.payload[2] << 8) |
                            msg.payload[3];
    EXPECT_EQ(extracted_id, brain_id);
}

TEST_F(MsgFramingTest, FastMsgDanger_SetssBroadcastFlag) {
    nimcp_fast_msg_t msg;

    nimcp_fast_msg_danger(&msg, 1, 42, 0.9f);

    EXPECT_EQ(msg.header.msg_type, MSG_TYPE_DANGER);
    EXPECT_TRUE(nimcp_msg_is_broadcast(&msg.header));
}

TEST_F(MsgFramingTest, FastMsgAck_ValidParams_PacksCorrectly) {
    nimcp_fast_msg_t msg;
    uint32_t brain_id = 100;
    uint32_t acked_seq = 12345;

    nimcp_fast_msg_ack(&msg, brain_id, acked_seq);

    EXPECT_EQ(msg.header.msg_type, MSG_TYPE_ACK);

    // Verify acked_seq in payload
    uint32_t extracted_seq = ((uint32_t)msg.payload[4] << 24) |
                             ((uint32_t)msg.payload[5] << 16) |
                             ((uint32_t)msg.payload[6] << 8) |
                             msg.payload[7];
    EXPECT_EQ(extracted_seq, acked_seq);
}

/*=============================================================================
 * Serialization Tests
 *===========================================================================*/

TEST_F(MsgFramingTest, HeaderSerialize_ValidHeader_ReturnsCorrectSize) {
    nimcp_msg_header_t header;
    uint8_t buffer[NIMCP_MSG_HEADER_SIZE];

    nimcp_msg_header_init(&header, MSG_TYPE_HEARTBEAT, 0, NIMCP_MSG_FAST_PAYLOAD);

    size_t written = nimcp_msg_header_serialize(&header, buffer);

    EXPECT_EQ(written, NIMCP_MSG_HEADER_SIZE);
}

TEST_F(MsgFramingTest, HeaderSerialize_MagicBytesCorrect) {
    nimcp_msg_header_t header;
    uint8_t buffer[NIMCP_MSG_HEADER_SIZE];

    nimcp_msg_header_init(&header, MSG_TYPE_HEARTBEAT, 0, NIMCP_MSG_FAST_PAYLOAD);
    nimcp_msg_header_serialize(&header, buffer);

    EXPECT_EQ(buffer[0], NIMCP_MSG_MAGIC_0);
    EXPECT_EQ(buffer[1], NIMCP_MSG_MAGIC_1);
}

TEST_F(MsgFramingTest, HeaderSerializeDeserialize_RoundTrip_Matches) {
    nimcp_msg_header_t original, deserialized;
    uint8_t buffer[NIMCP_MSG_HEADER_SIZE];

    nimcp_msg_header_init(&original, MSG_TYPE_SHARED_GOAL,
                          MSG_FLAG_PROTOBUF | MSG_FLAG_RELIABLE, 256);

    nimcp_msg_header_serialize(&original, buffer);
    int result = nimcp_msg_header_deserialize(buffer, &deserialized);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(deserialized.magic[0], original.magic[0]);
    EXPECT_EQ(deserialized.magic[1], original.magic[1]);
    EXPECT_EQ(deserialized.version, original.version);
    EXPECT_EQ(deserialized.flags, original.flags);
    EXPECT_EQ(deserialized.msg_type, original.msg_type);
    EXPECT_EQ(deserialized.payload_len, original.payload_len);
}

TEST_F(MsgFramingTest, FastMsgSerialize_ValidMsg_ReturnsCorrectSize) {
    nimcp_fast_msg_t msg;
    uint8_t buffer[24];

    nimcp_fast_msg_heartbeat(&msg, 1, 0.5f, 0.5f);

    size_t written = nimcp_fast_msg_serialize(&msg, buffer);

    EXPECT_EQ(written, 24u);
}

TEST_F(MsgFramingTest, FastMsgSerializeDeserialize_RoundTrip_Matches) {
    nimcp_fast_msg_t original, deserialized;
    uint8_t buffer[24];

    nimcp_fast_msg_heartbeat(&original, 0xCAFEBABE, 0.75f, 0.25f);

    nimcp_fast_msg_serialize(&original, buffer);
    int result = nimcp_fast_msg_deserialize(buffer, &deserialized);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(deserialized.header.msg_type, original.header.msg_type);
    EXPECT_EQ(0, memcmp(deserialized.payload, original.payload, NIMCP_MSG_FAST_PAYLOAD));
}

TEST_F(MsgFramingTest, HeaderDeserialize_InvalidMagic_ReturnsError) {
    uint8_t buffer[NIMCP_MSG_HEADER_SIZE] = {0};
    nimcp_msg_header_t header;

    buffer[0] = 0xFF;  // Wrong magic
    buffer[1] = 0xFF;

    int result = nimcp_msg_header_deserialize(buffer, &header);

    EXPECT_EQ(result, -1);
}

TEST_F(MsgFramingTest, FastMsgDeserialize_ProtobufFlag_ReturnsError) {
    nimcp_fast_msg_t msg;
    uint8_t buffer[24];

    // Create valid fast message
    nimcp_fast_msg_heartbeat(&msg, 1, 0.5f, 0.5f);
    nimcp_fast_msg_serialize(&msg, buffer);

    // Corrupt: set protobuf flag
    buffer[3] |= MSG_FLAG_PROTOBUF;

    nimcp_fast_msg_t deserialized;
    int result = nimcp_fast_msg_deserialize(buffer, &deserialized);

    EXPECT_EQ(result, -1);
}

/*=============================================================================
 * Buffer Tests
 *===========================================================================*/

TEST_F(MsgFramingTest, BufferInit_ValidParams_InitializesCorrectly) {
    nimcp_msg_buffer_t buffer;
    uint8_t payload[1024];

    nimcp_msg_buffer_init(&buffer, payload, sizeof(payload));

    EXPECT_EQ(buffer.payload, payload);
    EXPECT_EQ(buffer.payload_capacity, sizeof(payload));
    EXPECT_EQ(buffer.payload_size, 0u);
}

TEST_F(MsgFramingTest, BufferReset_AfterUse_ClearsPayloadSize) {
    nimcp_msg_buffer_t buffer;
    uint8_t payload[1024];

    nimcp_msg_buffer_init(&buffer, payload, sizeof(payload));
    buffer.payload_size = 500;  // Simulate usage

    nimcp_msg_buffer_reset(&buffer);

    EXPECT_EQ(buffer.payload_size, 0u);
}

TEST_F(MsgFramingTest, BufferTotalSize_WithPayload_ReturnsCorrectSize) {
    nimcp_msg_buffer_t buffer;
    uint8_t payload[1024];

    nimcp_msg_buffer_init(&buffer, payload, sizeof(payload));
    buffer.payload_size = 256;

    size_t total = nimcp_msg_buffer_total_size(&buffer);

    EXPECT_EQ(total, NIMCP_MSG_HEADER_SIZE + 256);
}

/*=============================================================================
 * Statistics Tests
 *===========================================================================*/

TEST_F(MsgFramingTest, Stats_AfterReset_AllZero) {
    nimcp_msg_stats_t stats;

    nimcp_msg_get_stats(&stats);

    EXPECT_EQ(stats.messages_sent, 0u);
    EXPECT_EQ(stats.messages_received, 0u);
    EXPECT_EQ(stats.fast_messages, 0u);
    EXPECT_EQ(stats.parse_errors, 0u);
}

TEST_F(MsgFramingTest, Stats_AfterSerialize_IncrementsSent) {
    nimcp_fast_msg_t msg;
    uint8_t buffer[24];

    nimcp_fast_msg_heartbeat(&msg, 1, 0.5f, 0.5f);
    nimcp_fast_msg_serialize(&msg, buffer);

    nimcp_msg_stats_t stats;
    nimcp_msg_get_stats(&stats);

    EXPECT_EQ(stats.messages_sent, 1u);
    EXPECT_EQ(stats.fast_messages, 1u);
    EXPECT_GT(stats.bytes_sent, 0u);
}

TEST_F(MsgFramingTest, Stats_AfterDeserialize_IncrementsReceived) {
    nimcp_fast_msg_t msg, deserialized;
    uint8_t buffer[24];

    nimcp_fast_msg_heartbeat(&msg, 1, 0.5f, 0.5f);
    nimcp_fast_msg_serialize(&msg, buffer);
    nimcp_fast_msg_deserialize(buffer, &deserialized);

    nimcp_msg_stats_t stats;
    nimcp_msg_get_stats(&stats);

    EXPECT_EQ(stats.messages_received, 1u);
    EXPECT_GT(stats.bytes_received, 0u);
}

TEST_F(MsgFramingTest, Stats_AfterParseError_IncrementsErrors) {
    uint8_t buffer[24] = {0xFF, 0xFF};  // Invalid magic
    nimcp_msg_header_t header;

    nimcp_msg_header_deserialize(buffer, &header);

    nimcp_msg_stats_t stats;
    nimcp_msg_get_stats(&stats);

    EXPECT_EQ(stats.parse_errors, 1u);
}

/*=============================================================================
 * Utility Tests
 *===========================================================================*/

TEST_F(MsgFramingTest, TypeName_KnownTypes_ReturnsCorrectName) {
    EXPECT_STREQ(nimcp_msg_type_name(MSG_TYPE_HEARTBEAT), "HEARTBEAT");
    EXPECT_STREQ(nimcp_msg_type_name(MSG_TYPE_SYNC), "SYNC");
    EXPECT_STREQ(nimcp_msg_type_name(MSG_TYPE_DANGER), "DANGER");
    EXPECT_STREQ(nimcp_msg_type_name(MSG_TYPE_NEURAL_STATE), "NEURAL_STATE");
    EXPECT_STREQ(nimcp_msg_type_name(MSG_TYPE_SHARED_GOAL), "SHARED_GOAL");
    EXPECT_STREQ(nimcp_msg_type_name(MSG_TYPE_BELIEF_GOSSIP), "BELIEF_GOSSIP");
    EXPECT_STREQ(nimcp_msg_type_name(MSG_TYPE_EXT_QUERY), "EXT_QUERY");
    EXPECT_STREQ(nimcp_msg_type_name(MSG_TYPE_PHI_UPDATE), "PHI_UPDATE");
}

TEST_F(MsgFramingTest, TypeName_UnknownType_ReturnsUnknown) {
    EXPECT_STREQ(nimcp_msg_type_name((nimcp_msg_type_t)0xFFFF), "UNKNOWN");
}

TEST_F(MsgFramingTest, TypeCategory_FastPath_ReturnsFast) {
    EXPECT_STREQ(nimcp_msg_type_category(MSG_TYPE_HEARTBEAT), "fast");
    EXPECT_STREQ(nimcp_msg_type_category(MSG_TYPE_SYNC), "fast");
    EXPECT_STREQ(nimcp_msg_type_category(MSG_TYPE_ACK), "fast");
}

TEST_F(MsgFramingTest, TypeCategory_Hyperscanning_ReturnsHyperscanning) {
    EXPECT_STREQ(nimcp_msg_type_category(MSG_TYPE_NEURAL_STATE), "hyperscanning");
    EXPECT_STREQ(nimcp_msg_type_category(MSG_TYPE_ENTRAINMENT_REQ), "hyperscanning");
}

TEST_F(MsgFramingTest, TypeCategory_Intentionality_ReturnsIntentionality) {
    EXPECT_STREQ(nimcp_msg_type_category(MSG_TYPE_SHARED_GOAL), "intentionality");
    EXPECT_STREQ(nimcp_msg_type_category(MSG_TYPE_WE_MODE), "intentionality");
}

TEST_F(MsgFramingTest, TypeCategory_Beliefs_ReturnsBeliefs) {
    EXPECT_STREQ(nimcp_msg_type_category(MSG_TYPE_BELIEF_GOSSIP), "beliefs");
    EXPECT_STREQ(nimcp_msg_type_category(MSG_TYPE_CONSENSUS_VOTE), "beliefs");
}

TEST_F(MsgFramingTest, TypeCategory_ExtendedMind_ReturnsExtendedMind) {
    EXPECT_STREQ(nimcp_msg_type_category(MSG_TYPE_EXT_QUERY), "extended_mind");
    EXPECT_STREQ(nimcp_msg_type_category(MSG_TYPE_OFFLOAD), "extended_mind");
}

TEST_F(MsgFramingTest, TypeCategory_Consciousness_ReturnsConsciousness) {
    EXPECT_STREQ(nimcp_msg_type_category(MSG_TYPE_PHI_UPDATE), "consciousness");
    EXPECT_STREQ(nimcp_msg_type_category(MSG_TYPE_EMERGENCE), "consciousness");
}

/*=============================================================================
 * Edge Case Tests
 *===========================================================================*/

TEST_F(MsgFramingTest, Serialize_NullParams_ReturnsZero) {
    nimcp_msg_header_t header;
    uint8_t buffer[24];

    EXPECT_EQ(nimcp_msg_header_serialize(nullptr, buffer), 0u);
    EXPECT_EQ(nimcp_msg_header_serialize(&header, nullptr), 0u);
    EXPECT_EQ(nimcp_fast_msg_serialize(nullptr, buffer), 0u);
}

TEST_F(MsgFramingTest, Deserialize_NullParams_ReturnsError) {
    uint8_t buffer[24] = {0};
    nimcp_msg_header_t header;
    nimcp_fast_msg_t msg;

    EXPECT_EQ(nimcp_msg_header_deserialize(nullptr, &header), -1);
    EXPECT_EQ(nimcp_msg_header_deserialize(buffer, nullptr), -1);
    EXPECT_EQ(nimcp_fast_msg_deserialize(nullptr, &msg), -1);
    EXPECT_EQ(nimcp_fast_msg_deserialize(buffer, nullptr), -1);
}

TEST_F(MsgFramingTest, AllMessageTypes_HaveValidCategory) {
    // Test that all defined message types return a valid category
    nimcp_msg_type_t types[] = {
        MSG_TYPE_HEARTBEAT, MSG_TYPE_SYNC, MSG_TYPE_DANGER,
        MSG_TYPE_NEURAL_STATE, MSG_TYPE_SHARED_GOAL, MSG_TYPE_BELIEF_GOSSIP,
        MSG_TYPE_EXT_QUERY, MSG_TYPE_PHI_UPDATE, MSG_TYPE_REGISTRATION
    };

    for (auto type : types) {
        const char* category = nimcp_msg_type_category(type);
        EXPECT_NE(category, nullptr);
        EXPECT_STRNE(category, "unknown");
    }
}

/*=============================================================================
 * Big-Endian Correctness Tests
 *===========================================================================*/

TEST_F(MsgFramingTest, BigEndian_MsgType_SerializesCorrectly) {
    nimcp_msg_header_t header;
    uint8_t buffer[NIMCP_MSG_HEADER_SIZE];

    nimcp_msg_header_init(&header, (nimcp_msg_type_t)0x1234, MSG_FLAG_PROTOBUF, 100);
    nimcp_msg_header_serialize(&header, buffer);

    // Big-endian: high byte first
    EXPECT_EQ(buffer[4], 0x12);
    EXPECT_EQ(buffer[5], 0x34);
}

TEST_F(MsgFramingTest, BigEndian_PayloadLen_SerializesCorrectly) {
    nimcp_msg_header_t header;
    uint8_t buffer[NIMCP_MSG_HEADER_SIZE];

    nimcp_msg_header_init(&header, MSG_TYPE_NEURAL_STATE, MSG_FLAG_PROTOBUF, 0xABCD);
    nimcp_msg_header_serialize(&header, buffer);

    // Big-endian: high byte first
    EXPECT_EQ(buffer[6], 0xAB);
    EXPECT_EQ(buffer[7], 0xCD);
}

/*=============================================================================
 * Integration-style Tests
 *===========================================================================*/

TEST_F(MsgFramingTest, MultipleMessages_StatisticsAccumulate) {
    nimcp_fast_msg_t msg;
    uint8_t buffer[24];

    // Send 10 messages
    for (int i = 0; i < 10; i++) {
        nimcp_fast_msg_heartbeat(&msg, i, 0.5f, 0.5f);
        nimcp_fast_msg_serialize(&msg, buffer);
    }

    nimcp_msg_stats_t stats;
    nimcp_msg_get_stats(&stats);

    EXPECT_EQ(stats.messages_sent, 10u);
    EXPECT_EQ(stats.fast_messages, 10u);
}

TEST_F(MsgFramingTest, MixedMessageTypes_AllSerializeCorrectly) {
    uint8_t buffer[24];
    nimcp_fast_msg_t msg;
    int result;

    // Heartbeat
    nimcp_fast_msg_heartbeat(&msg, 1, 0.5f, 0.3f);
    EXPECT_EQ(nimcp_fast_msg_serialize(&msg, buffer), 24u);

    // Sync
    nimcp_fast_msg_sync(&msg, 2, 0.8f, 1.57f);
    EXPECT_EQ(nimcp_fast_msg_serialize(&msg, buffer), 24u);

    // Danger
    nimcp_fast_msg_danger(&msg, 3, 99, 0.95f);
    EXPECT_EQ(nimcp_fast_msg_serialize(&msg, buffer), 24u);

    // ACK
    nimcp_fast_msg_ack(&msg, 4, 1000);
    EXPECT_EQ(nimcp_fast_msg_serialize(&msg, buffer), 24u);

    // All should deserialize
    nimcp_fast_msg_t deserialized;
    result = nimcp_fast_msg_deserialize(buffer, &deserialized);
    EXPECT_EQ(result, 0);
}
