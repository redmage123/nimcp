/**
 * @file test_protocol_event_packet.cpp
 * @brief Comprehensive test suite for NIMCP 2.0 Event Packet functions
 * @details Tests serialization, deserialization, validation, macros, and edge cases
 *
 * This test suite covers:
 * - event_packet_serialize() with various payloads
 * - event_packet_deserialize() with valid/invalid data
 * - event_packet_validate() with edge cases
 * - All EVENT_* macros (GET_VERSION, GET_FLAGS, SET_VERSION, SET_FLAGS)
 * - EVENT_GET_FEATURE_CODE and EVENT_SET_FEATURE_CODE macros
 * - EVENT_CONFIDENCE_TO_FLOAT and EVENT_FLOAT_TO_CONFIDENCE macros
 * - Feature code packing/unpacking in packets
 * - Excitatory/Inhibitory/Plasticity flags
 * - Hop count and TTL behavior
 * - Boundary conditions (confidence 0.0-1.0, timestamp overflow, max hop count)
 * - Serialization with and without payloads
 * - Buffer overflow protection
 * - NULL pointer safety
 */

#include <string.h>
#include <limits.h>
#include "networking/protocol/nimcp_protocol.h"
#include "test_helpers.h"

//=============================================================================
// Test Fixture Classes
//=============================================================================

class EventPacketTest : public ::testing::Test {
   protected:
    event_packet_t packet;
    uint8_t buffer[4096];
    uint8_t payload_buffer[2048];

    void SetUp() override {
        memset(&packet, 0, sizeof(event_packet_t));
        memset(buffer, 0, sizeof(buffer));
        memset(payload_buffer, 0, sizeof(payload_buffer));
    }

    void TearDown() override {
        // Clean up any resources if needed
    }

    // Helper to create a valid event packet
    void create_valid_packet() {
        EVENT_SET_VERSION(&packet, PROTOCOL_VERSION);
        EVENT_SET_FLAGS(&packet, EVENT_FLAG_EXCITATORY);
        EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0x1234));
        packet.source_node_id = 12345;
        packet.timestamp = 1000000;
        packet.confidence = EVENT_FLOAT_TO_CONFIDENCE(0.75f);
        packet.hop_count = 0;
        packet.payload_length = 0;
    }

    // Helper to create test payload
    void create_test_payload(uint8_t* buf, size_t size) {
        for (size_t i = 0; i < size; i++) {
            buf[i] = (uint8_t)(i & 0xFF);
        }
    }
};

class EventPacketSerializationTest : public EventPacketTest {};
class EventPacketDeserializationTest : public EventPacketTest {};
class EventPacketValidationTest : public EventPacketTest {};
class EventPacketMacroTest : public EventPacketTest {};
class EventPacketFeatureCodeTest : public EventPacketTest {};
class EventPacketConfidenceTest : public EventPacketTest {};
class EventPacketBoundaryTest : public EventPacketTest {};
class EventPacketFlagTest : public EventPacketTest {};

//=============================================================================
// Serialization Tests
//=============================================================================

TEST_F(EventPacketSerializationTest, BasicSerializeWithoutPayload)
{
    create_valid_packet();

    int result = event_packet_serialize(&packet, nullptr, buffer, sizeof(buffer));

    ASSERT_GT(result, 0);
    ASSERT_EQ(result, sizeof(event_packet_t));

    // Verify packet was copied to buffer
    event_packet_t* serialized = (event_packet_t*)buffer;
    ASSERT_EQ(EVENT_GET_VERSION(serialized), PROTOCOL_VERSION);
    ASSERT_EQ(serialized->source_node_id, 12345);
}

TEST_F(EventPacketSerializationTest, SerializeWithSmallPayload)
{
    create_valid_packet();
    uint8_t payload[64];
    create_test_payload(payload, sizeof(payload));
    packet.payload_length = sizeof(payload);

    int result = event_packet_serialize(&packet, payload, buffer, sizeof(buffer));

    ASSERT_GT(result, 0);
    ASSERT_EQ(result, sizeof(event_packet_t) + sizeof(payload));

    // Verify payload was copied correctly
    const uint8_t* serialized_payload = buffer + sizeof(event_packet_t);
    ASSERT_EQ(memcmp(serialized_payload, payload, sizeof(payload)), 0);
}

TEST_F(EventPacketSerializationTest, SerializeWithLargePayload)
{
    create_valid_packet();
    uint8_t payload[1024];
    create_test_payload(payload, sizeof(payload));
    packet.payload_length = sizeof(payload);

    int result = event_packet_serialize(&packet, payload, buffer, sizeof(buffer));

    ASSERT_GT(result, 0);
    ASSERT_EQ(result, sizeof(event_packet_t) + sizeof(payload));
}

TEST_F(EventPacketSerializationTest, SerializeWithMaxPayload)
{
    create_valid_packet();
    size_t max_size = 2048;
    uint8_t* large_payload = new uint8_t[max_size];
    create_test_payload(large_payload, max_size);
    packet.payload_length = max_size;

    uint8_t* large_buffer = new uint8_t[max_size + sizeof(event_packet_t)];
    int result = event_packet_serialize(&packet, large_payload, large_buffer,
                                       max_size + sizeof(event_packet_t));

    ASSERT_GT(result, 0);
    ASSERT_EQ(result, sizeof(event_packet_t) + max_size);

    delete[] large_payload;
    delete[] large_buffer;
}

TEST_F(EventPacketSerializationTest, SerializeBufferTooSmall)
{
    create_valid_packet();
    uint8_t small_buffer[10];

    int result = event_packet_serialize(&packet, nullptr, small_buffer, sizeof(small_buffer));

    ASSERT_EQ(result, -1);
}

TEST_F(EventPacketSerializationTest, SerializeNullPacket)
{
    int result = event_packet_serialize(nullptr, nullptr, buffer, sizeof(buffer));

    ASSERT_EQ(result, -1);
}

TEST_F(EventPacketSerializationTest, SerializeNullBuffer)
{
    create_valid_packet();

    int result = event_packet_serialize(&packet, nullptr, nullptr, sizeof(buffer));

    ASSERT_EQ(result, -1);
}

TEST_F(EventPacketSerializationTest, SerializePayloadMismatch)
{
    create_valid_packet();
    packet.payload_length = 100;  // Claim 100 bytes

    // Buffer too small for header + claimed payload
    uint8_t small_buffer[sizeof(event_packet_t) + 50];
    int result = event_packet_serialize(&packet, payload_buffer, small_buffer, sizeof(small_buffer));

    ASSERT_EQ(result, -1);
}

//=============================================================================
// Deserialization Tests
//=============================================================================

TEST_F(EventPacketDeserializationTest, BasicDeserializeWithoutPayload)
{
    create_valid_packet();

    // First serialize
    int written = event_packet_serialize(&packet, nullptr, buffer, sizeof(buffer));
    ASSERT_GT(written, 0);

    // Then deserialize
    event_packet_t deserialized;
    int result = event_packet_deserialize(buffer, written, &deserialized, nullptr, 0);

    ASSERT_GT(result, 0);
    ASSERT_EQ(EVENT_GET_VERSION(&deserialized), PROTOCOL_VERSION);
    ASSERT_EQ(deserialized.source_node_id, packet.source_node_id);
    ASSERT_EQ(deserialized.timestamp, packet.timestamp);
    ASSERT_EQ(deserialized.confidence, packet.confidence);
}

TEST_F(EventPacketDeserializationTest, DeserializeWithPayload)
{
    create_valid_packet();
    uint8_t payload[128];
    create_test_payload(payload, sizeof(payload));
    packet.payload_length = sizeof(payload);

    // Serialize
    int written = event_packet_serialize(&packet, payload, buffer, sizeof(buffer));
    ASSERT_GT(written, 0);

    // Deserialize
    event_packet_t deserialized;
    uint8_t deserialized_payload[128];
    int result = event_packet_deserialize(buffer, written, &deserialized,
                                         deserialized_payload, sizeof(deserialized_payload));

    ASSERT_GT(result, 0);
    ASSERT_EQ(result, written);
    ASSERT_EQ(deserialized.payload_length, sizeof(payload));
    ASSERT_EQ(memcmp(deserialized_payload, payload, sizeof(payload)), 0);
}

TEST_F(EventPacketDeserializationTest, DeserializeBufferTooSmall)
{
    create_valid_packet();
    uint8_t small_buffer[10];

    event_packet_t deserialized;
    int result = event_packet_deserialize(small_buffer, sizeof(small_buffer),
                                         &deserialized, nullptr, 0);

    ASSERT_EQ(result, -1);
}

TEST_F(EventPacketDeserializationTest, DeserializeNullBuffer)
{
    event_packet_t deserialized;
    int result = event_packet_deserialize(nullptr, 100, &deserialized, nullptr, 0);

    ASSERT_EQ(result, -1);
}

TEST_F(EventPacketDeserializationTest, DeserializeNullPacket)
{
    create_valid_packet();
    event_packet_serialize(&packet, nullptr, buffer, sizeof(buffer));

    int result = event_packet_deserialize(buffer, sizeof(buffer), nullptr, nullptr, 0);

    ASSERT_EQ(result, -1);
}

TEST_F(EventPacketDeserializationTest, DeserializePayloadBufferTooSmall)
{
    create_valid_packet();
    uint8_t payload[128];
    create_test_payload(payload, sizeof(payload));
    packet.payload_length = sizeof(payload);

    // Serialize with large payload
    int written = event_packet_serialize(&packet, payload, buffer, sizeof(buffer));
    ASSERT_GT(written, 0);

    // Try to deserialize with small payload buffer
    event_packet_t deserialized;
    uint8_t small_payload[64];
    int result = event_packet_deserialize(buffer, written, &deserialized,
                                         small_payload, sizeof(small_payload));

    ASSERT_EQ(result, -1);
}

TEST_F(EventPacketDeserializationTest, DeserializeInvalidPacket)
{
    // Create invalid packet (wrong version)
    create_valid_packet();
    EVENT_SET_VERSION(&packet, 99);  // Invalid version

    event_packet_serialize(&packet, nullptr, buffer, sizeof(buffer));

    event_packet_t deserialized;
    int result = event_packet_deserialize(buffer, sizeof(event_packet_t),
                                         &deserialized, nullptr, 0);

    ASSERT_EQ(result, -1);
}

//=============================================================================
// Validation Tests
//=============================================================================

TEST_F(EventPacketValidationTest, ValidPacket)
{
    create_valid_packet();

    ASSERT_TRUE(event_packet_validate(&packet));
}

TEST_F(EventPacketValidationTest, NullPacket)
{
    ASSERT_FALSE(event_packet_validate(nullptr));
}

TEST_F(EventPacketValidationTest, InvalidVersion)
{
    create_valid_packet();
    EVENT_SET_VERSION(&packet, 99);

    ASSERT_FALSE(event_packet_validate(&packet));
}

TEST_F(EventPacketValidationTest, ExcitatoryFlag)
{
    create_valid_packet();
    EVENT_SET_FLAGS(&packet, EVENT_FLAG_EXCITATORY);

    ASSERT_TRUE(event_packet_validate(&packet));
}

TEST_F(EventPacketValidationTest, InhibitoryFlag)
{
    create_valid_packet();
    EVENT_SET_FLAGS(&packet, EVENT_FLAG_INHIBITORY);

    ASSERT_TRUE(event_packet_validate(&packet));
}

TEST_F(EventPacketValidationTest, BothExcitatoryAndInhibitory)
{
    create_valid_packet();
    EVENT_SET_FLAGS(&packet, EVENT_FLAG_EXCITATORY | EVENT_FLAG_INHIBITORY);

    // Cannot be both excitatory and inhibitory
    ASSERT_FALSE(event_packet_validate(&packet));
}

TEST_F(EventPacketValidationTest, NoTypeFlags)
{
    create_valid_packet();
    EVENT_SET_FLAGS(&packet, 0);  // No flags

    // Invalid - must have exactly one of excitatory/inhibitory (XOR check)
    ASSERT_FALSE(event_packet_validate(&packet));
}

TEST_F(EventPacketValidationTest, PlasticityFlag)
{
    create_valid_packet();
    EVENT_SET_FLAGS(&packet, EVENT_FLAG_EXCITATORY | EVENT_FLAG_PLASTICITY);

    ASSERT_TRUE(event_packet_validate(&packet));
}

TEST_F(EventPacketValidationTest, RouteRecordingFlag)
{
    create_valid_packet();
    EVENT_SET_FLAGS(&packet, EVENT_FLAG_EXCITATORY | EVENT_FLAG_ROUTE_REC);

    ASSERT_TRUE(event_packet_validate(&packet));
}

TEST_F(EventPacketValidationTest, AllValidFlags)
{
    create_valid_packet();
    EVENT_SET_FLAGS(&packet, EVENT_FLAG_EXCITATORY | EVENT_FLAG_PLASTICITY | EVENT_FLAG_ROUTE_REC);

    ASSERT_TRUE(event_packet_validate(&packet));
}

TEST_F(EventPacketValidationTest, PayloadTooLarge)
{
    create_valid_packet();
    packet.payload_length = MAX_PAYLOAD_SIZE + 1;

    ASSERT_FALSE(event_packet_validate(&packet));
}

TEST_F(EventPacketValidationTest, MaxPayloadSize)
{
    create_valid_packet();
    packet.payload_length = MAX_PAYLOAD_SIZE;

    ASSERT_TRUE(event_packet_validate(&packet));
}

//=============================================================================
// Macro Tests - Version and Flags
//=============================================================================

TEST_F(EventPacketMacroTest, GetVersionMacro)
{
    create_valid_packet();

    uint8_t version = EVENT_GET_VERSION(&packet);

    ASSERT_EQ(version, PROTOCOL_VERSION);
}

TEST_F(EventPacketMacroTest, SetVersionMacro)
{
    memset(&packet, 0, sizeof(packet));

    EVENT_SET_VERSION(&packet, PROTOCOL_VERSION);

    ASSERT_EQ(EVENT_GET_VERSION(&packet), PROTOCOL_VERSION);
}

TEST_F(EventPacketMacroTest, GetFlagsMacro)
{
    create_valid_packet();
    EVENT_SET_FLAGS(&packet, EVENT_FLAG_EXCITATORY | EVENT_FLAG_PLASTICITY);

    uint8_t flags = EVENT_GET_FLAGS(&packet);

    ASSERT_EQ(flags, EVENT_FLAG_EXCITATORY | EVENT_FLAG_PLASTICITY);
}

TEST_F(EventPacketMacroTest, SetFlagsMacro)
{
    create_valid_packet();

    EVENT_SET_FLAGS(&packet, EVENT_FLAG_INHIBITORY | EVENT_FLAG_ROUTE_REC);

    uint8_t flags = EVENT_GET_FLAGS(&packet);
    ASSERT_EQ(flags, EVENT_FLAG_INHIBITORY | EVENT_FLAG_ROUTE_REC);
}

TEST_F(EventPacketMacroTest, VersionFlagsIndependent)
{
    memset(&packet, 0, sizeof(packet));

    EVENT_SET_VERSION(&packet, PROTOCOL_VERSION);
    EVENT_SET_FLAGS(&packet, EVENT_FLAG_EXCITATORY);

    ASSERT_EQ(EVENT_GET_VERSION(&packet), PROTOCOL_VERSION);
    ASSERT_EQ(EVENT_GET_FLAGS(&packet), EVENT_FLAG_EXCITATORY);
}

TEST_F(EventPacketMacroTest, SetFlagsPreservesVersion)
{
    EVENT_SET_VERSION(&packet, PROTOCOL_VERSION);

    EVENT_SET_FLAGS(&packet, EVENT_FLAG_INHIBITORY);

    ASSERT_EQ(EVENT_GET_VERSION(&packet), PROTOCOL_VERSION);
    ASSERT_EQ(EVENT_GET_FLAGS(&packet), EVENT_FLAG_INHIBITORY);
}

TEST_F(EventPacketMacroTest, SetVersionPreservesFlags)
{
    EVENT_SET_FLAGS(&packet, EVENT_FLAG_PLASTICITY);

    EVENT_SET_VERSION(&packet, PROTOCOL_VERSION);

    ASSERT_EQ(EVENT_GET_FLAGS(&packet), EVENT_FLAG_PLASTICITY);
    ASSERT_EQ(EVENT_GET_VERSION(&packet), PROTOCOL_VERSION);
}

//=============================================================================
// Feature Code Tests
//=============================================================================

TEST_F(EventPacketFeatureCodeTest, MakeFeatureCodeMacro)
{
    feature_code_t code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0x123456);

    ASSERT_EQ(GET_FEATURE_DOMAIN(code), FEATURE_DOMAIN_VISION);
    ASSERT_EQ(GET_FEATURE_SUBCODE(code), 0x123456);
}

TEST_F(EventPacketFeatureCodeTest, SetGetFeatureCode)
{
    create_valid_packet();
    feature_code_t code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_AUDITORY, 0xABCDEF);

    EVENT_SET_FEATURE_CODE(&packet, code);

    feature_code_t retrieved = EVENT_GET_FEATURE_CODE(&packet);
    ASSERT_EQ(retrieved, code);
}

TEST_F(EventPacketFeatureCodeTest, FeatureCodeAllDomains)
{
    create_valid_packet();

    feature_domain_t domains[] = {
        FEATURE_DOMAIN_SYSTEM,
        FEATURE_DOMAIN_VISION,
        FEATURE_DOMAIN_AUDITORY,
        FEATURE_DOMAIN_LANGUAGE,
        FEATURE_DOMAIN_MOTOR,
        FEATURE_DOMAIN_MEMORY,
        FEATURE_DOMAIN_EMOTION,
        FEATURE_DOMAIN_ETHICS,
        FEATURE_DOMAIN_NEUROMOD,
        FEATURE_DOMAIN_GLIAL,
        FEATURE_DOMAIN_BRAIN_REGION
    };

    for (size_t i = 0; i < sizeof(domains) / sizeof(domains[0]); i++) {
        feature_code_t code = MAKE_FEATURE_CODE(domains[i], 0x12345);
        EVENT_SET_FEATURE_CODE(&packet, code);

        feature_code_t retrieved = EVENT_GET_FEATURE_CODE(&packet);
        ASSERT_EQ(GET_FEATURE_DOMAIN(retrieved), domains[i]);
        ASSERT_EQ(GET_FEATURE_SUBCODE(retrieved), 0x12345);
    }
}

TEST_F(EventPacketFeatureCodeTest, FeatureCodeMaxSubcode)
{
    create_valid_packet();
    feature_code_t code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0xFFFFFF);

    EVENT_SET_FEATURE_CODE(&packet, code);

    feature_code_t retrieved = EVENT_GET_FEATURE_CODE(&packet);
    ASSERT_EQ(GET_FEATURE_SUBCODE(retrieved), 0xFFFFFF);
}

TEST_F(EventPacketFeatureCodeTest, FeatureCodeSerialization)
{
    create_valid_packet();
    feature_code_t code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_LANGUAGE, 0x789ABC);
    EVENT_SET_FEATURE_CODE(&packet, code);

    // Serialize
    int written = event_packet_serialize(&packet, nullptr, buffer, sizeof(buffer));
    ASSERT_GT(written, 0);

    // Deserialize
    event_packet_t deserialized;
    int result = event_packet_deserialize(buffer, written, &deserialized, nullptr, 0);
    ASSERT_GT(result, 0);

    // Verify feature code preserved
    feature_code_t retrieved = EVENT_GET_FEATURE_CODE(&deserialized);
    ASSERT_EQ(retrieved, code);
}

//=============================================================================
// Confidence Tests
//=============================================================================

TEST_F(EventPacketConfidenceTest, ConfidenceToFloat)
{
    uint16_t conf_min = 0;
    uint16_t conf_max = 65535;
    uint16_t conf_mid = 32767;

    ASSERT_FLOAT_EQ(EVENT_CONFIDENCE_TO_FLOAT(conf_min), 0.0f);
    ASSERT_FLOAT_EQ(EVENT_CONFIDENCE_TO_FLOAT(conf_max), 1.0f);
    ASSERT_NEAR(EVENT_CONFIDENCE_TO_FLOAT(conf_mid), 0.5f, 0.01f);
}

TEST_F(EventPacketConfidenceTest, FloatToConfidence)
{
    ASSERT_EQ(EVENT_FLOAT_TO_CONFIDENCE(0.0f), 0);
    ASSERT_EQ(EVENT_FLOAT_TO_CONFIDENCE(1.0f), 65535);
    ASSERT_NEAR(EVENT_FLOAT_TO_CONFIDENCE(0.5f), 32767, 1);
}

TEST_F(EventPacketConfidenceTest, ConfidenceRoundtrip)
{
    float values[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
        uint16_t conf = EVENT_FLOAT_TO_CONFIDENCE(values[i]);
        float result = EVENT_CONFIDENCE_TO_FLOAT(conf);

        ASSERT_NEAR(result, values[i], 0.001f);
    }
}

TEST_F(EventPacketConfidenceTest, ConfidenceInPacket)
{
    create_valid_packet();

    packet.confidence = EVENT_FLOAT_TO_CONFIDENCE(0.8f);

    float retrieved = EVENT_CONFIDENCE_TO_FLOAT(packet.confidence);
    ASSERT_NEAR(retrieved, 0.8f, 0.001f);
}

TEST_F(EventPacketConfidenceTest, ConfidenceSerialization)
{
    create_valid_packet();
    packet.confidence = EVENT_FLOAT_TO_CONFIDENCE(0.65f);

    // Serialize
    int written = event_packet_serialize(&packet, nullptr, buffer, sizeof(buffer));
    ASSERT_GT(written, 0);

    // Deserialize
    event_packet_t deserialized;
    int result = event_packet_deserialize(buffer, written, &deserialized, nullptr, 0);
    ASSERT_GT(result, 0);

    // Verify confidence preserved
    float retrieved = EVENT_CONFIDENCE_TO_FLOAT(deserialized.confidence);
    ASSERT_NEAR(retrieved, 0.65f, 0.001f);
}

//=============================================================================
// Boundary Condition Tests
//=============================================================================

TEST_F(EventPacketBoundaryTest, MinConfidence)
{
    create_valid_packet();
    packet.confidence = 0;

    ASSERT_TRUE(event_packet_validate(&packet));
    ASSERT_FLOAT_EQ(EVENT_CONFIDENCE_TO_FLOAT(packet.confidence), 0.0f);
}

TEST_F(EventPacketBoundaryTest, MaxConfidence)
{
    create_valid_packet();
    packet.confidence = 65535;

    ASSERT_TRUE(event_packet_validate(&packet));
    ASSERT_FLOAT_EQ(EVENT_CONFIDENCE_TO_FLOAT(packet.confidence), 1.0f);
}

TEST_F(EventPacketBoundaryTest, MinTimestamp)
{
    create_valid_packet();
    packet.timestamp = 0;

    ASSERT_TRUE(event_packet_validate(&packet));
}

TEST_F(EventPacketBoundaryTest, MaxTimestamp)
{
    create_valid_packet();
    packet.timestamp = UINT64_MAX;

    ASSERT_TRUE(event_packet_validate(&packet));
}

TEST_F(EventPacketBoundaryTest, MinHopCount)
{
    create_valid_packet();
    packet.hop_count = 0;

    ASSERT_TRUE(event_packet_validate(&packet));
}

TEST_F(EventPacketBoundaryTest, MaxHopCount)
{
    create_valid_packet();
    packet.hop_count = 255;

    ASSERT_TRUE(event_packet_validate(&packet));
}

TEST_F(EventPacketBoundaryTest, HopCountIncrement)
{
    create_valid_packet();
    packet.hop_count = 0;

    // Simulate packet traversal
    for (int i = 0; i < 10; i++) {
        packet.hop_count++;
        ASSERT_TRUE(event_packet_validate(&packet));
    }

    ASSERT_EQ(packet.hop_count, 10);
}

TEST_F(EventPacketBoundaryTest, TimestampOverflow)
{
    create_valid_packet();
    packet.timestamp = UINT64_MAX - 1;

    // Serialize and deserialize
    int written = event_packet_serialize(&packet, nullptr, buffer, sizeof(buffer));
    ASSERT_GT(written, 0);

    event_packet_t deserialized;
    int result = event_packet_deserialize(buffer, written, &deserialized, nullptr, 0);
    ASSERT_GT(result, 0);

    ASSERT_EQ(deserialized.timestamp, UINT64_MAX - 1);
}

TEST_F(EventPacketBoundaryTest, ZeroPayload)
{
    create_valid_packet();
    packet.payload_length = 0;

    int result = event_packet_serialize(&packet, nullptr, buffer, sizeof(buffer));

    ASSERT_GT(result, 0);
    ASSERT_EQ(result, sizeof(event_packet_t));
}

TEST_F(EventPacketBoundaryTest, MinimumBufferSize)
{
    create_valid_packet();

    // Buffer exactly the size needed
    uint8_t exact_buffer[sizeof(event_packet_t)];
    int result = event_packet_serialize(&packet, nullptr, exact_buffer, sizeof(exact_buffer));

    ASSERT_GT(result, 0);
    ASSERT_EQ(result, sizeof(event_packet_t));
}

//=============================================================================
// Flag Combination Tests
//=============================================================================

TEST_F(EventPacketFlagTest, SingleExcitatoryFlag)
{
    create_valid_packet();
    EVENT_SET_FLAGS(&packet, EVENT_FLAG_EXCITATORY);

    uint8_t flags = EVENT_GET_FLAGS(&packet);
    ASSERT_TRUE(flags & EVENT_FLAG_EXCITATORY);
    ASSERT_FALSE(flags & EVENT_FLAG_INHIBITORY);
    ASSERT_FALSE(flags & EVENT_FLAG_PLASTICITY);
}

TEST_F(EventPacketFlagTest, SingleInhibitoryFlag)
{
    create_valid_packet();
    EVENT_SET_FLAGS(&packet, EVENT_FLAG_INHIBITORY);

    uint8_t flags = EVENT_GET_FLAGS(&packet);
    ASSERT_FALSE(flags & EVENT_FLAG_EXCITATORY);
    ASSERT_TRUE(flags & EVENT_FLAG_INHIBITORY);
    ASSERT_FALSE(flags & EVENT_FLAG_PLASTICITY);
}

TEST_F(EventPacketFlagTest, ExcitatoryWithPlasticity)
{
    create_valid_packet();
    EVENT_SET_FLAGS(&packet, EVENT_FLAG_EXCITATORY | EVENT_FLAG_PLASTICITY);

    uint8_t flags = EVENT_GET_FLAGS(&packet);
    ASSERT_TRUE(flags & EVENT_FLAG_EXCITATORY);
    ASSERT_TRUE(flags & EVENT_FLAG_PLASTICITY);
    ASSERT_FALSE(flags & EVENT_FLAG_INHIBITORY);
}

TEST_F(EventPacketFlagTest, InhibitoryWithPlasticity)
{
    create_valid_packet();
    EVENT_SET_FLAGS(&packet, EVENT_FLAG_INHIBITORY | EVENT_FLAG_PLASTICITY);

    uint8_t flags = EVENT_GET_FLAGS(&packet);
    ASSERT_TRUE(flags & EVENT_FLAG_INHIBITORY);
    ASSERT_TRUE(flags & EVENT_FLAG_PLASTICITY);
    ASSERT_FALSE(flags & EVENT_FLAG_EXCITATORY);
}

TEST_F(EventPacketFlagTest, ExcitatoryWithRouteRecording)
{
    create_valid_packet();
    EVENT_SET_FLAGS(&packet, EVENT_FLAG_EXCITATORY | EVENT_FLAG_ROUTE_REC);

    uint8_t flags = EVENT_GET_FLAGS(&packet);
    ASSERT_TRUE(flags & EVENT_FLAG_EXCITATORY);
    ASSERT_TRUE(flags & EVENT_FLAG_ROUTE_REC);
}

TEST_F(EventPacketFlagTest, AllFlagsExceptInhibitory)
{
    create_valid_packet();
    EVENT_SET_FLAGS(&packet, EVENT_FLAG_EXCITATORY | EVENT_FLAG_PLASTICITY | EVENT_FLAG_ROUTE_REC);

    uint8_t flags = EVENT_GET_FLAGS(&packet);
    ASSERT_TRUE(flags & EVENT_FLAG_EXCITATORY);
    ASSERT_TRUE(flags & EVENT_FLAG_PLASTICITY);
    ASSERT_TRUE(flags & EVENT_FLAG_ROUTE_REC);
    ASSERT_FALSE(flags & EVENT_FLAG_INHIBITORY);

    ASSERT_TRUE(event_packet_validate(&packet));
}

TEST_F(EventPacketFlagTest, FlagsSerialization)
{
    create_valid_packet();
    EVENT_SET_FLAGS(&packet, EVENT_FLAG_INHIBITORY | EVENT_FLAG_PLASTICITY);

    // Serialize
    int written = event_packet_serialize(&packet, nullptr, buffer, sizeof(buffer));
    ASSERT_GT(written, 0);

    // Deserialize
    event_packet_t deserialized;
    int result = event_packet_deserialize(buffer, written, &deserialized, nullptr, 0);
    ASSERT_GT(result, 0);

    // Verify flags preserved
    uint8_t flags = EVENT_GET_FLAGS(&deserialized);
    ASSERT_TRUE(flags & EVENT_FLAG_INHIBITORY);
    ASSERT_TRUE(flags & EVENT_FLAG_PLASTICITY);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(EventPacketTest, CompletePacketRoundtrip)
{
    // Create complete packet with all fields set
    EVENT_SET_VERSION(&packet, PROTOCOL_VERSION);
    EVENT_SET_FLAGS(&packet, EVENT_FLAG_EXCITATORY | EVENT_FLAG_PLASTICITY);
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0x5678));
    packet.source_node_id = 999;
    packet.timestamp = 1234567890ULL;
    packet.confidence = EVENT_FLOAT_TO_CONFIDENCE(0.9f);
    packet.hop_count = 3;
    packet.payload_length = 256;

    uint8_t payload[256];
    create_test_payload(payload, sizeof(payload));

    // Serialize
    int written = event_packet_serialize(&packet, payload, buffer, sizeof(buffer));
    ASSERT_GT(written, 0);

    // Deserialize
    event_packet_t deserialized;
    uint8_t deserialized_payload[256];
    int result = event_packet_deserialize(buffer, written, &deserialized,
                                         deserialized_payload, sizeof(deserialized_payload));

    ASSERT_GT(result, 0);
    ASSERT_EQ(result, written);

    // Verify all fields
    ASSERT_EQ(EVENT_GET_VERSION(&deserialized), PROTOCOL_VERSION);
    ASSERT_EQ(EVENT_GET_FLAGS(&deserialized), EVENT_FLAG_EXCITATORY | EVENT_FLAG_PLASTICITY);
    ASSERT_EQ(EVENT_GET_FEATURE_CODE(&deserialized),
              MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0x5678));
    ASSERT_EQ(deserialized.source_node_id, 999);
    ASSERT_EQ(deserialized.timestamp, 1234567890ULL);
    ASSERT_EQ(deserialized.confidence, packet.confidence);
    ASSERT_EQ(deserialized.hop_count, 3);
    ASSERT_EQ(deserialized.payload_length, 256);
    ASSERT_EQ(memcmp(deserialized_payload, payload, sizeof(payload)), 0);
}

TEST_F(EventPacketTest, MultiplePacketsSerialization)
{
    // Test serializing multiple packets to same buffer
    for (int i = 0; i < 10; i++) {
        create_valid_packet();
        packet.source_node_id = i;
        packet.timestamp = i * 1000;
        packet.hop_count = i % 8;

        int result = event_packet_serialize(&packet, nullptr, buffer, sizeof(buffer));
        ASSERT_GT(result, 0);

        // Deserialize and verify
        event_packet_t deserialized;
        int read = event_packet_deserialize(buffer, result, &deserialized, nullptr, 0);
        ASSERT_GT(read, 0);

        ASSERT_EQ(deserialized.source_node_id, i);
        ASSERT_EQ(deserialized.timestamp, i * 1000);
        ASSERT_EQ(deserialized.hop_count, i % 8);
    }
}
