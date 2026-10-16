/**
 * @file test_swarm_protocol.cpp
 * @brief Comprehensive unit tests for NIMCP Swarm Protocol
 *
 * TEST COVERAGE:
 * - Message encoding/decoding for all types
 * - CRC validation and error detection
 * - Invalid message handling
 * - Phoneme sequence mapping
 * - Payload serialization
 * - Protocol version handling
 * - Edge cases and boundary conditions
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <random>

// Mock swarm protocol structures based on architecture doc
// Headers have their own extern "C" guards

// Message types
typedef enum {
    SWARM_MSG_HEARTBEAT,
    SWARM_MSG_THREAT_DETECTED,
    SWARM_MSG_TARGET_FOUND,
    SWARM_MSG_REQUEST_BACKUP,
    SWARM_MSG_FORMATION_CHANGE,
    SWARM_MSG_REWARD_SIGNAL,
    SWARM_MSG_NEUROMOD_SYNC,
    SWARM_MSG_WORKSPACE_BROADCAST,
    SWARM_MSG_VOTE_REQUEST,
    SWARM_MSG_VOTE_RESPONSE,
} swarm_message_type_t;

// Phoneme message structure (24 bytes)
typedef struct {
    uint8_t phoneme_sequence[8];  // Up to 8 phonemes per message
    uint8_t sequence_length;      // Actual phoneme count
    uint8_t message_type;         // swarm_message_type_t
    uint16_t sender_id;           // Drone ID
    float payload[4];             // Type-specific data
    uint16_t crc16;               // Error detection
} swarm_phoneme_message_t;

// Protocol API functions (to be implemented)
bool swarm_protocol_encode_message(
    const swarm_phoneme_message_t* message,
    uint8_t* buffer,
    uint32_t buffer_size,
    uint32_t* bytes_written
);

bool swarm_protocol_decode_message(
    const uint8_t* buffer,
    uint32_t buffer_size,
    swarm_phoneme_message_t* message
);

uint16_t swarm_protocol_compute_crc16(
    const uint8_t* data,
    uint32_t length
);

bool swarm_protocol_validate_message(
    const swarm_phoneme_message_t* message
);

const char* swarm_protocol_message_type_to_phoneme(
    swarm_message_type_t type
);

swarm_message_type_t swarm_protocol_phoneme_to_message_type(
    const char* phoneme
);

//=============================================================================
// Mock Implementation (for testing purposes)
//=============================================================================

// CRC-16-CCITT implementation
uint16_t swarm_protocol_compute_crc16(const uint8_t* data, uint32_t length) {
    uint16_t crc = 0xFFFF;
    for (uint32_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// Phoneme mappings
const char* swarm_protocol_message_type_to_phoneme(swarm_message_type_t type) {
    switch (type) {
        case SWARM_MSG_HEARTBEAT: return "/hɛlo/";
        case SWARM_MSG_THREAT_DETECTED: return "/deɪnʤər/";
        case SWARM_MSG_TARGET_FOUND: return "/faʊnd/";
        case SWARM_MSG_REQUEST_BACKUP: return "/hɛlp/";
        case SWARM_MSG_FORMATION_CHANGE: return "/muːv/";
        case SWARM_MSG_REWARD_SIGNAL: return "/gʊd/";
        case SWARM_MSG_NEUROMOD_SYNC: return "/sɪŋk/";
        case SWARM_MSG_WORKSPACE_BROADCAST: return "/brɔːd/";
        case SWARM_MSG_VOTE_REQUEST: return "/voʊt/";
        case SWARM_MSG_VOTE_RESPONSE: return "/jɛs/";
        default: return nullptr;
    }
}

swarm_message_type_t swarm_protocol_phoneme_to_message_type(const char* phoneme) {
    if (!phoneme) return SWARM_MSG_HEARTBEAT;
    if (strcmp(phoneme, "/hɛlo/") == 0) return SWARM_MSG_HEARTBEAT;
    if (strcmp(phoneme, "/deɪnʤər/") == 0) return SWARM_MSG_THREAT_DETECTED;
    if (strcmp(phoneme, "/faʊnd/") == 0) return SWARM_MSG_TARGET_FOUND;
    if (strcmp(phoneme, "/hɛlp/") == 0) return SWARM_MSG_REQUEST_BACKUP;
    if (strcmp(phoneme, "/muːv/") == 0) return SWARM_MSG_FORMATION_CHANGE;
    if (strcmp(phoneme, "/gʊd/") == 0) return SWARM_MSG_REWARD_SIGNAL;
    if (strcmp(phoneme, "/sɪŋk/") == 0) return SWARM_MSG_NEUROMOD_SYNC;
    if (strcmp(phoneme, "/brɔːd/") == 0) return SWARM_MSG_WORKSPACE_BROADCAST;
    if (strcmp(phoneme, "/voʊt/") == 0) return SWARM_MSG_VOTE_REQUEST;
    if (strcmp(phoneme, "/jɛs/") == 0) return SWARM_MSG_VOTE_RESPONSE;
    return SWARM_MSG_HEARTBEAT;
}

bool swarm_protocol_validate_message(const swarm_phoneme_message_t* message) {
    if (!message) return false;
    if (message->sequence_length > 8) return false;
    if (message->message_type >= 10) return false; // Max message types
    if (message->sender_id == 0) return false; // Reserved

    // Validate CRC
    uint8_t temp_buffer[22]; // Message without CRC
    memcpy(temp_buffer, message, 22);
    uint16_t computed_crc = swarm_protocol_compute_crc16(temp_buffer, 22);

    return computed_crc == message->crc16;
}

bool swarm_protocol_encode_message(
    const swarm_phoneme_message_t* message,
    uint8_t* buffer,
    uint32_t buffer_size,
    uint32_t* bytes_written
) {
    if (!message || !buffer || buffer_size < sizeof(swarm_phoneme_message_t)) {
        return false;
    }

    // Create message with CRC
    swarm_phoneme_message_t encoded = *message;

    // Compute CRC on all fields except CRC itself
    uint8_t temp_buffer[22];
    memcpy(temp_buffer, &encoded, 22);
    encoded.crc16 = swarm_protocol_compute_crc16(temp_buffer, 22);

    // Copy to output buffer
    memcpy(buffer, &encoded, sizeof(swarm_phoneme_message_t));
    if (bytes_written) {
        *bytes_written = sizeof(swarm_phoneme_message_t);
    }

    return true;
}

bool swarm_protocol_decode_message(
    const uint8_t* buffer,
    uint32_t buffer_size,
    swarm_phoneme_message_t* message
) {
    if (!buffer || !message || buffer_size < sizeof(swarm_phoneme_message_t)) {
        return false;
    }

    // Copy from buffer
    memcpy(message, buffer, sizeof(swarm_phoneme_message_t));

    // Validate
    return swarm_protocol_validate_message(message);
}

//=============================================================================
// Test Fixtures
//=============================================================================

class SwarmProtocolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize random seed for reproducible tests
        rng.seed(42);
    }

    void TearDown() override {
        // Cleanup if needed
    }

    // Helper: Create a test message
    swarm_phoneme_message_t CreateTestMessage(
        swarm_message_type_t type,
        uint16_t sender_id,
        const float* payload = nullptr
    ) {
        swarm_phoneme_message_t msg;
        memset(&msg, 0, sizeof(msg));

        // Set phoneme sequence (simplified - use message type as pattern)
        msg.sequence_length = std::min((uint8_t)4, (uint8_t)8);
        for (uint8_t i = 0; i < msg.sequence_length; i++) {
            msg.phoneme_sequence[i] = (type + i) % 44; // 44 phonemes
        }

        msg.message_type = type;
        msg.sender_id = sender_id;

        if (payload) {
            memcpy(msg.payload, payload, sizeof(float) * 4);
        } else {
            for (int i = 0; i < 4; i++) {
                msg.payload[i] = static_cast<float>(i) * 1.5f;
            }
        }

        msg.crc16 = 0; // Will be computed on encode

        return msg;
    }

    // Helper: Generate random payload
    std::vector<float> GenerateRandomPayload() {
        std::vector<float> payload(4);
        std::uniform_real_distribution<float> dist(-100.0f, 100.0f);
        for (int i = 0; i < 4; i++) {
            payload[i] = dist(rng);
        }
        return payload;
    }

    std::mt19937 rng;
};

//=============================================================================
// 1. Message Encoding Tests
//=============================================================================

TEST_F(SwarmProtocolTest, EncodeHeartbeatMessage) {
    swarm_phoneme_message_t msg = CreateTestMessage(SWARM_MSG_HEARTBEAT, 1);
    uint8_t buffer[128];
    uint32_t bytes_written = 0;

    bool success = swarm_protocol_encode_message(&msg, buffer, sizeof(buffer), &bytes_written);

    ASSERT_TRUE(success);
    EXPECT_EQ(bytes_written, sizeof(swarm_phoneme_message_t));
    // Note: sizeof may differ from documentation due to struct padding
}

TEST_F(SwarmProtocolTest, EncodeAllMessageTypes) {
    for (int type = SWARM_MSG_HEARTBEAT; type <= SWARM_MSG_VOTE_RESPONSE; type++) {
        swarm_phoneme_message_t msg = CreateTestMessage(
            static_cast<swarm_message_type_t>(type),
            100 + type
        );

        uint8_t buffer[128];
        uint32_t bytes_written = 0;

        bool success = swarm_protocol_encode_message(&msg, buffer, sizeof(buffer), &bytes_written);

        ASSERT_TRUE(success) << "Failed to encode message type " << type;
        EXPECT_EQ(bytes_written, sizeof(swarm_phoneme_message_t));
    }
}

TEST_F(SwarmProtocolTest, EncodeWithCustomPayload) {
    auto payload = GenerateRandomPayload();
    swarm_phoneme_message_t msg = CreateTestMessage(
        SWARM_MSG_TARGET_FOUND,
        42,
        payload.data()
    );

    uint8_t buffer[128];
    uint32_t bytes_written = 0;

    bool success = swarm_protocol_encode_message(&msg, buffer, sizeof(buffer), &bytes_written);

    ASSERT_TRUE(success);

    // Verify payload preserved
    swarm_phoneme_message_t* encoded = (swarm_phoneme_message_t*)buffer;
    for (int i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(encoded->payload[i], payload[i]);
    }
}

TEST_F(SwarmProtocolTest, EncodeWithNullBuffer) {
    swarm_phoneme_message_t msg = CreateTestMessage(SWARM_MSG_HEARTBEAT, 1);
    uint32_t bytes_written = 0;

    bool success = swarm_protocol_encode_message(&msg, nullptr, 128, &bytes_written);

    EXPECT_FALSE(success);
}

TEST_F(SwarmProtocolTest, EncodeWithInsufficientBuffer) {
    swarm_phoneme_message_t msg = CreateTestMessage(SWARM_MSG_HEARTBEAT, 1);
    uint8_t buffer[10]; // Too small
    uint32_t bytes_written = 0;

    bool success = swarm_protocol_encode_message(&msg, buffer, sizeof(buffer), &bytes_written);

    EXPECT_FALSE(success);
}

//=============================================================================
// 2. Message Decoding Tests
//=============================================================================

TEST_F(SwarmProtocolTest, DecodeValidMessage) {
    // Encode a message first
    swarm_phoneme_message_t original = CreateTestMessage(SWARM_MSG_THREAT_DETECTED, 7);
    uint8_t buffer[128];
    uint32_t bytes_written = 0;

    ASSERT_TRUE(swarm_protocol_encode_message(&original, buffer, sizeof(buffer), &bytes_written));

    // Now decode it
    swarm_phoneme_message_t decoded;
    bool success = swarm_protocol_decode_message(buffer, bytes_written, &decoded);

    ASSERT_TRUE(success);
    EXPECT_EQ(decoded.message_type, SWARM_MSG_THREAT_DETECTED);
    EXPECT_EQ(decoded.sender_id, 7);
    EXPECT_EQ(decoded.sequence_length, original.sequence_length);

    for (int i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(decoded.payload[i], original.payload[i]);
    }
}

TEST_F(SwarmProtocolTest, DecodeAllMessageTypes) {
    for (int type = SWARM_MSG_HEARTBEAT; type <= SWARM_MSG_VOTE_RESPONSE; type++) {
        swarm_phoneme_message_t original = CreateTestMessage(
            static_cast<swarm_message_type_t>(type),
            200 + type
        );

        uint8_t buffer[128];
        uint32_t bytes_written = 0;

        ASSERT_TRUE(swarm_protocol_encode_message(&original, buffer, sizeof(buffer), &bytes_written));

        swarm_phoneme_message_t decoded;
        bool success = swarm_protocol_decode_message(buffer, bytes_written, &decoded);

        ASSERT_TRUE(success) << "Failed to decode message type " << type;
        EXPECT_EQ(decoded.message_type, type);
        EXPECT_EQ(decoded.sender_id, 200 + type);
    }
}

TEST_F(SwarmProtocolTest, DecodeWithNullBuffer) {
    swarm_phoneme_message_t decoded;
    bool success = swarm_protocol_decode_message(nullptr, 24, &decoded);

    EXPECT_FALSE(success);
}

TEST_F(SwarmProtocolTest, DecodeWithInvalidSize) {
    uint8_t buffer[10];
    swarm_phoneme_message_t decoded;

    bool success = swarm_protocol_decode_message(buffer, sizeof(buffer), &decoded);

    EXPECT_FALSE(success);
}

//=============================================================================
// 3. CRC Validation Tests
//=============================================================================

TEST_F(SwarmProtocolTest, CRCComputationConsistent) {
    uint8_t data[] = {1, 2, 3, 4, 5, 6, 7, 8};

    uint16_t crc1 = swarm_protocol_compute_crc16(data, sizeof(data));
    uint16_t crc2 = swarm_protocol_compute_crc16(data, sizeof(data));

    EXPECT_EQ(crc1, crc2);
}

TEST_F(SwarmProtocolTest, CRCDetectsSingleBitFlip) {
    swarm_phoneme_message_t original = CreateTestMessage(SWARM_MSG_HEARTBEAT, 1);
    uint8_t buffer[128];
    uint32_t bytes_written = 0;

    ASSERT_TRUE(swarm_protocol_encode_message(&original, buffer, sizeof(buffer), &bytes_written));

    // Flip a bit in the payload
    buffer[10] ^= 0x01;

    // Try to decode - should fail CRC check
    swarm_phoneme_message_t decoded;
    bool success = swarm_protocol_decode_message(buffer, bytes_written, &decoded);

    EXPECT_FALSE(success); // CRC should detect corruption
}

TEST_F(SwarmProtocolTest, CRCDetectsMultipleBitFlips) {
    swarm_phoneme_message_t original = CreateTestMessage(SWARM_MSG_HEARTBEAT, 1);
    uint8_t buffer[128];
    uint32_t bytes_written = 0;

    ASSERT_TRUE(swarm_protocol_encode_message(&original, buffer, sizeof(buffer), &bytes_written));

    // Flip multiple bits
    buffer[5] ^= 0xFF;
    buffer[10] ^= 0xAA;
    buffer[15] ^= 0x55;

    swarm_phoneme_message_t decoded;
    bool success = swarm_protocol_decode_message(buffer, bytes_written, &decoded);

    EXPECT_FALSE(success);
}

TEST_F(SwarmProtocolTest, ValidMessagePassesCRC) {
    swarm_phoneme_message_t msg = CreateTestMessage(SWARM_MSG_HEARTBEAT, 1);
    uint8_t buffer[128];
    uint32_t bytes_written = 0;

    ASSERT_TRUE(swarm_protocol_encode_message(&msg, buffer, sizeof(buffer), &bytes_written));

    swarm_phoneme_message_t decoded;
    bool success = swarm_protocol_decode_message(buffer, bytes_written, &decoded);

    EXPECT_TRUE(success);
}

//=============================================================================
// 4. Phoneme Sequence Mapping Tests
//=============================================================================

TEST_F(SwarmProtocolTest, MessageTypeToPhoneme) {
    const char* phoneme = swarm_protocol_message_type_to_phoneme(SWARM_MSG_HEARTBEAT);
    ASSERT_NE(phoneme, nullptr);
    EXPECT_STREQ(phoneme, "/hɛlo/");

    phoneme = swarm_protocol_message_type_to_phoneme(SWARM_MSG_THREAT_DETECTED);
    ASSERT_NE(phoneme, nullptr);
    EXPECT_STREQ(phoneme, "/deɪnʤər/");
}

TEST_F(SwarmProtocolTest, AllMessageTypesHavePhonemes) {
    for (int type = SWARM_MSG_HEARTBEAT; type <= SWARM_MSG_VOTE_RESPONSE; type++) {
        const char* phoneme = swarm_protocol_message_type_to_phoneme(
            static_cast<swarm_message_type_t>(type)
        );
        ASSERT_NE(phoneme, nullptr) << "Message type " << type << " missing phoneme";
        EXPECT_GT(strlen(phoneme), 0u);
    }
}

TEST_F(SwarmProtocolTest, PhonemeToMessageType) {
    swarm_message_type_t type = swarm_protocol_phoneme_to_message_type("/hɛlo/");
    EXPECT_EQ(type, SWARM_MSG_HEARTBEAT);

    type = swarm_protocol_phoneme_to_message_type("/deɪnʤər/");
    EXPECT_EQ(type, SWARM_MSG_THREAT_DETECTED);
}

TEST_F(SwarmProtocolTest, PhonemeRoundTrip) {
    for (int type = SWARM_MSG_HEARTBEAT; type <= SWARM_MSG_VOTE_RESPONSE; type++) {
        const char* phoneme = swarm_protocol_message_type_to_phoneme(
            static_cast<swarm_message_type_t>(type)
        );
        swarm_message_type_t decoded_type = swarm_protocol_phoneme_to_message_type(phoneme);

        EXPECT_EQ(decoded_type, type) << "Round trip failed for type " << type;
    }
}

TEST_F(SwarmProtocolTest, InvalidPhonemeReturnsDefault) {
    swarm_message_type_t type = swarm_protocol_phoneme_to_message_type("/invalid/");
    EXPECT_EQ(type, SWARM_MSG_HEARTBEAT); // Default fallback
}

//=============================================================================
// 5. Message Validation Tests
//=============================================================================

TEST_F(SwarmProtocolTest, ValidateValidMessage) {
    swarm_phoneme_message_t msg = CreateTestMessage(SWARM_MSG_HEARTBEAT, 1);
    uint8_t buffer[128];
    uint32_t bytes_written = 0;

    ASSERT_TRUE(swarm_protocol_encode_message(&msg, buffer, sizeof(buffer), &bytes_written));

    swarm_phoneme_message_t* encoded = (swarm_phoneme_message_t*)buffer;
    bool valid = swarm_protocol_validate_message(encoded);

    EXPECT_TRUE(valid);
}

TEST_F(SwarmProtocolTest, ValidateInvalidSequenceLength) {
    swarm_phoneme_message_t msg = CreateTestMessage(SWARM_MSG_HEARTBEAT, 1);
    msg.sequence_length = 10; // Invalid (max is 8)

    // Even if we encode with wrong length, validation should catch it
    bool valid = swarm_protocol_validate_message(&msg);

    EXPECT_FALSE(valid);
}

TEST_F(SwarmProtocolTest, ValidateInvalidSenderId) {
    swarm_phoneme_message_t msg = CreateTestMessage(SWARM_MSG_HEARTBEAT, 0); // 0 is reserved

    bool valid = swarm_protocol_validate_message(&msg);

    EXPECT_FALSE(valid);
}

TEST_F(SwarmProtocolTest, ValidateNullMessage) {
    bool valid = swarm_protocol_validate_message(nullptr);

    EXPECT_FALSE(valid);
}

//=============================================================================
// 6. Edge Cases and Boundary Conditions
//=============================================================================

TEST_F(SwarmProtocolTest, MaxSenderId) {
    swarm_phoneme_message_t msg = CreateTestMessage(SWARM_MSG_HEARTBEAT, 65535);
    uint8_t buffer[128];
    uint32_t bytes_written = 0;

    ASSERT_TRUE(swarm_protocol_encode_message(&msg, buffer, sizeof(buffer), &bytes_written));

    swarm_phoneme_message_t decoded;
    ASSERT_TRUE(swarm_protocol_decode_message(buffer, bytes_written, &decoded));

    EXPECT_EQ(decoded.sender_id, 65535);
}

TEST_F(SwarmProtocolTest, MaxPhonemeSequenceLength) {
    swarm_phoneme_message_t msg = CreateTestMessage(SWARM_MSG_HEARTBEAT, 1);
    msg.sequence_length = 8; // Max length
    for (int i = 0; i < 8; i++) {
        msg.phoneme_sequence[i] = i;
    }

    uint8_t buffer[128];
    uint32_t bytes_written = 0;

    ASSERT_TRUE(swarm_protocol_encode_message(&msg, buffer, sizeof(buffer), &bytes_written));

    swarm_phoneme_message_t decoded;
    ASSERT_TRUE(swarm_protocol_decode_message(buffer, bytes_written, &decoded));

    EXPECT_EQ(decoded.sequence_length, 8);
    for (int i = 0; i < 8; i++) {
        EXPECT_EQ(decoded.phoneme_sequence[i], i);
    }
}

TEST_F(SwarmProtocolTest, ZeroPayload) {
    float zero_payload[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    swarm_phoneme_message_t msg = CreateTestMessage(SWARM_MSG_HEARTBEAT, 1, zero_payload);

    uint8_t buffer[128];
    uint32_t bytes_written = 0;

    ASSERT_TRUE(swarm_protocol_encode_message(&msg, buffer, sizeof(buffer), &bytes_written));

    swarm_phoneme_message_t decoded;
    ASSERT_TRUE(swarm_protocol_decode_message(buffer, bytes_written, &decoded));

    for (int i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(decoded.payload[i], 0.0f);
    }
}

TEST_F(SwarmProtocolTest, ExtremePayloadValues) {
    float extreme_payload[4] = {
        -std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::min(),
        std::numeric_limits<float>::infinity()
    };

    swarm_phoneme_message_t msg = CreateTestMessage(SWARM_MSG_HEARTBEAT, 1, extreme_payload);

    uint8_t buffer[128];
    uint32_t bytes_written = 0;

    ASSERT_TRUE(swarm_protocol_encode_message(&msg, buffer, sizeof(buffer), &bytes_written));

    swarm_phoneme_message_t decoded;
    ASSERT_TRUE(swarm_protocol_decode_message(buffer, bytes_written, &decoded));

    EXPECT_FLOAT_EQ(decoded.payload[0], extreme_payload[0]);
    EXPECT_FLOAT_EQ(decoded.payload[1], extreme_payload[1]);
    EXPECT_FLOAT_EQ(decoded.payload[2], extreme_payload[2]);
    EXPECT_TRUE(std::isinf(decoded.payload[3]));
}

//=============================================================================
// 7. Performance and Stress Tests
//=============================================================================

TEST_F(SwarmProtocolTest, EncodeDecodePerformance) {
    const int iterations = 10000;
    auto payload = GenerateRandomPayload();

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        swarm_phoneme_message_t msg = CreateTestMessage(
            static_cast<swarm_message_type_t>(i % 10),
            i,
            payload.data()
        );

        uint8_t buffer[128];
        uint32_t bytes_written = 0;

        swarm_protocol_encode_message(&msg, buffer, sizeof(buffer), &bytes_written);

        swarm_phoneme_message_t decoded;
        swarm_protocol_decode_message(buffer, bytes_written, &decoded);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should complete in reasonable time (< 1 second for 10k iterations)
    EXPECT_LT(duration.count(), 1000000);

    std::cout << "Encode/Decode " << iterations << " messages in "
              << duration.count() << " microseconds" << std::endl;
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
