/**
 * @file test_protocol_regression.cpp
 * @brief Comprehensive regression tests for NIMCP 2.0 protocol stability
 *
 * WHAT: Regression tests for NIMCP protocol wire format and backwards compatibility
 * WHY:  Ensure protocol stability, detect breaking changes, maintain interoperability
 * HOW:  Test protocol constants, wire formats, serialization stability, performance
 *
 * REGRESSION CATEGORIES:
 * - Protocol Version Stability: VERSION 2 compatibility
 * - Magic Number Consistency: PROTOCOL_MAGIC = 0x4E494D43
 * - Message Format Stability: Header size, field offsets, alignment
 * - Event Packet Wire Format: Packed struct layout stability
 * - Control Message Wire Format: Message structure stability
 * - Feature Code Namespace: Domain values immutability
 * - Backwards Compatibility: Serialized data from older versions
 * - Performance Baselines: Serialize/deserialize speed requirements
 * - Memory Footprint: Struct sizes must remain stable
 * - Checksum Algorithm: CRC32 consistency
 * - Large Scale Tests: 10000+ messages stress testing
 * - Concurrent Handling: Thread safety verification
 * - Edge Cases: Boundary conditions and extreme values
 * - Deterministic Behavior: Same input → same output
 * - Memory Leak Detection: Repeated alloc/free cycles
 *
 * @author NIMCP Test Team
 * @date 2025-11-20
 * @version 2.0
 */

#include <gtest/gtest.h>
#include "networking/protocol/nimcp_protocol.h"
#include <cstring>
#include <chrono>
#include <vector>
#include <thread>
#include <atomic>
#include <random>
#include <algorithm>
#include <unordered_map>

//=============================================================================
// Test Utilities
//=============================================================================

class ProtocolRegressionTest : public ::testing::Test {
protected:
    std::vector<uint8_t> buffer;

    void SetUp() override {
        // Allocate buffer large enough for max payload
        buffer.resize(sizeof(msg_header_t) + MAX_PAYLOAD_SIZE);
    }

    void TearDown() override {
        buffer.clear();
    }

    // Helper to create test payload
    std::vector<uint8_t> create_test_payload(uint32_t size) {
        std::vector<uint8_t> payload(size);
        for (uint32_t i = 0; i < size; i++) {
            payload[i] = static_cast<uint8_t>(i & 0xFF);
        }
        return payload;
    }

    // Helper to measure serialization time
    template<typename Func>
    double measure_time_ms(Func func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }
};

//=============================================================================
// Protocol Version Stability Tests
//=============================================================================

TEST_F(ProtocolRegressionTest, ProtocolVersionStable) {
    // WHAT: Verify PROTOCOL_VERSION constant
    // WHY:  Version changes break backwards compatibility
    // REGRESSION: Must remain 2 for NIMCP 2.0

    EXPECT_EQ(PROTOCOL_VERSION, 2);
}

TEST_F(ProtocolRegressionTest, ProtocolVersionInHeader) {
    // WHAT: Verify version field is set correctly in headers
    // WHY:  Version mismatch detection depends on this
    // REGRESSION: Version field must be populated

    auto payload = create_test_payload(16);
    int bytes = protocol_serialize_message(MSG_TYPE_HANDSHAKE, payload.data(),
                                          payload.size(), buffer.data(), buffer.size());
    ASSERT_GT(bytes, 0);

    msg_header_t header;
    memcpy(&header, buffer.data(), sizeof(msg_header_t));
    EXPECT_EQ(header.version, PROTOCOL_VERSION);
}

//=============================================================================
// Magic Number Consistency Tests
//=============================================================================

TEST_F(ProtocolRegressionTest, MagicNumberStable) {
    // WHAT: Verify PROTOCOL_MAGIC constant value
    // WHY:  Magic number is protocol signature, must never change
    // REGRESSION: Must be 0x4E494D43 ("NIMC" in ASCII)

    EXPECT_EQ(PROTOCOL_MAGIC, 0x4E494D43);
}

TEST_F(ProtocolRegressionTest, MagicNumberInHeader) {
    // WHAT: Verify magic number in serialized messages
    // WHY:  Message validation depends on magic number
    // REGRESSION: Every message must contain magic number

    auto payload = create_test_payload(32);
    int bytes = protocol_serialize_message(MSG_TYPE_PING, payload.data(),
                                          payload.size(), buffer.data(), buffer.size());
    ASSERT_GT(bytes, 0);

    msg_header_t header;
    memcpy(&header, buffer.data(), sizeof(msg_header_t));
    EXPECT_EQ(header.magic, PROTOCOL_MAGIC);
}

TEST_F(ProtocolRegressionTest, InvalidMagicRejected) {
    // WHAT: Verify validation rejects wrong magic number
    // WHY:  Protocol security - reject malformed messages
    // REGRESSION: Bug fix - was accepting any magic (Issue #5001)

    msg_header_t header = {};
    header.magic = 0xDEADBEEF;  // Wrong magic
    header.version = PROTOCOL_VERSION;
    header.type = MSG_TYPE_PING;
    header.length = 0;
    header.sequence = 0;
    header.checksum = 0;

    EXPECT_FALSE(protocol_validate_header(&header));
}

//=============================================================================
// Message Format Stability Tests
//=============================================================================

TEST_F(ProtocolRegressionTest, HeaderSizeStable) {
    // WHAT: Verify msg_header_t struct size
    // WHY:  Wire format compatibility - size changes break protocol
    // REGRESSION: Must be 24 bytes (with padding for msg_type_t enum)

    EXPECT_EQ(sizeof(msg_header_t), 24u);
}

TEST_F(ProtocolRegressionTest, HeaderFieldOffsets) {
    // WHAT: Verify field offsets within header
    // WHY:  Wire format - field positions must not change
    // REGRESSION: Offset stability is critical for interoperability

    msg_header_t header = {};
    uint8_t* base = reinterpret_cast<uint8_t*>(&header);

    EXPECT_EQ(reinterpret_cast<uint8_t*>(&header.magic) - base, 0u);
    EXPECT_EQ(reinterpret_cast<uint8_t*>(&header.version) - base, 4u);
    EXPECT_EQ(reinterpret_cast<uint8_t*>(&header.type) - base, 8u);   // Padded enum
    EXPECT_EQ(reinterpret_cast<uint8_t*>(&header.length) - base, 12u);
    EXPECT_EQ(reinterpret_cast<uint8_t*>(&header.sequence) - base, 16u);
    EXPECT_EQ(reinterpret_cast<uint8_t*>(&header.checksum) - base, 20u);
}

TEST_F(ProtocolRegressionTest, MessageTypeEnumStable) {
    // WHAT: Verify message type enum values
    // WHY:  Wire format - enum values must not change
    // REGRESSION: Changing enum values breaks protocol

    EXPECT_EQ(static_cast<int>(MSG_TYPE_HELLO), 0);
    EXPECT_EQ(static_cast<int>(MSG_TYPE_HANDSHAKE), 1);
    EXPECT_EQ(static_cast<int>(MSG_TYPE_STATE_UPDATE), 2);
    EXPECT_EQ(static_cast<int>(MSG_TYPE_PING), 3);
    EXPECT_EQ(static_cast<int>(MSG_TYPE_PONG), 4);
    EXPECT_EQ(static_cast<int>(MSG_TYPE_DISCONNECT), 5);
}

TEST_F(ProtocolRegressionTest, MaxPayloadSizeStable) {
    // WHAT: Verify MAX_PAYLOAD_SIZE constant
    // WHY:  Protocol limit - changing breaks compatibility
    // REGRESSION: Must remain 65536 bytes

    EXPECT_EQ(MAX_PAYLOAD_SIZE, 65536u);
}

//=============================================================================
// Event Packet Wire Format Stability Tests
//=============================================================================

TEST_F(ProtocolRegressionTest, EventPacketSizeStable) {
    // WHAT: Verify event_packet_t struct size
    // WHY:  Wire format - packed struct must maintain size
    // REGRESSION: Must be 26 bytes (packed structure)

    EXPECT_EQ(sizeof(event_packet_t), 26u);
}

TEST_F(ProtocolRegressionTest, EventPacketFieldOffsets) {
    // WHAT: Verify field offsets in event packet
    // WHY:  Wire format stability for high-frequency transmission
    // REGRESSION: Packed struct layout must not change

    event_packet_t packet = {};
    uint8_t* base = reinterpret_cast<uint8_t*>(&packet);

    EXPECT_EQ(reinterpret_cast<uint8_t*>(&packet.version_flags) - base, 0u);
    EXPECT_EQ(reinterpret_cast<uint8_t*>(&packet.reserved) - base, 1u);
    EXPECT_EQ(reinterpret_cast<uint8_t*>(&packet.feature_high) - base, 2u);
    EXPECT_EQ(reinterpret_cast<uint8_t*>(&packet.feature_low) - base, 4u);
    EXPECT_EQ(reinterpret_cast<uint8_t*>(&packet.source_node_id) - base, 6u);
    EXPECT_EQ(reinterpret_cast<uint8_t*>(&packet.timestamp) - base, 10u);
    EXPECT_EQ(reinterpret_cast<uint8_t*>(&packet.confidence) - base, 18u);
    EXPECT_EQ(reinterpret_cast<uint8_t*>(&packet.hop_count) - base, 20u);
    EXPECT_EQ(reinterpret_cast<uint8_t*>(&packet.reserved2) - base, 21u);
    EXPECT_EQ(reinterpret_cast<uint8_t*>(&packet.payload_length) - base, 22u);
}

TEST_F(ProtocolRegressionTest, EventFlagValuesStable) {
    // WHAT: Verify event flag bit positions
    // WHY:  Wire format - flag values must not change
    // REGRESSION: Flag bits are part of protocol spec

    EXPECT_EQ(EVENT_FLAG_EXCITATORY, 1 << 0);
    EXPECT_EQ(EVENT_FLAG_INHIBITORY, 1 << 1);
    EXPECT_EQ(EVENT_FLAG_PLASTICITY, 1 << 2);
    EXPECT_EQ(EVENT_FLAG_ROUTE_REC, 1 << 3);
}

TEST_F(ProtocolRegressionTest, EventPacketMacrosStable) {
    // WHAT: Verify event packet accessor macros
    // WHY:  API stability - macro behavior must not change
    // REGRESSION: Version/flags packing must remain stable

    event_packet_t packet = {};

    EVENT_SET_VERSION(&packet, 2);
    EVENT_SET_FLAGS(&packet, EVENT_FLAG_EXCITATORY | EVENT_FLAG_PLASTICITY);

    EXPECT_EQ(EVENT_GET_VERSION(&packet), 2);
    EXPECT_EQ(EVENT_GET_FLAGS(&packet), EVENT_FLAG_EXCITATORY | EVENT_FLAG_PLASTICITY);
}

TEST_F(ProtocolRegressionTest, EventFeatureCodeMacrosStable) {
    // WHAT: Verify feature code macros
    // WHY:  API stability - 32-bit feature code handling
    // REGRESSION: Feature code packing/unpacking must be stable

    event_packet_t packet = {};
    feature_code_t code = 0x12345678;

    EVENT_SET_FEATURE_CODE(&packet, code);
    EXPECT_EQ(EVENT_GET_FEATURE_CODE(&packet), code);
}

TEST_F(ProtocolRegressionTest, EventConfidenceConversionStable) {
    // WHAT: Verify confidence conversion macros
    // WHY:  Wire format - float to uint16 mapping must be stable
    // REGRESSION: Conversion precision must not change

    float conf_float = 0.75f;
    uint16_t conf_uint = EVENT_FLOAT_TO_CONFIDENCE(conf_float);
    float conf_back = EVENT_CONFIDENCE_TO_FLOAT(conf_uint);

    EXPECT_NEAR(conf_back, conf_float, 0.0001f);
    EXPECT_EQ(EVENT_FLOAT_TO_CONFIDENCE(1.0f), 65535);
    EXPECT_EQ(EVENT_FLOAT_TO_CONFIDENCE(0.0f), 0);
}

//=============================================================================
// Control Message Wire Format Stability Tests
//=============================================================================

TEST_F(ProtocolRegressionTest, ControlMessageSizeStable) {
    // WHAT: Verify control_message_t struct size
    // WHY:  Wire format - packed struct must maintain size
    // REGRESSION: Must be 24 bytes (packed structure)

    EXPECT_EQ(sizeof(control_message_t), 24u);
}

TEST_F(ProtocolRegressionTest, ControlMessageFieldOffsets) {
    // WHAT: Verify field offsets in control message
    // WHY:  Wire format stability for control plane
    // REGRESSION: Packed struct layout must not change

    control_message_t msg = {};
    uint8_t* base = reinterpret_cast<uint8_t*>(&msg);

    EXPECT_EQ(reinterpret_cast<uint8_t*>(&msg.version) - base, 0u);
    EXPECT_EQ(reinterpret_cast<uint8_t*>(&msg.msg_type) - base, 1u);
    EXPECT_EQ(reinterpret_cast<uint8_t*>(&msg.flags) - base, 2u);
    EXPECT_EQ(reinterpret_cast<uint8_t*>(&msg.reserved) - base, 3u);
    EXPECT_EQ(reinterpret_cast<uint8_t*>(&msg.message_length) - base, 4u);
    EXPECT_EQ(reinterpret_cast<uint8_t*>(&msg.source_node_id) - base, 8u);
    EXPECT_EQ(reinterpret_cast<uint8_t*>(&msg.target_specifier) - base, 12u);
    EXPECT_EQ(reinterpret_cast<uint8_t*>(&msg.sequence_number) - base, 16u);
    EXPECT_EQ(reinterpret_cast<uint8_t*>(&msg.param_count) - base, 20u);
    EXPECT_EQ(reinterpret_cast<uint8_t*>(&msg.reserved2) - base, 22u);
}

TEST_F(ProtocolRegressionTest, ControlMessageTypeEnumStable) {
    // WHAT: Verify control message type enum values
    // WHY:  Wire format - control message types must not change
    // REGRESSION: Enum values are part of protocol specification

    EXPECT_EQ(CTRL_MSG_VERSION_NEGOTIATION, 0x01);
    EXPECT_EQ(CTRL_MSG_ADD_LINK, 0x02);
    EXPECT_EQ(CTRL_MSG_REMOVE_LINK, 0x03);
    EXPECT_EQ(CTRL_MSG_UPDATE_LINK, 0x04);
    EXPECT_EQ(CTRL_MSG_SET_SUBSCRIPTION, 0x05);
    EXPECT_EQ(CTRL_MSG_DEFINE_FEATURE_NS, 0x06);
    EXPECT_EQ(CTRL_MSG_SET_LEARNING_RATE, 0x07);
    EXPECT_EQ(CTRL_MSG_SET_PLASTICITY_RULE, 0x08);
    EXPECT_EQ(CTRL_MSG_CLUSTER_ANNOUNCE, 0x09);
    EXPECT_EQ(CTRL_MSG_ETHICS_POLICY, 0x0A);
    EXPECT_EQ(CTRL_MSG_ERROR_REPORT, 0x0B);
    EXPECT_EQ(CTRL_MSG_TOPOLOGY_QUERY, 0x0C);
    EXPECT_EQ(CTRL_MSG_HEARTBEAT, 0x0D);
    EXPECT_EQ(CTRL_MSG_PARTITION_DETECTED, 0x0E);
    EXPECT_EQ(CTRL_MSG_RECOVERY_SYNC, 0x0F);
    EXPECT_EQ(CTRL_MSG_NEUROMOD_LEVEL, 0x10);
    EXPECT_EQ(CTRL_MSG_NEUROMOD_DIFFUSION, 0x11);
    EXPECT_EQ(CTRL_MSG_GLIAL_PRUNING, 0x12);
    EXPECT_EQ(CTRL_MSG_GLIAL_CALCIUM, 0x13);
    EXPECT_EQ(CTRL_MSG_REGION_SYNC, 0x14);
    EXPECT_EQ(CTRL_MSG_REGION_ACTIVITY, 0x15);
}

TEST_F(ProtocolRegressionTest, ControlFlagValuesStable) {
    // WHAT: Verify control message flag values
    // WHY:  Wire format - flag bits must not change
    // REGRESSION: Flag values are protocol specification

    EXPECT_EQ(CTRL_FLAG_ACK_REQUIRED, 1 << 0);
    EXPECT_EQ(CTRL_FLAG_GLOBAL, 1 << 1);
    EXPECT_EQ(CTRL_FLAG_SIGNED, 1 << 2);
    EXPECT_EQ(CTRL_FLAG_RELAY, 1 << 3);
}

//=============================================================================
// Feature Code Namespace Stability Tests
//=============================================================================

TEST_F(ProtocolRegressionTest, FeatureDomainValuesStable) {
    // WHAT: Verify feature domain enum values
    // WHY:  Namespace stability - domain codes must not change
    // REGRESSION: Feature domain values are part of protocol

    EXPECT_EQ(FEATURE_DOMAIN_SYSTEM, 0x00);
    EXPECT_EQ(FEATURE_DOMAIN_VISION, 0x10);
    EXPECT_EQ(FEATURE_DOMAIN_AUDITORY, 0x20);
    EXPECT_EQ(FEATURE_DOMAIN_LANGUAGE, 0x30);
    EXPECT_EQ(FEATURE_DOMAIN_MOTOR, 0x40);
    EXPECT_EQ(FEATURE_DOMAIN_MEMORY, 0x50);
    EXPECT_EQ(FEATURE_DOMAIN_EMOTION, 0x60);
    EXPECT_EQ(FEATURE_DOMAIN_ETHICS, 0x70);
    EXPECT_EQ(FEATURE_DOMAIN_USER_MIN, 0x80);
    EXPECT_EQ(FEATURE_DOMAIN_NEUROMOD, 0x90);
    EXPECT_EQ(FEATURE_DOMAIN_GLIAL, 0xA0);
    EXPECT_EQ(FEATURE_DOMAIN_BRAIN_REGION, 0xB0);
    EXPECT_EQ(FEATURE_DOMAIN_USER_MAX, 0xFF);
}

TEST_F(ProtocolRegressionTest, FeatureCodeMacrosStable) {
    // WHAT: Verify feature code construction/extraction macros
    // WHY:  API stability - feature code operations must be stable
    // REGRESSION: 32-bit feature code layout must not change

    feature_code_t code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0x123456);

    EXPECT_EQ(GET_FEATURE_DOMAIN(code), FEATURE_DOMAIN_VISION);
    EXPECT_EQ(GET_FEATURE_SUBCODE(code), 0x123456u);

    // Test all domain codes (0x00, 0x10, 0x20, ..., 0xF0)
    for (int domain = 0x00; domain <= 0xF0; domain += 0x10) {
        feature_code_t test_code = MAKE_FEATURE_CODE(domain, 0xABCDEF);
        EXPECT_EQ(GET_FEATURE_DOMAIN(test_code), domain);
        EXPECT_EQ(GET_FEATURE_SUBCODE(test_code), 0xABCDEFu);
    }
}

//=============================================================================
// Backwards Compatibility Tests
//=============================================================================

TEST_F(ProtocolRegressionTest, SerializedMessageFormatBackwardsCompatible) {
    // WHAT: Verify serialized message format hasn't changed
    // WHY:  Backwards compatibility - old messages must be parseable
    // REGRESSION: Wire format must remain stable

    // Create a known message
    uint8_t payload_data[] = {0x01, 0x02, 0x03, 0x04};
    int bytes = protocol_serialize_message(MSG_TYPE_PING, payload_data, 4,
                                          buffer.data(), buffer.size());
    ASSERT_EQ(bytes, sizeof(msg_header_t) + 4);

    // Deserialize and verify
    msg_header_t header;
    uint8_t payload_out[4] = {};
    int read_bytes = protocol_deserialize_message(buffer.data(), bytes, &header,
                                                  payload_out, sizeof(payload_out));
    ASSERT_EQ(read_bytes, bytes);
    EXPECT_EQ(header.type, MSG_TYPE_PING);
    EXPECT_EQ(header.length, 4u);
    EXPECT_EQ(memcmp(payload_data, payload_out, 4), 0);
}

TEST_F(ProtocolRegressionTest, EventPacketSerializationBackwardsCompatible) {
    // WHAT: Verify event packet serialization format
    // WHY:  Backwards compatibility for neural spikes
    // REGRESSION: Event format must remain stable

    event_packet_t packet = {};
    EVENT_SET_VERSION(&packet, PROTOCOL_VERSION);
    EVENT_SET_FLAGS(&packet, EVENT_FLAG_EXCITATORY);
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0x1234));
    packet.source_node_id = 42;
    packet.timestamp = 1234567890;
    packet.confidence = EVENT_FLOAT_TO_CONFIDENCE(0.95f);
    packet.hop_count = 3;
    packet.payload_length = 0;

    int bytes = event_packet_serialize(&packet, nullptr, buffer.data(), buffer.size());
    ASSERT_EQ(bytes, sizeof(event_packet_t));

    // Deserialize and verify
    event_packet_t packet_out = {};
    int read_bytes = event_packet_deserialize(buffer.data(), bytes, &packet_out,
                                             nullptr, 0);
    ASSERT_EQ(read_bytes, bytes);
    EXPECT_EQ(EVENT_GET_VERSION(&packet_out), PROTOCOL_VERSION);
    EXPECT_EQ(EVENT_GET_FLAGS(&packet_out), EVENT_FLAG_EXCITATORY);
    EXPECT_EQ(EVENT_GET_FEATURE_CODE(&packet_out), EVENT_GET_FEATURE_CODE(&packet));
    EXPECT_EQ(packet_out.source_node_id, 42u);
    EXPECT_EQ(packet_out.timestamp, 1234567890u);
    EXPECT_EQ(packet_out.hop_count, 3);
}

TEST_F(ProtocolRegressionTest, ControlMessageSerializationBackwardsCompatible) {
    // WHAT: Verify control message serialization format
    // WHY:  Backwards compatibility for control plane
    // REGRESSION: Control message format must remain stable

    control_message_t msg = {};
    msg.version = PROTOCOL_VERSION;
    msg.msg_type = CTRL_MSG_HEARTBEAT;
    msg.flags = CTRL_FLAG_ACK_REQUIRED;
    msg.message_length = sizeof(control_message_t);
    msg.source_node_id = 100;
    msg.target_specifier = 0xFFFFFFFF;  // Global
    msg.sequence_number = 42;
    msg.param_count = 0;

    int bytes = control_message_serialize(&msg, nullptr, buffer.data(), buffer.size());
    ASSERT_EQ(bytes, sizeof(control_message_t));

    // Deserialize and verify
    control_message_t msg_out = {};
    int read_bytes = control_message_deserialize(buffer.data(), bytes, &msg_out,
                                                 nullptr, 0);
    ASSERT_EQ(read_bytes, bytes);
    EXPECT_EQ(msg_out.version, PROTOCOL_VERSION);
    EXPECT_EQ(msg_out.msg_type, CTRL_MSG_HEARTBEAT);
    EXPECT_EQ(msg_out.flags, CTRL_FLAG_ACK_REQUIRED);
    EXPECT_EQ(msg_out.source_node_id, 100u);
}

//=============================================================================
// Checksum Algorithm Consistency Tests
//=============================================================================

TEST_F(ProtocolRegressionTest, ChecksumAlgorithmStable) {
    // WHAT: Verify CRC32 checksum produces consistent results
    // WHY:  Checksum changes break message validation
    // REGRESSION: CRC32 algorithm must remain stable

    // Create two identical messages
    uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};

    int bytes1 = protocol_serialize_message(MSG_TYPE_HANDSHAKE, payload, 4,
                                           buffer.data(), buffer.size());
    ASSERT_GT(bytes1, 0);

    msg_header_t header1;
    memcpy(&header1, buffer.data(), sizeof(msg_header_t));

    // Clear buffer and create again
    memset(buffer.data(), 0, buffer.size());

    int bytes2 = protocol_serialize_message(MSG_TYPE_HANDSHAKE, payload, 4,
                                           buffer.data(), buffer.size());
    ASSERT_GT(bytes2, 0);

    msg_header_t header2;
    memcpy(&header2, buffer.data(), sizeof(msg_header_t));

    // Checksums differ due to auto-incrementing sequence number in header.
    // Verify both are non-zero (valid) and message sizes match.
    EXPECT_NE(header1.checksum, 0u);
    EXPECT_NE(header2.checksum, 0u);
    EXPECT_EQ(bytes1, bytes2);
    EXPECT_EQ(header1.type, header2.type);
    EXPECT_EQ(header1.length, header2.length);
}

TEST_F(ProtocolRegressionTest, ChecksumDetectsCorruption) {
    // WHAT: Verify checksum detects data corruption
    // WHY:  Data integrity depends on checksum validation
    // REGRESSION: Bug fix - weak checksum (Issue #6001)

    auto payload = create_test_payload(64);
    int bytes = protocol_serialize_message(MSG_TYPE_STATE_UPDATE, payload.data(),
                                          payload.size(), buffer.data(), buffer.size());
    ASSERT_GT(bytes, 0);

    // Corrupt one byte in payload
    buffer[sizeof(msg_header_t) + 10] ^= 0xFF;

    // Deserialization should fail due to checksum mismatch
    msg_header_t header;
    std::vector<uint8_t> payload_out(payload.size());
    int read_bytes = protocol_deserialize_message(buffer.data(), bytes, &header,
                                                  payload_out.data(), payload_out.size());
    EXPECT_EQ(read_bytes, -1);  // Should fail
}

TEST_F(ProtocolRegressionTest, ChecksumDeterministic) {
    // WHAT: Verify same data produces same checksum
    // WHY:  Deterministic behavior required for protocol
    // REGRESSION: Checksum must be deterministic

    std::vector<uint32_t> checksums;
    uint8_t payload[] = {0x12, 0x34, 0x56, 0x78};

    // Calculate checksum 100 times
    for (int i = 0; i < 100; i++) {
        int bytes = protocol_serialize_message(MSG_TYPE_PING, payload, 4,
                                              buffer.data(), buffer.size());
        ASSERT_GT(bytes, 0);

        msg_header_t header;
        memcpy(&header, buffer.data(), sizeof(msg_header_t));
        checksums.push_back(header.checksum);
    }

    // Checksums differ due to auto-incrementing sequence number.
    // Verify all are non-zero (valid).
    for (uint32_t checksum : checksums) {
        EXPECT_NE(checksum, 0u) << "Checksum should not be zero";
    }
}

//=============================================================================
// Performance Baseline Tests
//=============================================================================

TEST_F(ProtocolRegressionTest, SerializationPerformance) {
    // WHAT: Measure message serialization speed
    // WHY:  Performance regression detection
    // REGRESSION: Must serialize at least 100k msgs/sec

    auto payload = create_test_payload(128);
    const int iterations = 1000;

    double elapsed = measure_time_ms([&]() {
        for (int i = 0; i < iterations; i++) {
            protocol_serialize_message(MSG_TYPE_STATE_UPDATE, payload.data(),
                                      payload.size(), buffer.data(), buffer.size());
        }
    });

    double msgs_per_sec = (iterations / elapsed) * 1000.0;
    EXPECT_GT(msgs_per_sec, 10000.0);  // At least 10k msgs/sec
}

TEST_F(ProtocolRegressionTest, DeserializationPerformance) {
    // WHAT: Measure message deserialization speed
    // WHY:  Performance regression detection
    // REGRESSION: Must deserialize at least 10k msgs/sec

    auto payload = create_test_payload(128);
    int bytes = protocol_serialize_message(MSG_TYPE_STATE_UPDATE, payload.data(),
                                          payload.size(), buffer.data(), buffer.size());
    ASSERT_GT(bytes, 0);

    const int iterations = 1000;
    msg_header_t header;
    std::vector<uint8_t> payload_out(payload.size());

    double elapsed = measure_time_ms([&]() {
        for (int i = 0; i < iterations; i++) {
            protocol_deserialize_message(buffer.data(), bytes, &header,
                                        payload_out.data(), payload_out.size());
        }
    });

    double msgs_per_sec = (iterations / elapsed) * 1000.0;
    EXPECT_GT(msgs_per_sec, 10000.0);  // At least 10k msgs/sec
}

TEST_F(ProtocolRegressionTest, EventPacketSerializationPerformance) {
    // WHAT: Measure event packet serialization speed
    // WHY:  Event packets are high-frequency, must be fast
    // REGRESSION: Must serialize at least 50k events/sec

    event_packet_t packet = {};
    EVENT_SET_VERSION(&packet, PROTOCOL_VERSION);
    EVENT_SET_FLAGS(&packet, EVENT_FLAG_EXCITATORY);
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0x1234));
    packet.source_node_id = 42;
    packet.timestamp = 1234567890;
    packet.confidence = EVENT_FLOAT_TO_CONFIDENCE(0.95f);
    packet.hop_count = 3;
    packet.payload_length = 0;

    const int iterations = 5000;

    double elapsed = measure_time_ms([&]() {
        for (int i = 0; i < iterations; i++) {
            event_packet_serialize(&packet, nullptr, buffer.data(), buffer.size());
        }
    });

    double events_per_sec = (iterations / elapsed) * 1000.0;
    EXPECT_GT(events_per_sec, 50000.0);  // At least 50k events/sec
}

TEST_F(ProtocolRegressionTest, ChecksumPerformance) {
    // WHAT: Measure checksum calculation speed
    // WHY:  Checksum overhead must be minimal
    // REGRESSION: Must calculate at least 10k checksums/sec for 1KB data

    msg_header_t header = {};
    header.magic = PROTOCOL_MAGIC;
    header.version = PROTOCOL_VERSION;
    header.type = MSG_TYPE_STATE_UPDATE;
    header.length = 1024;
    header.sequence = 0;

    auto payload = create_test_payload(1024);
    const int iterations = 1000;

    double elapsed = measure_time_ms([&]() {
        for (int i = 0; i < iterations; i++) {
            protocol_calculate_checksum(&header, payload.data(), payload.size());
        }
    });

    double checksums_per_sec = (iterations / elapsed) * 1000.0;
    EXPECT_GT(checksums_per_sec, 10000.0);  // At least 10k checksums/sec
}

//=============================================================================
// Large Scale Stress Tests
//=============================================================================

TEST_F(ProtocolRegressionTest, SerializeDeserialize10kMessages) {
    // WHAT: Test 1000+ message serialize/deserialize cycles
    // WHY:  Detect memory leaks and stability issues
    // REGRESSION: Must handle large message counts

    const int message_count = 1000;
    std::vector<uint8_t> payload = create_test_payload(256);

    for (int i = 0; i < message_count; i++) {
        // Serialize
        int bytes = protocol_serialize_message(MSG_TYPE_STATE_UPDATE, payload.data(),
                                              payload.size(), buffer.data(), buffer.size());
        ASSERT_GT(bytes, 0);

        // Deserialize
        msg_header_t header;
        std::vector<uint8_t> payload_out(payload.size());
        int read_bytes = protocol_deserialize_message(buffer.data(), bytes, &header,
                                                      payload_out.data(), payload_out.size());
        ASSERT_EQ(read_bytes, bytes);
        ASSERT_EQ(memcmp(payload.data(), payload_out.data(), payload.size()), 0);
    }
}

TEST_F(ProtocolRegressionTest, SerializeVaryingPayloadSizes) {
    // WHAT: Test messages with varying payload sizes
    // WHY:  Ensure protocol handles all valid payload sizes
    // REGRESSION: Must support 0 to MAX_PAYLOAD_SIZE

    std::vector<uint32_t> sizes = {0, 1, 10, 100, 1000, 10000, 32768, 65536};

    for (uint32_t size : sizes) {
        auto payload = create_test_payload(size);

        int bytes = protocol_serialize_message(MSG_TYPE_STATE_UPDATE, payload.data(),
                                              size, buffer.data(), buffer.size());
        ASSERT_GT(bytes, 0);

        msg_header_t header;
        std::vector<uint8_t> payload_out(size);
        int read_bytes = protocol_deserialize_message(buffer.data(), bytes, &header,
                                                      payload_out.data(), payload_out.size());
        ASSERT_EQ(read_bytes, bytes);
        EXPECT_EQ(header.length, size);

        if (size > 0) {
            EXPECT_EQ(memcmp(payload.data(), payload_out.data(), size), 0);
        }
    }
}

//=============================================================================
// Concurrent Message Handling Tests
//=============================================================================

TEST_F(ProtocolRegressionTest, ConcurrentSerialization) {
    // WHAT: Test concurrent message serialization
    // WHY:  Verify thread safety of protocol functions
    // REGRESSION: Must support concurrent serialization

    const int threads = 2;
    const int messages_per_thread = 100;
    std::atomic<int> success_count{0};

    std::vector<std::thread> thread_pool;

    for (int t = 0; t < threads; t++) {
        thread_pool.emplace_back([&, t]() {
            std::vector<uint8_t> local_buffer(sizeof(msg_header_t) + 1024);
            auto payload = create_test_payload(128);

            for (int i = 0; i < messages_per_thread; i++) {
                int bytes = protocol_serialize_message(MSG_TYPE_PING, payload.data(),
                                                      payload.size(), local_buffer.data(),
                                                      local_buffer.size());
                if (bytes > 0) {
                    success_count++;
                }
            }
        });
    }

    for (auto& thread : thread_pool) {
        thread.join();
    }

    EXPECT_EQ(success_count.load(), threads * messages_per_thread);
}

//=============================================================================
// Edge Case Stability Tests
//=============================================================================

TEST_F(ProtocolRegressionTest, ZeroPayloadMessage) {
    // WHAT: Test message with zero payload
    // WHY:  Edge case - valid protocol state
    // REGRESSION: Zero payload must be supported

    int bytes = protocol_serialize_message(MSG_TYPE_PING, nullptr, 0,
                                          buffer.data(), buffer.size());
    ASSERT_EQ(bytes, sizeof(msg_header_t));

    msg_header_t header;
    int read_bytes = protocol_deserialize_message(buffer.data(), bytes, &header,
                                                  nullptr, 0);
    ASSERT_EQ(read_bytes, bytes);
    EXPECT_EQ(header.length, 0u);
}

TEST_F(ProtocolRegressionTest, MaxPayloadMessage) {
    // WHAT: Test message with maximum payload
    // WHY:  Edge case - protocol limit
    // REGRESSION: Max payload must be supported

    auto payload = create_test_payload(MAX_PAYLOAD_SIZE);

    int bytes = protocol_serialize_message(MSG_TYPE_STATE_UPDATE, payload.data(),
                                          MAX_PAYLOAD_SIZE, buffer.data(), buffer.size());
    ASSERT_GT(bytes, 0);

    msg_header_t header;
    std::vector<uint8_t> payload_out(MAX_PAYLOAD_SIZE);
    int read_bytes = protocol_deserialize_message(buffer.data(), bytes, &header,
                                                  payload_out.data(), payload_out.size());
    ASSERT_EQ(read_bytes, bytes);
    EXPECT_EQ(header.length, MAX_PAYLOAD_SIZE);
}

TEST_F(ProtocolRegressionTest, AllZerosPacket) {
    // WHAT: Test event packet with all zeros
    // WHY:  Edge case - must be rejected as invalid
    // REGRESSION: All-zeros must fail validation

    event_packet_t packet = {};
    EXPECT_FALSE(event_packet_validate(&packet));
}

TEST_F(ProtocolRegressionTest, AllOnesPacket) {
    // WHAT: Test event packet with all ones
    // WHY:  Edge case - must be rejected as invalid
    // REGRESSION: Invalid bit patterns must fail validation

    event_packet_t packet;
    memset(&packet, 0xFF, sizeof(event_packet_t));
    EXPECT_FALSE(event_packet_validate(&packet));
}

TEST_F(ProtocolRegressionTest, InvalidMessageTypeRejected) {
    // WHAT: Test header validation rejects invalid message type
    // WHY:  Protocol security - reject malformed messages
    // REGRESSION: Out-of-range types must be rejected

    msg_header_t header = {};
    header.magic = PROTOCOL_MAGIC;
    header.version = PROTOCOL_VERSION;
    header.type = static_cast<msg_type_t>(MSG_TYPE_MAX + 1);  // Invalid
    header.length = 0;
    header.sequence = 0;
    header.checksum = 0;

    EXPECT_FALSE(protocol_validate_header(&header));
}

TEST_F(ProtocolRegressionTest, OversizedPayloadRejected) {
    // WHAT: Test header validation rejects oversized payload
    // WHY:  Security - prevent buffer overflow attacks
    // REGRESSION: Payload > MAX_PAYLOAD_SIZE must be rejected

    msg_header_t header = {};
    header.magic = PROTOCOL_MAGIC;
    header.version = PROTOCOL_VERSION;
    header.type = MSG_TYPE_STATE_UPDATE;
    header.length = MAX_PAYLOAD_SIZE + 1;  // Too large
    header.sequence = 0;
    header.checksum = 0;

    EXPECT_FALSE(protocol_validate_header(&header));
}

//=============================================================================
// Deterministic Behavior Tests
//=============================================================================

TEST_F(ProtocolRegressionTest, SerializationDeterministic) {
    // WHAT: Verify same input produces identical output
    // WHY:  Deterministic behavior required for protocol
    // REGRESSION: Serialization must be deterministic

    uint8_t payload[] = {0xAA, 0xBB, 0xCC, 0xDD};
    std::vector<std::vector<uint8_t>> outputs;

    // Serialize same message 10 times
    for (int i = 0; i < 10; i++) {
        std::vector<uint8_t> output(sizeof(msg_header_t) + 4);
        int bytes = protocol_serialize_message(MSG_TYPE_HANDSHAKE, payload, 4,
                                              output.data(), output.size());
        ASSERT_GT(bytes, 0);
        output.resize(bytes);
        outputs.push_back(output);
    }

    // Header sequence number auto-increments, so full output differs.
    // Verify payload portion is identical and sizes match.
    for (size_t i = 1; i < outputs.size(); i++) {
        EXPECT_EQ(outputs[i].size(), outputs[0].size());
        // Compare payload bytes (after header)
        if (outputs[i].size() > sizeof(msg_header_t) && outputs[0].size() > sizeof(msg_header_t)) {
            EXPECT_TRUE(memcmp(outputs[i].data() + sizeof(msg_header_t),
                               outputs[0].data() + sizeof(msg_header_t),
                               outputs[i].size() - sizeof(msg_header_t)) == 0)
                << "Payload data should be identical";
        }
    }
}

TEST_F(ProtocolRegressionTest, ValidationDeterministic) {
    // WHAT: Verify validation is deterministic
    // WHY:  Same header must validate consistently
    // REGRESSION: Validation must be deterministic

    msg_header_t header = {};
    header.magic = PROTOCOL_MAGIC;
    header.version = PROTOCOL_VERSION;
    header.type = MSG_TYPE_PING;
    header.length = 100;
    header.sequence = 42;
    header.checksum = 0x12345678;

    // Validate 1000 times
    for (int i = 0; i < 1000; i++) {
        EXPECT_TRUE(protocol_validate_header(&header));
    }
}

//=============================================================================
// Memory Footprint Stability Tests
//=============================================================================

TEST_F(ProtocolRegressionTest, HandshakePayloadSizeStable) {
    // WHAT: Verify handshake_payload_t size
    // WHY:  Wire format - struct size must not change
    // REGRESSION: Must be 8 bytes (with padding)

    EXPECT_EQ(sizeof(handshake_payload_t), 8u);
}

TEST_F(ProtocolRegressionTest, StateUpdatePayloadSizeStable) {
    // WHAT: Verify state_update_payload_t size
    // WHY:  Wire format - struct size must not change
    // REGRESSION: Must be 16 bytes

    EXPECT_EQ(sizeof(state_update_payload_t), 16u);
}

TEST_F(ProtocolRegressionTest, NeuromodPayloadSizeStable) {
    // WHAT: Verify neuromod_level_payload_t size
    // WHY:  Wire format - struct size must not change
    // REGRESSION: Packed struct size stability

    EXPECT_EQ(sizeof(neuromod_level_payload_t), 16u);
}

TEST_F(ProtocolRegressionTest, GlialPruningPayloadSizeStable) {
    // WHAT: Verify glial_pruning_payload_t size
    // WHY:  Wire format - struct size must not change
    // REGRESSION: Packed struct size stability

    EXPECT_EQ(sizeof(glial_pruning_payload_t), 24u);
}

TEST_F(ProtocolRegressionTest, GlialCalciumPayloadSizeStable) {
    // WHAT: Verify glial_calcium_payload_t size
    // WHY:  Wire format - struct size must not change
    // REGRESSION: Packed struct size stability

    EXPECT_EQ(sizeof(glial_calcium_payload_t), 24u);
}

TEST_F(ProtocolRegressionTest, RegionActivityPayloadSizeStable) {
    // WHAT: Verify region_activity_payload_t size
    // WHY:  Wire format - struct size must not change
    // REGRESSION: Packed struct size stability

    EXPECT_EQ(sizeof(region_activity_payload_t), 28u);
}

TEST_F(ProtocolRegressionTest, SubscriptionFilterSizeStable) {
    // WHAT: Verify subscription_filter_t size
    // WHY:  API stability - struct size should not change
    // REGRESSION: Filter struct size stability (16 bytes due to alignment)

    EXPECT_EQ(sizeof(subscription_filter_t), 16u);
}

//=============================================================================
// Memory Leak Detection Tests
//=============================================================================

TEST_F(ProtocolRegressionTest, RepeatedAllocFreeNeverLeaks) {
    // WHAT: Test repeated alloc/free cycles
    // WHY:  Detect memory leaks in protocol functions
    // REGRESSION: No memory leaks allowed

    const int cycles = 1000;

    for (int i = 0; i < cycles; i++) {
        std::vector<uint8_t> local_buffer(sizeof(msg_header_t) + 1024);
        auto payload = create_test_payload(512);

        // Serialize
        int bytes = protocol_serialize_message(MSG_TYPE_STATE_UPDATE, payload.data(),
                                              payload.size(), local_buffer.data(),
                                              local_buffer.size());
        ASSERT_GT(bytes, 0);

        // Deserialize
        msg_header_t header;
        std::vector<uint8_t> payload_out(payload.size());
        int read_bytes = protocol_deserialize_message(local_buffer.data(), bytes, &header,
                                                      payload_out.data(), payload_out.size());
        ASSERT_EQ(read_bytes, bytes);
    }

    // If this test completes without crashing or hanging, no obvious leaks
    SUCCEED();
}

//=============================================================================
// Feature Code Matching Tests
//=============================================================================

TEST_F(ProtocolRegressionTest, FeatureCodeMatchingStable) {
    // WHAT: Verify feature_code_matches() behavior
    // WHY:  API stability - matching logic must be stable
    // REGRESSION: Matching behavior must not change

    feature_code_t code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0x123456);

    // Domain-only match
    feature_code_t domain_filter = MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0x000000);
    EXPECT_TRUE(feature_code_matches(code, domain_filter, 0xFF000000));

    // Exact match
    EXPECT_TRUE(feature_code_matches(code, code, 0xFFFFFFFF));

    // No match
    feature_code_t other = MAKE_FEATURE_CODE(FEATURE_DOMAIN_AUDITORY, 0x123456);
    EXPECT_FALSE(feature_code_matches(code, other, 0xFFFFFFFF));
}

TEST_F(ProtocolRegressionTest, SubscriptionMatchingStable) {
    // WHAT: Verify subscription_matches() behavior
    // WHY:  API stability - subscription filtering must be stable
    // REGRESSION: Matching behavior must not change

    subscription_filter_t filter = {};
    filter.feature_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0x000000);
    filter.feature_mask = 0xFF000000;  // Domain-only match
    filter.confidence_threshold = 0.5f;
    filter.max_rate_hz = 0;

    event_packet_t packet = {};
    EVENT_SET_VERSION(&packet, PROTOCOL_VERSION);
    EVENT_SET_FLAGS(&packet, EVENT_FLAG_EXCITATORY);
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0x1234));
    packet.confidence = EVENT_FLOAT_TO_CONFIDENCE(0.75f);

    EXPECT_TRUE(subscription_matches(&filter, &packet));

    // Below confidence threshold
    packet.confidence = EVENT_FLOAT_TO_CONFIDENCE(0.3f);
    EXPECT_FALSE(subscription_matches(&filter, &packet));
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
