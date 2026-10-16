/**
 * @file test_networking_immune_bridges.cpp
 * @brief Unit tests for Networking-Immune Bridge modules
 * @version 1.0.0
 * @date 2025-12-12
 *
 * Tests for:
 * - Distributed Cognition-Immune Bridge
 * - Protocol-Immune Bridge
 * - P2P-Immune Bridge
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "networking/immune/nimcp_distributed_immune_bridge.h"
#include "networking/immune/nimcp_protocol_immune_bridge.h"
#include "networking/immune/nimcp_p2p_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Distributed-Immune Bridge Test Fixture
 * ============================================================================ */

class DistributedImmuneBridgeTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system = nullptr;
    distributed_cognition_t mock_distributed;
    distributed_immune_bridge_t* bridge = nullptr;

    void SetUp() override {
        /* Create brain immune system */
        brain_immune_config_t config;
        brain_immune_default_config(&config);
        immune_system = brain_immune_create(&config);
        ASSERT_NE(immune_system, nullptr);
        brain_immune_start(immune_system);

        /* Initialize mock distributed cognition */
        memset(&mock_distributed, 0, sizeof(mock_distributed));

        /* Create bridge */
        distributed_immune_config_t bridge_config;
        distributed_immune_default_config(&bridge_config);
        bridge = distributed_immune_bridge_create(
            &bridge_config,
            immune_system,
            &mock_distributed
        );
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            distributed_immune_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
            immune_system = nullptr;
        }
    }
};

/* ============================================================================
 * Distributed-Immune Bridge Lifecycle Tests
 * ============================================================================ */

TEST_F(DistributedImmuneBridgeTest, DefaultConfigIsValid) {
    distributed_immune_config_t config;
    int result = distributed_immune_default_config(&config);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_congestion_inflammation);
    EXPECT_TRUE(config.enable_peer_failure_antigens);
}

TEST_F(DistributedImmuneBridgeTest, DefaultConfigNullFails) {
    int result = distributed_immune_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(DistributedImmuneBridgeTest, CreateWithNullImmuneFails) {
    distributed_immune_bridge_t* b = distributed_immune_bridge_create(
        nullptr, nullptr, &mock_distributed
    );
    EXPECT_EQ(b, nullptr);
}

TEST_F(DistributedImmuneBridgeTest, CreateWithNullNetworkFails) {
    distributed_immune_bridge_t* b = distributed_immune_bridge_create(
        nullptr, immune_system, nullptr
    );
    EXPECT_EQ(b, nullptr);
}

TEST_F(DistributedImmuneBridgeTest, CreateWithDefaultConfig) {
    distributed_immune_bridge_t* b = distributed_immune_bridge_create(
        nullptr, immune_system, &mock_distributed
    );
    ASSERT_NE(b, nullptr);
    distributed_immune_bridge_destroy(b);
}

TEST_F(DistributedImmuneBridgeTest, DestroyNull) {
    distributed_immune_bridge_destroy(nullptr);
}

/* ============================================================================
 * Distributed-Immune Bridge Update Tests
 * ============================================================================ */

TEST_F(DistributedImmuneBridgeTest, BridgeUpdate) {
    int result = distributed_immune_bridge_update(bridge, 100);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(bridge->total_updates, 0u);
}

TEST_F(DistributedImmuneBridgeTest, BridgeUpdateNull) {
    int result = distributed_immune_bridge_update(nullptr, 100);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Distributed-Immune Bridge Bio-Async Tests
 * ============================================================================ */

TEST_F(DistributedImmuneBridgeTest, ConnectBioAsync) {
    int result = distributed_immune_connect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_TRUE(distributed_immune_is_bio_async_connected(bridge));
}

TEST_F(DistributedImmuneBridgeTest, DisconnectBioAsync) {
    distributed_immune_connect_bio_async(bridge);
    int result = distributed_immune_disconnect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FALSE(distributed_immune_is_bio_async_connected(bridge));
}

/* ============================================================================
 * Protocol-Immune Bridge Test Fixture
 * ============================================================================ */

class ProtocolImmuneBridgeTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system = nullptr;
    nlp_node_t mock_nlp_node;
    protocol_immune_bridge_t* bridge = nullptr;

    void SetUp() override {
        /* Create brain immune system */
        brain_immune_config_t config;
        brain_immune_default_config(&config);
        immune_system = brain_immune_create(&config);
        ASSERT_NE(immune_system, nullptr);
        brain_immune_start(immune_system);

        /* Initialize mock NLP node */
        memset(&mock_nlp_node, 0, sizeof(mock_nlp_node));

        /* Create bridge */
        protocol_immune_config_t bridge_config;
        protocol_immune_default_config(&bridge_config);
        bridge = protocol_immune_bridge_create(
            &bridge_config,
            immune_system,
            &mock_nlp_node
        );
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            protocol_immune_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
            immune_system = nullptr;
        }
    }
};

/* ============================================================================
 * Protocol-Immune Bridge Lifecycle Tests
 * ============================================================================ */

TEST_F(ProtocolImmuneBridgeTest, DefaultConfigIsValid) {
    protocol_immune_config_t config;
    int result = protocol_immune_default_config(&config);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_protocol_violation_antigens);
}

TEST_F(ProtocolImmuneBridgeTest, DefaultConfigNullFails) {
    int result = protocol_immune_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(ProtocolImmuneBridgeTest, CreateWithDefaultConfig) {
    protocol_immune_bridge_t* b = protocol_immune_bridge_create(
        nullptr, immune_system, &mock_nlp_node
    );
    ASSERT_NE(b, nullptr);
    protocol_immune_bridge_destroy(b);
}

TEST_F(ProtocolImmuneBridgeTest, DestroyNull) {
    protocol_immune_bridge_destroy(nullptr);
}

/* ============================================================================
 * Protocol-Immune Bridge Update Tests
 * ============================================================================ */

TEST_F(ProtocolImmuneBridgeTest, BridgeUpdate) {
    int result = protocol_immune_bridge_update(bridge, 100);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(bridge->total_updates, 0u);
}

TEST_F(ProtocolImmuneBridgeTest, BridgeUpdateNull) {
    int result = protocol_immune_bridge_update(nullptr, 100);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Protocol-Immune Bridge Bio-Async Tests
 * ============================================================================ */

TEST_F(ProtocolImmuneBridgeTest, ConnectBioAsync) {
    int result = protocol_immune_connect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_TRUE(protocol_immune_is_bio_async_connected(bridge));
}

TEST_F(ProtocolImmuneBridgeTest, DisconnectBioAsync) {
    protocol_immune_connect_bio_async(bridge);
    int result = protocol_immune_disconnect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FALSE(protocol_immune_is_bio_async_connected(bridge));
}

/* ============================================================================
 * P2P-Immune Bridge Test Fixture
 * ============================================================================ */

class P2PImmuneBridgeTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system = nullptr;
    p2p_network_t mock_p2p;
    p2p_immune_bridge_t* bridge = nullptr;

    void SetUp() override {
        /* Create brain immune system */
        brain_immune_config_t config;
        brain_immune_default_config(&config);
        immune_system = brain_immune_create(&config);
        ASSERT_NE(immune_system, nullptr);
        brain_immune_start(immune_system);

        /* Initialize mock P2P network */
        memset(&mock_p2p, 0, sizeof(mock_p2p));

        /* Create bridge */
        p2p_immune_config_t bridge_config;
        p2p_immune_default_config(&bridge_config);
        bridge = p2p_immune_bridge_create(
            &bridge_config,
            immune_system,
            &mock_p2p
        );
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            p2p_immune_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
            immune_system = nullptr;
        }
    }
};

/* ============================================================================
 * P2P-Immune Bridge Lifecycle Tests
 * ============================================================================ */

TEST_F(P2PImmuneBridgeTest, DefaultConfigIsValid) {
    p2p_immune_config_t config;
    int result = p2p_immune_default_config(&config);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_peer_reputation_antigens);
}

TEST_F(P2PImmuneBridgeTest, DefaultConfigNullFails) {
    int result = p2p_immune_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(P2PImmuneBridgeTest, CreateWithDefaultConfig) {
    p2p_immune_bridge_t* b = p2p_immune_bridge_create(
        nullptr, immune_system, &mock_p2p
    );
    ASSERT_NE(b, nullptr);
    p2p_immune_bridge_destroy(b);
}

TEST_F(P2PImmuneBridgeTest, DestroyNull) {
    p2p_immune_bridge_destroy(nullptr);
}

/* ============================================================================
 * P2P-Immune Bridge Update Tests
 * ============================================================================ */

TEST_F(P2PImmuneBridgeTest, BridgeUpdate) {
    int result = p2p_immune_bridge_update(bridge, 100);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(bridge->total_updates, 0u);
}

TEST_F(P2PImmuneBridgeTest, BridgeUpdateNull) {
    int result = p2p_immune_bridge_update(nullptr, 100);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * P2P-Immune Bridge Bio-Async Tests
 * ============================================================================ */

TEST_F(P2PImmuneBridgeTest, ConnectBioAsync) {
    int result = p2p_immune_connect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_TRUE(p2p_immune_is_bio_async_connected(bridge));
}

TEST_F(P2PImmuneBridgeTest, DisconnectBioAsync) {
    p2p_immune_connect_bio_async(bridge);
    int result = p2p_immune_disconnect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FALSE(p2p_immune_is_bio_async_connected(bridge));
}
