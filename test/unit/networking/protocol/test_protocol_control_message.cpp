/**
 * @file test_protocol_control_message.cpp
 * @brief Comprehensive unit tests for NIMCP 2.0 control message functions
 * @details Tests serialization, deserialization, validation, and all control message types
 *          including neuromodulator, glial, and brain region payloads
 */

#include <string.h>
#include <time.h>
#include "networking/protocol/nimcp_protocol.h"
#include "test_helpers.h"

//=============================================================================
// Test Fixture Classes
//=============================================================================

/**
 * @brief Base test fixture for control message tests
 */
class ControlMessageTest : public ::testing::Test {
   protected:
    uint8_t buffer[8192];
    uint8_t param_buffer[4096];
    control_message_t msg;
    uint64_t test_timestamp;

    void SetUp() override
    {
        memset(buffer, 0, sizeof(buffer));
        memset(param_buffer, 0, sizeof(param_buffer));
        memset(&msg, 0, sizeof(msg));
        test_timestamp = (uint64_t)time(NULL) * 1000000;  // Current time in microseconds
    }

    /**
     * @brief Helper to initialize a basic control message
     */
    void init_control_message(control_msg_type_t type, uint32_t param_size = 0)
    {
        msg.version = PROTOCOL_VERSION;
        msg.msg_type = type;
        msg.flags = 0;
        msg.reserved = 0;
        msg.message_length = sizeof(control_message_t) + param_size;
        msg.source_node_id = 0x12345678;
        msg.target_specifier = 0xFFFFFFFF;  // Global
        msg.sequence_number = 1;
        msg.param_count = 0;
        msg.reserved2 = 0;
    }
};

/**
 * @brief Test fixture for control message serialization
 */
class ControlMessageSerializationTest : public ControlMessageTest {};

/**
 * @brief Test fixture for control message deserialization
 */
class ControlMessageDeserializationTest : public ControlMessageTest {};

/**
 * @brief Test fixture for control message validation
 */
class ControlMessageValidationTest : public ControlMessageTest {};

/**
 * @brief Test fixture for neuromodulator message payloads
 */
class NeuromodulatorPayloadTest : public ControlMessageTest {};

/**
 * @brief Test fixture for glial message payloads
 */
class GlialPayloadTest : public ControlMessageTest {};

/**
 * @brief Test fixture for brain region message payloads
 */
class BrainRegionPayloadTest : public ControlMessageTest {};

/**
 * @brief Test fixture for control message flags
 */
class ControlMessageFlagsTest : public ControlMessageTest {};

//=============================================================================
// Control Message Serialization Tests
//=============================================================================

TEST_F(ControlMessageSerializationTest, BasicSerializeNoParams)
{
    init_control_message(CTRL_MSG_HEARTBEAT);

    int result = control_message_serialize(&msg, nullptr, buffer, sizeof(buffer));

    ASSERT_GT(result, 0);
    ASSERT_EQ(result, sizeof(control_message_t));

    // Verify header was copied correctly
    control_message_t* serialized = (control_message_t*)buffer;
    ASSERT_EQ(serialized->version, PROTOCOL_VERSION);
    ASSERT_EQ(serialized->msg_type, CTRL_MSG_HEARTBEAT);
    ASSERT_EQ(serialized->source_node_id, 0x12345678);
}

TEST_F(ControlMessageSerializationTest, SerializeWithParameters)
{
    uint32_t param_data[] = {0x11111111, 0x22222222, 0x33333333};
    uint32_t param_size = sizeof(param_data);

    init_control_message(CTRL_MSG_SET_LEARNING_RATE, param_size);

    int result = control_message_serialize(&msg, param_data, buffer, sizeof(buffer));

    ASSERT_GT(result, 0);
    ASSERT_EQ(result, sizeof(control_message_t) + param_size);

    // Verify parameters were copied
    uint32_t* serialized_params = (uint32_t*)(buffer + sizeof(control_message_t));
    ASSERT_EQ(serialized_params[0], 0x11111111);
    ASSERT_EQ(serialized_params[1], 0x22222222);
    ASSERT_EQ(serialized_params[2], 0x33333333);
}

TEST_F(ControlMessageSerializationTest, SerializeBufferTooSmall)
{
    init_control_message(CTRL_MSG_HEARTBEAT);

    uint8_t small_buffer[10];
    int result = control_message_serialize(&msg, nullptr, small_buffer, sizeof(small_buffer));

    ASSERT_EQ(result, -1);
}

TEST_F(ControlMessageSerializationTest, SerializeNullMessage)
{
    int result = control_message_serialize(nullptr, nullptr, buffer, sizeof(buffer));

    ASSERT_EQ(result, -1);
}

TEST_F(ControlMessageSerializationTest, SerializeNullBuffer)
{
    init_control_message(CTRL_MSG_HEARTBEAT);

    int result = control_message_serialize(&msg, nullptr, nullptr, sizeof(buffer));

    ASSERT_EQ(result, -1);
}

TEST_F(ControlMessageSerializationTest, SerializeAllControlMessageTypes)
{
    control_msg_type_t types[] = {
        CTRL_MSG_VERSION_NEGOTIATION, CTRL_MSG_ADD_LINK,       CTRL_MSG_REMOVE_LINK,
        CTRL_MSG_UPDATE_LINK,         CTRL_MSG_SET_SUBSCRIPTION, CTRL_MSG_DEFINE_FEATURE_NS,
        CTRL_MSG_SET_LEARNING_RATE,   CTRL_MSG_SET_PLASTICITY_RULE, CTRL_MSG_CLUSTER_ANNOUNCE,
        CTRL_MSG_ETHICS_POLICY,       CTRL_MSG_ERROR_REPORT,   CTRL_MSG_TOPOLOGY_QUERY,
        CTRL_MSG_HEARTBEAT,           CTRL_MSG_PARTITION_DETECTED, CTRL_MSG_RECOVERY_SYNC,
        CTRL_MSG_NEUROMOD_LEVEL,      CTRL_MSG_NEUROMOD_DIFFUSION, CTRL_MSG_GLIAL_PRUNING,
        CTRL_MSG_GLIAL_CALCIUM,       CTRL_MSG_REGION_SYNC,    CTRL_MSG_REGION_ACTIVITY
    };

    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        memset(buffer, 0, sizeof(buffer));
        init_control_message(types[i]);

        int result = control_message_serialize(&msg, nullptr, buffer, sizeof(buffer));

        ASSERT_GT(result, 0) << "Failed for message type: " << (int)types[i];
        ASSERT_EQ(result, sizeof(control_message_t));

        control_message_t* serialized = (control_message_t*)buffer;
        ASSERT_EQ(serialized->msg_type, types[i]);
    }
}

//=============================================================================
// Control Message Deserialization Tests
//=============================================================================

TEST_F(ControlMessageDeserializationTest, BasicDeserializeNoParams)
{
    init_control_message(CTRL_MSG_HEARTBEAT);

    // Serialize first
    int written = control_message_serialize(&msg, nullptr, buffer, sizeof(buffer));
    ASSERT_GT(written, 0);

    // Now deserialize
    control_message_t deserialized;
    int read = control_message_deserialize(buffer, written, &deserialized, nullptr, 0);

    ASSERT_GT(read, 0);
    ASSERT_EQ(read, sizeof(control_message_t));
    ASSERT_EQ(deserialized.version, PROTOCOL_VERSION);
    ASSERT_EQ(deserialized.msg_type, CTRL_MSG_HEARTBEAT);
    ASSERT_EQ(deserialized.source_node_id, 0x12345678);
}

TEST_F(ControlMessageDeserializationTest, DeserializeWithParameters)
{
    uint32_t param_data[] = {0xAAAAAAAA, 0xBBBBBBBB};
    uint32_t param_size = sizeof(param_data);

    init_control_message(CTRL_MSG_SET_LEARNING_RATE, param_size);

    // Serialize
    int written = control_message_serialize(&msg, param_data, buffer, sizeof(buffer));
    ASSERT_GT(written, 0);

    // Deserialize
    control_message_t deserialized;
    uint32_t deserialized_params[2];
    int read = control_message_deserialize(buffer, written, &deserialized, deserialized_params,
                                           sizeof(deserialized_params));

    ASSERT_GT(read, 0);
    ASSERT_EQ(deserialized_params[0], 0xAAAAAAAA);
    ASSERT_EQ(deserialized_params[1], 0xBBBBBBBB);
}

TEST_F(ControlMessageDeserializationTest, DeserializeInvalidVersion)
{
    init_control_message(CTRL_MSG_HEARTBEAT);
    msg.version = 0xFF;  // Invalid version

    memcpy(buffer, &msg, sizeof(msg));

    control_message_t deserialized;
    int result = control_message_deserialize(buffer, sizeof(msg), &deserialized, nullptr, 0);

    ASSERT_EQ(result, -1);
}

TEST_F(ControlMessageDeserializationTest, DeserializeInvalidMessageType)
{
    init_control_message((control_msg_type_t)0xFF);  // Invalid type

    memcpy(buffer, &msg, sizeof(msg));

    control_message_t deserialized;
    int result = control_message_deserialize(buffer, sizeof(msg), &deserialized, nullptr, 0);

    ASSERT_EQ(result, -1);
}

TEST_F(ControlMessageDeserializationTest, DeserializeNullBuffer)
{
    control_message_t deserialized;
    int result = control_message_deserialize(nullptr, 100, &deserialized, nullptr, 0);

    ASSERT_EQ(result, -1);
}

TEST_F(ControlMessageDeserializationTest, DeserializeNullMessage)
{
    init_control_message(CTRL_MSG_HEARTBEAT);
    memcpy(buffer, &msg, sizeof(msg));

    int result = control_message_deserialize(buffer, sizeof(msg), nullptr, nullptr, 0);

    ASSERT_EQ(result, -1);
}

TEST_F(ControlMessageDeserializationTest, DeserializeBufferTooSmall)
{
    control_message_t deserialized;
    int result = control_message_deserialize(buffer, 5, &deserialized, nullptr, 0);

    ASSERT_EQ(result, -1);
}

TEST_F(ControlMessageDeserializationTest, DeserializeParamBufferTooSmall)
{
    uint32_t param_data[] = {0x11111111, 0x22222222, 0x33333333};
    uint32_t param_size = sizeof(param_data);

    init_control_message(CTRL_MSG_SET_LEARNING_RATE, param_size);

    // Serialize
    int written = control_message_serialize(&msg, param_data, buffer, sizeof(buffer));
    ASSERT_GT(written, 0);

    // Try to deserialize with too small param buffer
    control_message_t deserialized;
    uint32_t small_param_buffer[1];  // Only room for 1, but need 3
    int result = control_message_deserialize(buffer, written, &deserialized, small_param_buffer,
                                            sizeof(small_param_buffer));

    ASSERT_EQ(result, -1);
}

//=============================================================================
// Control Message Validation Tests
//=============================================================================

TEST_F(ControlMessageValidationTest, ValidMessage)
{
    init_control_message(CTRL_MSG_HEARTBEAT);

    ASSERT_TRUE(control_message_validate(&msg));
}

TEST_F(ControlMessageValidationTest, ValidMessageWithParams)
{
    init_control_message(CTRL_MSG_SET_LEARNING_RATE, 128);

    ASSERT_TRUE(control_message_validate(&msg));
}

TEST_F(ControlMessageValidationTest, NullPointer)
{
    ASSERT_FALSE(control_message_validate(nullptr));
}

TEST_F(ControlMessageValidationTest, InvalidVersion)
{
    init_control_message(CTRL_MSG_HEARTBEAT);
    msg.version = 0;

    ASSERT_FALSE(control_message_validate(&msg));
}

TEST_F(ControlMessageValidationTest, InvalidMessageType)
{
    init_control_message(CTRL_MSG_HEARTBEAT);
    msg.msg_type = CTRL_MSG_MAX;

    ASSERT_FALSE(control_message_validate(&msg));
}

TEST_F(ControlMessageValidationTest, MessageLengthTooSmall)
{
    init_control_message(CTRL_MSG_HEARTBEAT);
    msg.message_length = sizeof(control_message_t) - 1;

    ASSERT_FALSE(control_message_validate(&msg));
}

TEST_F(ControlMessageValidationTest, MessageLengthTooLarge)
{
    init_control_message(CTRL_MSG_HEARTBEAT);
    msg.message_length = MAX_PAYLOAD_SIZE + 1;

    ASSERT_FALSE(control_message_validate(&msg));
}

TEST_F(ControlMessageValidationTest, MessageLengthAtMax)
{
    init_control_message(CTRL_MSG_HEARTBEAT);
    msg.message_length = MAX_PAYLOAD_SIZE;

    ASSERT_TRUE(control_message_validate(&msg));
}

//=============================================================================
// Control Message Flags Tests
//=============================================================================

TEST_F(ControlMessageFlagsTest, AckRequiredFlag)
{
    init_control_message(CTRL_MSG_SET_LEARNING_RATE);
    msg.flags = CTRL_FLAG_ACK_REQUIRED;

    int result = control_message_serialize(&msg, nullptr, buffer, sizeof(buffer));
    ASSERT_GT(result, 0);

    control_message_t deserialized;
    control_message_deserialize(buffer, result, &deserialized, nullptr, 0);

    ASSERT_EQ(deserialized.flags & CTRL_FLAG_ACK_REQUIRED, CTRL_FLAG_ACK_REQUIRED);
}

TEST_F(ControlMessageFlagsTest, GlobalFlag)
{
    init_control_message(CTRL_MSG_CLUSTER_ANNOUNCE);
    msg.flags = CTRL_FLAG_GLOBAL;

    int result = control_message_serialize(&msg, nullptr, buffer, sizeof(buffer));
    ASSERT_GT(result, 0);

    control_message_t deserialized;
    control_message_deserialize(buffer, result, &deserialized, nullptr, 0);

    ASSERT_EQ(deserialized.flags & CTRL_FLAG_GLOBAL, CTRL_FLAG_GLOBAL);
}

TEST_F(ControlMessageFlagsTest, SignedFlag)
{
    init_control_message(CTRL_MSG_ETHICS_POLICY);
    msg.flags = CTRL_FLAG_SIGNED;

    int result = control_message_serialize(&msg, nullptr, buffer, sizeof(buffer));
    ASSERT_GT(result, 0);

    control_message_t deserialized;
    control_message_deserialize(buffer, result, &deserialized, nullptr, 0);

    ASSERT_EQ(deserialized.flags & CTRL_FLAG_SIGNED, CTRL_FLAG_SIGNED);
}

TEST_F(ControlMessageFlagsTest, RelayFlag)
{
    init_control_message(CTRL_MSG_PARTITION_DETECTED);
    msg.flags = CTRL_FLAG_RELAY;

    int result = control_message_serialize(&msg, nullptr, buffer, sizeof(buffer));
    ASSERT_GT(result, 0);

    control_message_t deserialized;
    control_message_deserialize(buffer, result, &deserialized, nullptr, 0);

    ASSERT_EQ(deserialized.flags & CTRL_FLAG_RELAY, CTRL_FLAG_RELAY);
}

TEST_F(ControlMessageFlagsTest, MultipleFlagsCombined)
{
    init_control_message(CTRL_MSG_ETHICS_POLICY);
    msg.flags = CTRL_FLAG_ACK_REQUIRED | CTRL_FLAG_GLOBAL | CTRL_FLAG_SIGNED;

    int result = control_message_serialize(&msg, nullptr, buffer, sizeof(buffer));
    ASSERT_GT(result, 0);

    control_message_t deserialized;
    control_message_deserialize(buffer, result, &deserialized, nullptr, 0);

    ASSERT_EQ(deserialized.flags & CTRL_FLAG_ACK_REQUIRED, CTRL_FLAG_ACK_REQUIRED);
    ASSERT_EQ(deserialized.flags & CTRL_FLAG_GLOBAL, CTRL_FLAG_GLOBAL);
    ASSERT_EQ(deserialized.flags & CTRL_FLAG_SIGNED, CTRL_FLAG_SIGNED);
}

TEST_F(ControlMessageFlagsTest, AllFlagsCombined)
{
    init_control_message(CTRL_MSG_RECOVERY_SYNC);
    msg.flags = CTRL_FLAG_ACK_REQUIRED | CTRL_FLAG_GLOBAL | CTRL_FLAG_SIGNED | CTRL_FLAG_RELAY;

    int result = control_message_serialize(&msg, nullptr, buffer, sizeof(buffer));
    ASSERT_GT(result, 0);

    control_message_t deserialized;
    control_message_deserialize(buffer, result, &deserialized, nullptr, 0);

    ASSERT_EQ(deserialized.flags, msg.flags);
}

//=============================================================================
// Neuromodulator Payload Tests
//=============================================================================

TEST_F(NeuromodulatorPayloadTest, NeuromodLevelPayloadSerialization)
{
    neuromod_level_payload_t payload;
    payload.neuromod_type = 0;  // DOPAMINE
    payload.reserved = 0;
    payload.region_id = 42;
    payload.concentration = 0.75f;
    payload.timestamp = test_timestamp;

    init_control_message(CTRL_MSG_NEUROMOD_LEVEL, sizeof(payload));

    int result = control_message_serialize(&msg, &payload, buffer, sizeof(buffer));

    ASSERT_GT(result, 0);
    ASSERT_EQ(result, sizeof(control_message_t) + sizeof(payload));

    // Verify payload
    neuromod_level_payload_t* serialized_payload =
        (neuromod_level_payload_t*)(buffer + sizeof(control_message_t));
    ASSERT_EQ(serialized_payload->neuromod_type, 0);
    ASSERT_EQ(serialized_payload->region_id, 42);
    ASSERT_FLOAT_EQ(serialized_payload->concentration, 0.75f);
    ASSERT_EQ(serialized_payload->timestamp, test_timestamp);
}

TEST_F(NeuromodulatorPayloadTest, NeuromodLevelPayloadRoundtrip)
{
    neuromod_level_payload_t payload;
    payload.neuromod_type = 1;  // SEROTONIN
    payload.reserved = 0;
    payload.region_id = 123;
    payload.concentration = 0.42f;
    payload.timestamp = test_timestamp;

    init_control_message(CTRL_MSG_NEUROMOD_LEVEL, sizeof(payload));

    // Serialize
    int written = control_message_serialize(&msg, &payload, buffer, sizeof(buffer));
    ASSERT_GT(written, 0);

    // Deserialize
    control_message_t deserialized;
    neuromod_level_payload_t deserialized_payload;
    int read = control_message_deserialize(buffer, written, &deserialized, &deserialized_payload,
                                           sizeof(deserialized_payload));

    ASSERT_GT(read, 0);
    ASSERT_EQ(deserialized.msg_type, CTRL_MSG_NEUROMOD_LEVEL);
    ASSERT_EQ(deserialized_payload.neuromod_type, 1);
    ASSERT_EQ(deserialized_payload.region_id, 123);
    ASSERT_FLOAT_EQ(deserialized_payload.concentration, 0.42f);
    ASSERT_EQ(deserialized_payload.timestamp, test_timestamp);
}

TEST_F(NeuromodulatorPayloadTest, AllNeuromodulatorTypes)
{
    uint8_t neuromod_types[] = {0, 1, 2, 3, 4, 5};  // All 6 types

    for (size_t i = 0; i < sizeof(neuromod_types); i++) {
        neuromod_level_payload_t payload;
        payload.neuromod_type = neuromod_types[i];
        payload.reserved = 0;
        payload.region_id = 100 + i;
        payload.concentration = 0.1f * (i + 1);
        payload.timestamp = test_timestamp + i;

        init_control_message(CTRL_MSG_NEUROMOD_LEVEL, sizeof(payload));

        int written = control_message_serialize(&msg, &payload, buffer, sizeof(buffer));
        ASSERT_GT(written, 0);

        control_message_t deserialized;
        neuromod_level_payload_t deserialized_payload;
        int read = control_message_deserialize(buffer, written, &deserialized,
                                               &deserialized_payload, sizeof(deserialized_payload));

        ASSERT_GT(read, 0);
        ASSERT_EQ(deserialized_payload.neuromod_type, neuromod_types[i]);
        ASSERT_EQ(deserialized_payload.region_id, 100 + i);
    }
}

TEST_F(NeuromodulatorPayloadTest, NeuromodDiffusionMessage)
{
    neuromod_level_payload_t payload;
    payload.neuromod_type = 2;  // ACETYLCHOLINE
    payload.reserved = 0;
    payload.region_id = 55;
    payload.concentration = 0.88f;
    payload.timestamp = test_timestamp;

    init_control_message(CTRL_MSG_NEUROMOD_DIFFUSION, sizeof(payload));
    msg.flags = CTRL_FLAG_RELAY;  // Diffusion should relay

    int written = control_message_serialize(&msg, &payload, buffer, sizeof(buffer));
    ASSERT_GT(written, 0);

    control_message_t deserialized;
    neuromod_level_payload_t deserialized_payload;
    int read = control_message_deserialize(buffer, written, &deserialized, &deserialized_payload,
                                           sizeof(deserialized_payload));

    ASSERT_GT(read, 0);
    ASSERT_EQ(deserialized.msg_type, CTRL_MSG_NEUROMOD_DIFFUSION);
    ASSERT_EQ(deserialized.flags & CTRL_FLAG_RELAY, CTRL_FLAG_RELAY);
}

//=============================================================================
// Glial Payload Tests
//=============================================================================

TEST_F(GlialPayloadTest, GlialPruningPayloadSerialization)
{
    glial_pruning_payload_t payload;
    payload.source_neuron_id = 1000;
    payload.target_neuron_id = 2000;
    payload.activity_score = 0.25f;
    payload.pruning_action = 1;  // Prune
    memset(payload.reserved, 0, sizeof(payload.reserved));
    payload.timestamp = test_timestamp;

    init_control_message(CTRL_MSG_GLIAL_PRUNING, sizeof(payload));

    int result = control_message_serialize(&msg, &payload, buffer, sizeof(buffer));

    ASSERT_GT(result, 0);

    glial_pruning_payload_t* serialized_payload =
        (glial_pruning_payload_t*)(buffer + sizeof(control_message_t));
    ASSERT_EQ(serialized_payload->source_neuron_id, 1000);
    ASSERT_EQ(serialized_payload->target_neuron_id, 2000);
    ASSERT_FLOAT_EQ(serialized_payload->activity_score, 0.25f);
    ASSERT_EQ(serialized_payload->pruning_action, 1);
}

TEST_F(GlialPayloadTest, GlialPruningPayloadRoundtrip)
{
    glial_pruning_payload_t payload;
    payload.source_neuron_id = 3333;
    payload.target_neuron_id = 4444;
    payload.activity_score = 0.95f;
    payload.pruning_action = 2;  // Preserve
    memset(payload.reserved, 0, sizeof(payload.reserved));
    payload.timestamp = test_timestamp;

    init_control_message(CTRL_MSG_GLIAL_PRUNING, sizeof(payload));

    int written = control_message_serialize(&msg, &payload, buffer, sizeof(buffer));
    ASSERT_GT(written, 0);

    control_message_t deserialized;
    glial_pruning_payload_t deserialized_payload;
    int read = control_message_deserialize(buffer, written, &deserialized, &deserialized_payload,
                                           sizeof(deserialized_payload));

    ASSERT_GT(read, 0);
    ASSERT_EQ(deserialized_payload.source_neuron_id, 3333);
    ASSERT_EQ(deserialized_payload.target_neuron_id, 4444);
    ASSERT_FLOAT_EQ(deserialized_payload.activity_score, 0.95f);
    ASSERT_EQ(deserialized_payload.pruning_action, 2);
}

TEST_F(GlialPayloadTest, GlialPruningAllActions)
{
    uint8_t actions[] = {0, 1, 2};  // Monitor, Prune, Preserve

    for (size_t i = 0; i < sizeof(actions); i++) {
        glial_pruning_payload_t payload;
        payload.source_neuron_id = 1000 + i;
        payload.target_neuron_id = 2000 + i;
        payload.activity_score = 0.3f * i;
        payload.pruning_action = actions[i];
        memset(payload.reserved, 0, sizeof(payload.reserved));
        payload.timestamp = test_timestamp + i;

        init_control_message(CTRL_MSG_GLIAL_PRUNING, sizeof(payload));

        int written = control_message_serialize(&msg, &payload, buffer, sizeof(buffer));
        ASSERT_GT(written, 0);

        control_message_t deserialized;
        glial_pruning_payload_t deserialized_payload;
        int read = control_message_deserialize(buffer, written, &deserialized,
                                               &deserialized_payload, sizeof(deserialized_payload));

        ASSERT_GT(read, 0);
        ASSERT_EQ(deserialized_payload.pruning_action, actions[i]);
    }
}

TEST_F(GlialPayloadTest, GlialCalciumPayloadSerialization)
{
    glial_calcium_payload_t payload;
    payload.astrocyte_id = 777;
    payload.calcium_level = 0.68f;
    payload.wave_velocity = 125.5f;
    payload.affected_synapses = 42;
    payload.reserved = 0;
    payload.timestamp = test_timestamp;

    init_control_message(CTRL_MSG_GLIAL_CALCIUM, sizeof(payload));

    int result = control_message_serialize(&msg, &payload, buffer, sizeof(buffer));

    ASSERT_GT(result, 0);

    glial_calcium_payload_t* serialized_payload =
        (glial_calcium_payload_t*)(buffer + sizeof(control_message_t));
    ASSERT_EQ(serialized_payload->astrocyte_id, 777);
    ASSERT_FLOAT_EQ(serialized_payload->calcium_level, 0.68f);
    ASSERT_FLOAT_EQ(serialized_payload->wave_velocity, 125.5f);
    ASSERT_EQ(serialized_payload->affected_synapses, 42);
}

TEST_F(GlialPayloadTest, GlialCalciumPayloadRoundtrip)
{
    glial_calcium_payload_t payload;
    payload.astrocyte_id = 999;
    payload.calcium_level = 0.85f;
    payload.wave_velocity = 200.0f;
    payload.affected_synapses = 128;
    payload.reserved = 0;
    payload.timestamp = test_timestamp;

    init_control_message(CTRL_MSG_GLIAL_CALCIUM, sizeof(payload));
    msg.flags = CTRL_FLAG_RELAY;  // Calcium waves should propagate

    int written = control_message_serialize(&msg, &payload, buffer, sizeof(buffer));
    ASSERT_GT(written, 0);

    control_message_t deserialized;
    glial_calcium_payload_t deserialized_payload;
    int read = control_message_deserialize(buffer, written, &deserialized, &deserialized_payload,
                                           sizeof(deserialized_payload));

    ASSERT_GT(read, 0);
    ASSERT_EQ(deserialized.msg_type, CTRL_MSG_GLIAL_CALCIUM);
    ASSERT_EQ(deserialized_payload.astrocyte_id, 999);
    ASSERT_FLOAT_EQ(deserialized_payload.calcium_level, 0.85f);
    ASSERT_FLOAT_EQ(deserialized_payload.wave_velocity, 200.0f);
    ASSERT_EQ(deserialized_payload.affected_synapses, 128);
}

//=============================================================================
// Brain Region Payload Tests
//=============================================================================

TEST_F(BrainRegionPayloadTest, RegionActivityPayloadSerialization)
{
    region_activity_payload_t payload;
    payload.region_type = 1;  // Example region type
    payload.reserved = 0;
    payload.avg_activity = 0.55f;
    payload.spike_rate = 47.3f;
    payload.active_neurons = 850;
    payload.total_neurons = 1000;
    payload.timestamp = test_timestamp;

    init_control_message(CTRL_MSG_REGION_ACTIVITY, sizeof(payload));

    int result = control_message_serialize(&msg, &payload, buffer, sizeof(buffer));

    ASSERT_GT(result, 0);

    region_activity_payload_t* serialized_payload =
        (region_activity_payload_t*)(buffer + sizeof(control_message_t));
    ASSERT_EQ(serialized_payload->region_type, 1);
    ASSERT_FLOAT_EQ(serialized_payload->avg_activity, 0.55f);
    ASSERT_FLOAT_EQ(serialized_payload->spike_rate, 47.3f);
    ASSERT_EQ(serialized_payload->active_neurons, 850);
    ASSERT_EQ(serialized_payload->total_neurons, 1000);
}

TEST_F(BrainRegionPayloadTest, RegionActivityPayloadRoundtrip)
{
    region_activity_payload_t payload;
    payload.region_type = 5;
    payload.reserved = 0;
    payload.avg_activity = 0.72f;
    payload.spike_rate = 88.9f;
    payload.active_neurons = 1500;
    payload.total_neurons = 2000;
    payload.timestamp = test_timestamp;

    init_control_message(CTRL_MSG_REGION_ACTIVITY, sizeof(payload));

    int written = control_message_serialize(&msg, &payload, buffer, sizeof(buffer));
    ASSERT_GT(written, 0);

    control_message_t deserialized;
    region_activity_payload_t deserialized_payload;
    int read = control_message_deserialize(buffer, written, &deserialized, &deserialized_payload,
                                           sizeof(deserialized_payload));

    ASSERT_GT(read, 0);
    ASSERT_EQ(deserialized.msg_type, CTRL_MSG_REGION_ACTIVITY);
    ASSERT_EQ(deserialized_payload.region_type, 5);
    ASSERT_FLOAT_EQ(deserialized_payload.avg_activity, 0.72f);
    ASSERT_FLOAT_EQ(deserialized_payload.spike_rate, 88.9f);
    ASSERT_EQ(deserialized_payload.active_neurons, 1500);
    ASSERT_EQ(deserialized_payload.total_neurons, 2000);
}

TEST_F(BrainRegionPayloadTest, RegionSyncMessage)
{
    region_activity_payload_t payload;
    payload.region_type = 10;
    payload.reserved = 0;
    payload.avg_activity = 0.33f;
    payload.spike_rate = 22.1f;
    payload.active_neurons = 300;
    payload.total_neurons = 500;
    payload.timestamp = test_timestamp;

    init_control_message(CTRL_MSG_REGION_SYNC, sizeof(payload));
    msg.flags = CTRL_FLAG_GLOBAL;  // Region sync should be global

    int written = control_message_serialize(&msg, &payload, buffer, sizeof(buffer));
    ASSERT_GT(written, 0);

    control_message_t deserialized;
    region_activity_payload_t deserialized_payload;
    int read = control_message_deserialize(buffer, written, &deserialized, &deserialized_payload,
                                           sizeof(deserialized_payload));

    ASSERT_GT(read, 0);
    ASSERT_EQ(deserialized.msg_type, CTRL_MSG_REGION_SYNC);
    ASSERT_EQ(deserialized.flags & CTRL_FLAG_GLOBAL, CTRL_FLAG_GLOBAL);
}

//=============================================================================
// Sequence Number Tests
//=============================================================================

TEST_F(ControlMessageTest, SequenceNumberHandling)
{
    init_control_message(CTRL_MSG_HEARTBEAT);

    for (uint32_t seq = 0; seq < 10; seq++) {
        msg.sequence_number = seq;

        int written = control_message_serialize(&msg, nullptr, buffer, sizeof(buffer));
        ASSERT_GT(written, 0);

        control_message_t deserialized;
        int read = control_message_deserialize(buffer, written, &deserialized, nullptr, 0);

        ASSERT_GT(read, 0);
        ASSERT_EQ(deserialized.sequence_number, seq);
    }
}

TEST_F(ControlMessageTest, SequenceNumberWrapAround)
{
    init_control_message(CTRL_MSG_HEARTBEAT);
    msg.sequence_number = 0xFFFFFFFF;  // Max value

    int written = control_message_serialize(&msg, nullptr, buffer, sizeof(buffer));
    ASSERT_GT(written, 0);

    control_message_t deserialized;
    int read = control_message_deserialize(buffer, written, &deserialized, nullptr, 0);

    ASSERT_GT(read, 0);
    ASSERT_EQ(deserialized.sequence_number, 0xFFFFFFFF);
}

//=============================================================================
// Target Specifier Tests
//=============================================================================

TEST_F(ControlMessageTest, TargetSpecifierSingleNode)
{
    init_control_message(CTRL_MSG_SET_LEARNING_RATE);
    msg.target_specifier = 0x12345678;  // Specific node

    int written = control_message_serialize(&msg, nullptr, buffer, sizeof(buffer));
    ASSERT_GT(written, 0);

    control_message_t deserialized;
    int read = control_message_deserialize(buffer, written, &deserialized, nullptr, 0);

    ASSERT_GT(read, 0);
    ASSERT_EQ(deserialized.target_specifier, 0x12345678);
}

TEST_F(ControlMessageTest, TargetSpecifierGlobal)
{
    init_control_message(CTRL_MSG_CLUSTER_ANNOUNCE);
    msg.target_specifier = 0xFFFFFFFF;  // Global broadcast
    msg.flags = CTRL_FLAG_GLOBAL;

    int written = control_message_serialize(&msg, nullptr, buffer, sizeof(buffer));
    ASSERT_GT(written, 0);

    control_message_t deserialized;
    int read = control_message_deserialize(buffer, written, &deserialized, nullptr, 0);

    ASSERT_GT(read, 0);
    ASSERT_EQ(deserialized.target_specifier, 0xFFFFFFFF);
    ASSERT_EQ(deserialized.flags & CTRL_FLAG_GLOBAL, CTRL_FLAG_GLOBAL);
}

TEST_F(ControlMessageTest, TargetSpecifierCluster)
{
    init_control_message(CTRL_MSG_RECOVERY_SYNC);
    msg.target_specifier = 0xABCD0000;  // Cluster ID

    int written = control_message_serialize(&msg, nullptr, buffer, sizeof(buffer));
    ASSERT_GT(written, 0);

    control_message_t deserialized;
    int read = control_message_deserialize(buffer, written, &deserialized, nullptr, 0);

    ASSERT_GT(read, 0);
    ASSERT_EQ(deserialized.target_specifier, 0xABCD0000);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(ControlMessageTest, CompleteMessageRoundtrip)
{
    neuromod_level_payload_t payload;
    payload.neuromod_type = 3;  // NOREPINEPHRINE
    payload.reserved = 0;
    payload.region_id = 777;
    payload.concentration = 0.91f;
    payload.timestamp = test_timestamp;

    init_control_message(CTRL_MSG_NEUROMOD_LEVEL, sizeof(payload));
    msg.flags = CTRL_FLAG_ACK_REQUIRED | CTRL_FLAG_RELAY;
    msg.sequence_number = 42;

    // Serialize
    int written = control_message_serialize(&msg, &payload, buffer, sizeof(buffer));
    ASSERT_GT(written, 0);

    // Deserialize
    control_message_t deserialized;
    neuromod_level_payload_t deserialized_payload;
    int read = control_message_deserialize(buffer, written, &deserialized, &deserialized_payload,
                                           sizeof(deserialized_payload));

    ASSERT_GT(read, 0);
    ASSERT_EQ(read, written);

    // Verify all fields
    ASSERT_EQ(deserialized.version, PROTOCOL_VERSION);
    ASSERT_EQ(deserialized.msg_type, CTRL_MSG_NEUROMOD_LEVEL);
    ASSERT_EQ(deserialized.flags, CTRL_FLAG_ACK_REQUIRED | CTRL_FLAG_RELAY);
    ASSERT_EQ(deserialized.source_node_id, 0x12345678);
    ASSERT_EQ(deserialized.target_specifier, 0xFFFFFFFF);
    ASSERT_EQ(deserialized.sequence_number, 42);

    ASSERT_EQ(deserialized_payload.neuromod_type, 3);
    ASSERT_EQ(deserialized_payload.region_id, 777);
    ASSERT_FLOAT_EQ(deserialized_payload.concentration, 0.91f);
    ASSERT_EQ(deserialized_payload.timestamp, test_timestamp);
}

TEST_F(ControlMessageTest, MultipleMessagesInSequence)
{
    for (int i = 0; i < 5; i++) {
        memset(buffer, 0, sizeof(buffer));

        init_control_message(CTRL_MSG_HEARTBEAT);
        msg.sequence_number = i;

        int written = control_message_serialize(&msg, nullptr, buffer, sizeof(buffer));
        ASSERT_GT(written, 0);

        control_message_t deserialized;
        int read = control_message_deserialize(buffer, written, &deserialized, nullptr, 0);

        ASSERT_GT(read, 0);
        ASSERT_EQ(deserialized.sequence_number, (uint32_t)i);
    }
}
