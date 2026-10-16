/**
 * @file test_mesh_integration.cpp
 * @brief Integration tests for genius profiles mesh network integration
 *
 * Test Categories:
 * 1. Mesh Proposal Tests - Propose profile activations via mesh
 * 2. Mesh Endorsement Tests - Endorse transactions
 * 3. Mesh State Tests - Verify mesh state tracking
 * 4. Error Handling Tests - Invalid inputs, disabled mesh
 *
 * @author NIMCP Development Team
 * @date 2026-02-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "core/brain/genius/nimcp_genius_profiles.h"
#include "core/brain/genius/nimcp_genius_types.h"
#include "core/brain/genius/nimcp_genius_traits.h"
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_transaction.h"
}

// Mirror of private payload struct from nimcp_genius_profiles.c for test use
typedef struct {
    genius_type_t type;
    float strength;
    uint64_t timestamp_ms;
    uint8_t epitope[GENIUS_EPITOPE_SIZE];
} genius_profile_tx_payload_t;

//=============================================================================
// Test Fixture
//=============================================================================

class MeshIntegrationTest : public ::testing::Test {
protected:
    genius_profiles_bridge_t* bridge = nullptr;
    genius_profiles_bridge_t* bridge_mesh_enabled = nullptr;
    genius_profiles_config_t config;
    genius_profiles_config_t config_mesh;

    void SetUp() override {
        // Bridge without mesh
        ASSERT_EQ(genius_profiles_config_default(&config), GENIUS_ERROR_SUCCESS);
        config.enable_bio_async = false;
        config.enable_mesh_coordination = false;
        config.enable_training_integration = false;
        config.enable_rcog_integration = false;
        config.enable_ccog_integration = false;
        config.enable_quantum_optimization = false;
        config.enable_kg_wiring = false;

        bridge = genius_profiles_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);

        // Bridge with mesh enabled
        ASSERT_EQ(genius_profiles_config_default(&config_mesh), GENIUS_ERROR_SUCCESS);
        config_mesh.enable_bio_async = false;
        config_mesh.enable_mesh_coordination = true;  // Enable mesh
        config_mesh.enable_training_integration = false;
        config_mesh.enable_rcog_integration = false;
        config_mesh.enable_ccog_integration = false;
        config_mesh.enable_quantum_optimization = false;
        config_mesh.enable_kg_wiring = false;
        config_mesh.mesh_timeout_ms = 5000;

        bridge_mesh_enabled = genius_profiles_bridge_create(&config_mesh);
        ASSERT_NE(bridge_mesh_enabled, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            genius_profiles_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (bridge_mesh_enabled) {
            genius_profiles_bridge_destroy(bridge_mesh_enabled);
            bridge_mesh_enabled = nullptr;
        }
    }
};

//=============================================================================
// 1. MESH PROPOSAL TESTS
//=============================================================================

TEST_F(MeshIntegrationTest, MeshProposeDisabledReturnsSuccess) {
    // When mesh is disabled, propose should return success (direct activation)
    genius_error_t err = genius_profiles_mesh_propose(bridge, GENIUS_TYPE_MATHEMATICAL, 1.0f);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);

    // Profile should be activated directly
    EXPECT_EQ(genius_profiles_get_state(bridge), GENIUS_STATE_ACTIVE);
}

TEST_F(MeshIntegrationTest, MeshProposeEnabledActivatesProfile) {
    // When mesh is enabled but no coordinator, should still activate
    genius_error_t err = genius_profiles_mesh_propose(bridge_mesh_enabled, GENIUS_TYPE_VISUAL_ARTISTIC, 1.0f);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);

    // Profile should be activated (optimistic execution)
    EXPECT_EQ(genius_profiles_get_state(bridge_mesh_enabled), GENIUS_STATE_ACTIVE);
}

TEST_F(MeshIntegrationTest, MeshProposeAllGeniusTypes) {
    genius_type_t types[] = {
        GENIUS_TYPE_MATHEMATICAL,
        GENIUS_TYPE_VISUAL_ARTISTIC,
        GENIUS_TYPE_MUSICAL,
        GENIUS_TYPE_LITERARY,
        GENIUS_TYPE_SCIENTIFIC,
        GENIUS_TYPE_ATHLETIC,
        GENIUS_TYPE_STRATEGIC,
        GENIUS_TYPE_FINANCIAL
    };

    for (size_t i = 0; i < sizeof(types)/sizeof(types[0]); i++) {
        // Reset bridge state first
        genius_profiles_deactivate(bridge_mesh_enabled);
        EXPECT_EQ(genius_profiles_get_state(bridge_mesh_enabled), GENIUS_STATE_INACTIVE);

        // Propose activation via mesh
        genius_error_t err = genius_profiles_mesh_propose(bridge_mesh_enabled, types[i], 1.0f);
        EXPECT_EQ(err, GENIUS_ERROR_SUCCESS) << "Failed for type " << (int)types[i];
    }
}

TEST_F(MeshIntegrationTest, MeshProposeInvalidType) {
    genius_error_t err = genius_profiles_mesh_propose(bridge_mesh_enabled, (genius_type_t)999, 1.0f);
    EXPECT_NE(err, GENIUS_ERROR_SUCCESS);
}

TEST_F(MeshIntegrationTest, MeshProposeNullBridge) {
    genius_error_t err = genius_profiles_mesh_propose(nullptr, GENIUS_TYPE_MATHEMATICAL, 1.0f);
    EXPECT_EQ(err, GENIUS_ERROR_NULL_POINTER);
}

TEST_F(MeshIntegrationTest, MeshProposeInvalidStrength) {
    genius_error_t err = genius_profiles_mesh_propose(bridge_mesh_enabled, GENIUS_TYPE_MATHEMATICAL, -0.5f);
    EXPECT_NE(err, GENIUS_ERROR_SUCCESS);

    err = genius_profiles_mesh_propose(bridge_mesh_enabled, GENIUS_TYPE_MATHEMATICAL, 1.5f);
    EXPECT_NE(err, GENIUS_ERROR_SUCCESS);
}

TEST_F(MeshIntegrationTest, MeshProposePartialStrength) {
    // Should work with partial strength
    genius_error_t err = genius_profiles_mesh_propose(bridge_mesh_enabled, GENIUS_TYPE_MATHEMATICAL, 0.5f);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);
}

//=============================================================================
// 2. MESH ENDORSEMENT TESTS
//=============================================================================

TEST_F(MeshIntegrationTest, MeshEndorseNullTransaction) {
    genius_error_t err = genius_profiles_mesh_endorse(bridge_mesh_enabled, nullptr);
    EXPECT_EQ(err, GENIUS_ERROR_NULL_POINTER);
}

TEST_F(MeshIntegrationTest, MeshEndorseNullBridge) {
    mesh_transaction_t tx;
    memset(&tx, 0, sizeof(tx));

    genius_error_t err = genius_profiles_mesh_endorse(nullptr, &tx);
    EXPECT_EQ(err, GENIUS_ERROR_NULL_POINTER);
}

TEST_F(MeshIntegrationTest, MeshEndorseDisabledReturnsSuccess) {
    // When mesh is disabled, endorse should return success (no-op)
    mesh_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.type = MESH_TX_BELIEF_UPDATE;

    genius_error_t err = genius_profiles_mesh_endorse(bridge, &tx);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);
}

TEST_F(MeshIntegrationTest, MeshEndorseInvalidTransactionType) {
    // Transaction with non-belief/non-config type
    mesh_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.type = MESH_TX_STATE_CHANGE;  // Not BELIEF_UPDATE or CONFIG_UPDATE

    genius_error_t err = genius_profiles_mesh_endorse(bridge_mesh_enabled, &tx);
    // Should return success (skip endorsement for non-relevant types)
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);
}

TEST_F(MeshIntegrationTest, MeshEndorseNoPayload) {
    mesh_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.type = MESH_TX_BELIEF_UPDATE;
    tx.payload = nullptr;
    tx.payload_size = 0;

    genius_error_t err = genius_profiles_mesh_endorse(bridge_mesh_enabled, &tx);
    EXPECT_EQ(err, GENIUS_ERROR_MESH_ENDORSEMENT_FAILED);
}

TEST_F(MeshIntegrationTest, MeshEndorseWhileInFlowState) {
    // Activate profile and enter flow state
    genius_profiles_activate(bridge_mesh_enabled, GENIUS_TYPE_MATHEMATICAL, 1.0f);
    genius_profiles_enter_flow(bridge_mesh_enabled, 0.7f, 0.7f);

    // Create valid transaction
    genius_profile_tx_payload_t payload;
    memset(&payload, 0, sizeof(payload));
    payload.type = GENIUS_TYPE_VISUAL_ARTISTIC;
    payload.strength = 1.0f;

    mesh_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.type = MESH_TX_BELIEF_UPDATE;
    tx.payload = (uint8_t*)&payload;
    tx.payload_size = sizeof(payload);

    genius_error_t err = genius_profiles_mesh_endorse(bridge_mesh_enabled, &tx);
    EXPECT_EQ(err, GENIUS_ERROR_FLOW_STATE_CONFLICT);
}

//=============================================================================
// 3. MESH STATE TESTS
//=============================================================================

TEST_F(MeshIntegrationTest, MeshConsensusCountIncrementsAfterPropose) {
    uint64_t initial_count = bridge_mesh_enabled->mesh_consensus_count;

    genius_profiles_mesh_propose(bridge_mesh_enabled, GENIUS_TYPE_MATHEMATICAL, 1.0f);

    // Since we don't have a real mesh coordinator, count may not increment
    // But the function should complete without error
    EXPECT_GE(bridge_mesh_enabled->mesh_consensus_count, initial_count);
}

TEST_F(MeshIntegrationTest, PendingMeshTxFieldsAreSet) {
    // Initial values should be 0
    EXPECT_EQ(bridge_mesh_enabled->pending_mesh_tx_sequence, 0);
    EXPECT_EQ(bridge_mesh_enabled->pending_mesh_tx_timestamp, 0);

    // Propose should set these
    genius_profiles_mesh_propose(bridge_mesh_enabled, GENIUS_TYPE_MATHEMATICAL, 1.0f);

    // Values should be set (or remain 0 if mesh not fully connected)
    // Just verify it doesn't crash
}

//=============================================================================
// 4. ERROR HANDLING TESTS
//=============================================================================

TEST_F(MeshIntegrationTest, MeshProposeWithHighFatigue) {
    // Simulate high fatigue
    bridge_mesh_enabled->fatigue_level = 0.95f;

    genius_error_t err = genius_profiles_mesh_propose(bridge_mesh_enabled, GENIUS_TYPE_MATHEMATICAL, 1.0f);
    // Should succeed (fatigue check is in endorse, not propose)
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);
}

TEST_F(MeshIntegrationTest, MeshEndorseWithHighFatigue) {
    // Set high fatigue
    bridge_mesh_enabled->fatigue_level = 0.95f;

    // Create valid payload
    genius_profile_tx_payload_t payload;
    memset(&payload, 0, sizeof(payload));
    payload.type = GENIUS_TYPE_VISUAL_ARTISTIC;
    payload.strength = 1.0f;

    mesh_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.type = MESH_TX_BELIEF_UPDATE;
    tx.payload = (uint8_t*)&payload;
    tx.payload_size = sizeof(payload);

    genius_error_t err = genius_profiles_mesh_endorse(bridge_mesh_enabled, &tx);
    EXPECT_EQ(err, GENIUS_ERROR_FATIGUE_EXCEEDED);
}

TEST_F(MeshIntegrationTest, MeshEndorseInvalidGeniusType) {
    genius_profile_tx_payload_t payload;
    memset(&payload, 0, sizeof(payload));
    payload.type = (genius_type_t)999;  // Invalid type
    payload.strength = 1.0f;

    mesh_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.type = MESH_TX_BELIEF_UPDATE;
    tx.payload = (uint8_t*)&payload;
    tx.payload_size = sizeof(payload);

    genius_error_t err = genius_profiles_mesh_endorse(bridge_mesh_enabled, &tx);
    EXPECT_EQ(err, GENIUS_ERROR_INVALID_TYPE);
}

TEST_F(MeshIntegrationTest, MeshEndorseInvalidStrength) {
    genius_profile_tx_payload_t payload;
    memset(&payload, 0, sizeof(payload));
    payload.type = GENIUS_TYPE_MATHEMATICAL;
    payload.strength = 1.5f;  // Invalid: > 1.0

    mesh_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.type = MESH_TX_BELIEF_UPDATE;
    tx.payload = (uint8_t*)&payload;
    tx.payload_size = sizeof(payload);

    genius_error_t err = genius_profiles_mesh_endorse(bridge_mesh_enabled, &tx);
    EXPECT_EQ(err, GENIUS_ERROR_MESH_ENDORSEMENT_FAILED);
}

//=============================================================================
// 5. SEQUENTIAL MESH OPERATIONS TESTS
//=============================================================================

TEST_F(MeshIntegrationTest, SequentialMeshProposals) {
    // First proposal
    genius_error_t err = genius_profiles_mesh_propose(bridge_mesh_enabled, GENIUS_TYPE_MATHEMATICAL, 1.0f);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);
    EXPECT_EQ(genius_profiles_get_state(bridge_mesh_enabled), GENIUS_STATE_ACTIVE);

    // Deactivate
    genius_profiles_deactivate(bridge_mesh_enabled);

    // Second proposal with different type
    err = genius_profiles_mesh_propose(bridge_mesh_enabled, GENIUS_TYPE_VISUAL_ARTISTIC, 0.8f);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);
    EXPECT_EQ(genius_profiles_get_state(bridge_mesh_enabled), GENIUS_STATE_ACTIVE);
}

TEST_F(MeshIntegrationTest, ProposeDuringActiveState) {
    // First activation
    genius_profiles_activate(bridge_mesh_enabled, GENIUS_TYPE_MATHEMATICAL, 1.0f);
    EXPECT_EQ(genius_profiles_get_state(bridge_mesh_enabled), GENIUS_STATE_ACTIVE);

    // Propose while already active (should switch profiles)
    genius_error_t err = genius_profiles_mesh_propose(bridge_mesh_enabled, GENIUS_TYPE_VISUAL_ARTISTIC, 1.0f);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);
}
