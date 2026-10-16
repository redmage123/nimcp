/**
 * @file test_mesh_types.cpp
 * @brief Unit tests for mesh network types
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "mesh/nimcp_mesh_types.h"
}

/* ============================================================================
 * Participant ID Tests
 * ============================================================================ */

TEST(MeshTypesTest, MakeParticipantId) {
    mesh_channel_id_t channel = MESH_CHANNEL_LEFT_HEMISPHERE;
    mesh_participant_type_t type = MESH_PARTICIPANT_MODULE;
    uint32_t local_id = 42;

    mesh_participant_id_t id = mesh_make_participant_id(channel, type, local_id);

    EXPECT_EQ(mesh_get_channel(id), channel);
    EXPECT_EQ(mesh_get_participant_type(id), type);
    EXPECT_EQ(mesh_get_local_id(id), local_id);
}

TEST(MeshTypesTest, ParticipantIdEncoding) {
    /* Test that encoding is correct */
    mesh_participant_id_t id = mesh_make_participant_id(
        MESH_CHANNEL_GPU_COMPUTE,
        MESH_PARTICIPANT_COORDINATOR,
        0xDEADBEEF
    );

    /* Upper 16 bits should be channel */
    EXPECT_EQ((id >> 48) & 0xFFFF, MESH_CHANNEL_GPU_COMPUTE);

    /* Next 16 bits should be type */
    EXPECT_EQ((id >> 32) & 0xFFFF, MESH_PARTICIPANT_COORDINATOR);

    /* Lower 32 bits should be local ID */
    EXPECT_EQ(id & 0xFFFFFFFF, 0xDEADBEEF);
}

TEST(MeshTypesTest, ParticipantIdExtraction) {
    mesh_participant_id_t id = 0x000100020000002AULL;

    EXPECT_EQ(mesh_get_channel(id), 1);
    EXPECT_EQ(mesh_get_participant_type(id), MESH_PARTICIPANT_COORDINATOR);
    EXPECT_EQ(mesh_get_local_id(id), 42);
}

/* ============================================================================
 * Transaction ID Tests
 * ============================================================================ */

TEST(MeshTypesTest, TxIdCompareSame) {
    mesh_tx_id_t a = {
        .channel = 1,
        .proposer = 0x1234,
        .sequence = 100,
        .timestamp_ns = 1000000
    };
    mesh_tx_id_t b = a;

    EXPECT_EQ(mesh_tx_id_compare(&a, &b), 0);
}

TEST(MeshTypesTest, TxIdCompareDifferentChannel) {
    mesh_tx_id_t a = {.channel = 1, .proposer = 0x1234, .sequence = 100, .timestamp_ns = 1000};
    mesh_tx_id_t b = {.channel = 2, .proposer = 0x1234, .sequence = 100, .timestamp_ns = 1000};

    EXPECT_LT(mesh_tx_id_compare(&a, &b), 0);
    EXPECT_GT(mesh_tx_id_compare(&b, &a), 0);
}

TEST(MeshTypesTest, TxIdCompareDifferentSequence) {
    mesh_tx_id_t a = {.channel = 1, .proposer = 0x1234, .sequence = 100, .timestamp_ns = 1000};
    mesh_tx_id_t b = {.channel = 1, .proposer = 0x1234, .sequence = 200, .timestamp_ns = 1000};

    EXPECT_LT(mesh_tx_id_compare(&a, &b), 0);
}

TEST(MeshTypesTest, TxIdIsValid) {
    mesh_tx_id_t valid = {
        .channel = 1,
        .proposer = 0x1234,
        .sequence = 100,
        .timestamp_ns = 1000000
    };
    EXPECT_TRUE(mesh_tx_id_is_valid(&valid));

    mesh_tx_id_t invalid_proposer = {0};
    EXPECT_FALSE(mesh_tx_id_is_valid(&invalid_proposer));

    mesh_tx_id_t invalid_timestamp = {.channel = 1, .proposer = 0x1234, .sequence = 0, .timestamp_ns = 0};
    EXPECT_FALSE(mesh_tx_id_is_valid(&invalid_timestamp));

    EXPECT_FALSE(mesh_tx_id_is_valid(NULL));
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST(MeshTypesTest, ParticipantTypeToString) {
    EXPECT_STREQ(mesh_participant_type_to_string(MESH_PARTICIPANT_NONE), "NONE");
    EXPECT_STREQ(mesh_participant_type_to_string(MESH_PARTICIPANT_MODULE), "MODULE");
    EXPECT_STREQ(mesh_participant_type_to_string(MESH_PARTICIPANT_COORDINATOR), "COORDINATOR");
    EXPECT_STREQ(mesh_participant_type_to_string(MESH_PARTICIPANT_ORDERER), "ORDERER");
    EXPECT_STREQ(mesh_participant_type_to_string(MESH_PARTICIPANT_GATEWAY), "GATEWAY");
    EXPECT_STREQ(mesh_participant_type_to_string(MESH_PARTICIPANT_OBSERVER), "OBSERVER");
    EXPECT_STREQ(mesh_participant_type_to_string((mesh_participant_type_t)999), "UNKNOWN");
}

TEST(MeshTypesTest, CoordinatorRoleToString) {
    EXPECT_STREQ(mesh_coordinator_role_to_string(COORD_ROLE_NONE), "NONE");
    EXPECT_STREQ(mesh_coordinator_role_to_string(COORD_ROLE_LEADER), "LEADER");
    EXPECT_STREQ(mesh_coordinator_role_to_string(COORD_ROLE_WORKER), "WORKER");
    EXPECT_STREQ(mesh_coordinator_role_to_string(COORD_ROLE_STANDBY), "STANDBY");
    EXPECT_STREQ(mesh_coordinator_role_to_string(COORD_ROLE_FOLLOWER), "FOLLOWER");
}

TEST(MeshTypesTest, CoordinatorStateToString) {
    EXPECT_STREQ(mesh_coordinator_state_to_string(COORD_STATE_INIT), "INIT");
    EXPECT_STREQ(mesh_coordinator_state_to_string(COORD_STATE_ACTIVE), "ACTIVE");
    EXPECT_STREQ(mesh_coordinator_state_to_string(COORD_STATE_ELECTION), "ELECTION");
    EXPECT_STREQ(mesh_coordinator_state_to_string(COORD_STATE_FAILED), "FAILED");
}

TEST(MeshTypesTest, TxTypeToString) {
    EXPECT_STREQ(mesh_tx_type_to_string(MESH_TX_BELIEF_UPDATE), "BELIEF_UPDATE");
    EXPECT_STREQ(mesh_tx_type_to_string(MESH_TX_STATE_CHANGE), "STATE_CHANGE");
    EXPECT_STREQ(mesh_tx_type_to_string(MESH_TX_CONSENSUS_VOTE), "CONSENSUS_VOTE");
    EXPECT_STREQ(mesh_tx_type_to_string(MESH_TX_CROSS_CHANNEL), "CROSS_CHANNEL");
}

TEST(MeshTypesTest, TxStatusToString) {
    EXPECT_STREQ(mesh_tx_status_to_string(MESH_TX_STATUS_PROPOSED), "PROPOSED");
    EXPECT_STREQ(mesh_tx_status_to_string(MESH_TX_STATUS_ENDORSING), "ENDORSING");
    EXPECT_STREQ(mesh_tx_status_to_string(MESH_TX_STATUS_COMMITTED), "COMMITTED");
    EXPECT_STREQ(mesh_tx_status_to_string(MESH_TX_STATUS_FAILED), "FAILED");
}

TEST(MeshTypesTest, EndorsementResultToString) {
    EXPECT_STREQ(mesh_endorsement_result_to_string(ENDORSEMENT_APPROVED), "APPROVED");
    EXPECT_STREQ(mesh_endorsement_result_to_string(ENDORSEMENT_REJECTED), "REJECTED");
    EXPECT_STREQ(mesh_endorsement_result_to_string(ENDORSEMENT_ABSTAIN), "ABSTAIN");
}

TEST(MeshTypesTest, CredentialStateToString) {
    EXPECT_STREQ(mesh_credential_state_to_string(CREDENTIAL_STATE_VALID), "VALID");
    EXPECT_STREQ(mesh_credential_state_to_string(CREDENTIAL_STATE_REVOKED), "REVOKED");
    EXPECT_STREQ(mesh_credential_state_to_string(CREDENTIAL_STATE_EXPIRED), "EXPIRED");
}

TEST(MeshTypesTest, ChannelName) {
    EXPECT_STREQ(mesh_channel_name(MESH_CHANNEL_SYSTEM), "SYSTEM");
    EXPECT_STREQ(mesh_channel_name(MESH_CHANNEL_LEFT_HEMISPHERE), "LEFT_HEMISPHERE");
    EXPECT_STREQ(mesh_channel_name(MESH_CHANNEL_RIGHT_HEMISPHERE), "RIGHT_HEMISPHERE");
    EXPECT_STREQ(mesh_channel_name(MESH_CHANNEL_SUBCORTICAL), "SUBCORTICAL");
    EXPECT_STREQ(mesh_channel_name(MESH_CHANNEL_GPU_COMPUTE), "GPU_COMPUTE");
    EXPECT_STREQ(mesh_channel_name(100), "CUSTOM");
}

/* ============================================================================
 * Structure Tests
 * ============================================================================ */

TEST(MeshTypesTest, BeliefStructureSize) {
    /* Verify belief vector dimension */
    EXPECT_EQ(MESH_BELIEF_VECTOR_DIM, 64);

    mesh_belief_t belief = {0};
    EXPECT_EQ(sizeof(belief.belief_vector) / sizeof(float), MESH_BELIEF_VECTOR_DIM);
}

TEST(MeshTypesTest, CredentialStructure) {
    credential_t cred = {0};

    cred.state = CREDENTIAL_STATE_VALID;
    cred.privilege_level = 5;
    cred.issued_at_ns = 1000000;
    cred.expires_at_ns = 2000000;
    cred.capabilities = 0xFFFF;

    EXPECT_EQ(cred.state, CREDENTIAL_STATE_VALID);
    EXPECT_EQ(cred.privilege_level, 5);
    EXPECT_EQ(cred.capabilities, 0xFFFF);
}

TEST(MeshTypesTest, TimingStructure) {
    mesh_timing_t timing = {
        .base_interval_ms = 100.0f,
        .jitter_amplitude_ms = 25.0f,
        .min_interval_ms = 50.0f,
        .max_interval_ms = 200.0f
    };

    EXPECT_FLOAT_EQ(timing.base_interval_ms, 100.0f);
    EXPECT_FLOAT_EQ(timing.jitter_amplitude_ms, 25.0f);
}

/* ============================================================================
 * Constants Tests
 * ============================================================================ */

TEST(MeshTypesTest, Constants) {
    EXPECT_EQ(NIMCP_MESH_MAGIC, 0x4D455348);
    EXPECT_GT(MESH_MAX_PARTICIPANTS_PER_CHANNEL, 0);
    EXPECT_GT(MESH_MAX_CHANNELS, 0);
    EXPECT_GT(MESH_MAX_COORDINATORS_PER_POOL, 0);
    EXPECT_GT(MESH_MAX_PENDING_TRANSACTIONS, 0);
    EXPECT_GT(MESH_MAX_PAYLOAD_SIZE, 0);
}

/* ============================================================================
 * Well-Known Channels
 * ============================================================================ */

TEST(MeshTypesTest, WellKnownChannels) {
    EXPECT_EQ(MESH_CHANNEL_SYSTEM, 0);
    EXPECT_EQ(MESH_CHANNEL_LEFT_HEMISPHERE, 1);
    EXPECT_EQ(MESH_CHANNEL_RIGHT_HEMISPHERE, 2);
    EXPECT_EQ(MESH_CHANNEL_SUBCORTICAL, 3);
    EXPECT_EQ(MESH_CHANNEL_GPU_COMPUTE, 4);
}
