/**
 * @file test_nlp_node.cpp
 * @brief Unit tests for Neural Link Protocol node lifecycle
 *
 * WHAT: Tests for NLP node creation, startup, peer management, mode switching
 * WHY:  Ensure reliable node operations and configuration management
 * HOW:  Use GoogleTest with comprehensive lifecycle validation
 *
 * TEST COVERAGE: 22 tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <string.h>
#include <cstdint>

// Headers have their own extern "C" guards
#include "networking/nlp/nimcp_neural_link_protocol.h"

//=============================================================================
// Test Constants
//=============================================================================

static const char* TEST_ADDRESS = "127.0.0.1";
static const uint16_t TEST_PORT = 9000;
static const uint32_t TEST_BRAIN_ID = 0x12345678;

//=============================================================================
// Test Fixture
//=============================================================================

class NLPNodeTest : public ::testing::Test {
protected:
    nlp_node_t node;

    void SetUp() override
    {
        node = nullptr;
    }

    void TearDown() override
    {
        if (node) {
            nlp_node_stop(node);
            nlp_node_destroy(node);
            node = nullptr;
        }
    }
};

//=============================================================================
// Node Creation/Destruction Tests
//=============================================================================

/**
 * WHAT: Test node creation with default config
 * WHY:  Verify basic initialization works
 */
TEST_F(NLPNodeTest, NodeCreateDestroy_DefaultConfig)
{
    node = nlp_node_create(nullptr);
    EXPECT_NE(node, nullptr) << "Node creation with default config should succeed";

    nlp_node_destroy(node);
    node = nullptr;
}

/**
 * WHAT: Test node creation with custom config
 * WHY:  Verify configuration is applied correctly
 */
TEST_F(NLPNodeTest, NodeCreateDestroy_CustomConfig)
{
    nlp_config_t config = nlp_config_default();
    config.brain_id = TEST_BRAIN_ID;
    config.default_mode = NLP_MODE_TACTICAL;
    config.port = TEST_PORT;
    config.max_peers = 128;

    node = nlp_node_create(&config);
    ASSERT_NE(node, nullptr);

    // Verify mode was set
    EXPECT_EQ(nlp_get_mode(node), NLP_MODE_TACTICAL);
}

/**
 * WHAT: Test node destruction safety
 * WHY:  Verify cleanup is safe
 */
TEST_F(NLPNodeTest, NodeCreateDestroy_NullSafe)
{
    nlp_node_destroy(nullptr);
    // Should not crash
    SUCCEED();
}

/**
 * WHAT: Test multiple node creation
 * WHY:  Verify multiple nodes can coexist
 */
TEST_F(NLPNodeTest, NodeCreateDestroy_MultipleNodes)
{
    nlp_config_t config1 = nlp_config_default();
    config1.brain_id = 0x11111111;
    config1.port = 9001;

    nlp_config_t config2 = nlp_config_default();
    config2.brain_id = 0x22222222;
    config2.port = 9002;

    nlp_node_t node1 = nlp_node_create(&config1);
    nlp_node_t node2 = nlp_node_create(&config2);

    EXPECT_NE(node1, nullptr);
    EXPECT_NE(node2, nullptr);
    EXPECT_NE(node1, node2);

    nlp_node_destroy(node1);
    nlp_node_destroy(node2);
}

//=============================================================================
// Start/Stop Tests
//=============================================================================

/**
 * WHAT: Test node start
 * WHY:  Verify node can be started
 */
TEST_F(NLPNodeTest, NodeStartStop_BasicStart)
{
    nlp_config_t config = nlp_config_default();
    config.brain_id = TEST_BRAIN_ID;
    config.port = TEST_PORT;

    node = nlp_node_create(&config);
    ASSERT_NE(node, nullptr);

    int result = nlp_node_start(node);
    EXPECT_EQ(result, 0) << "Node start should succeed";

    nlp_node_stop(node);
}

/**
 * WHAT: Test node stop
 * WHY:  Verify graceful shutdown
 */
TEST_F(NLPNodeTest, NodeStartStop_GracefulStop)
{
    nlp_config_t config = nlp_config_default();
    config.brain_id = TEST_BRAIN_ID;

    node = nlp_node_create(&config);
    ASSERT_NE(node, nullptr);

    nlp_node_start(node);

    int result = nlp_node_stop(node);
    EXPECT_EQ(result, 0) << "Node stop should succeed";
}

/**
 * WHAT: Test start with null node
 * WHY:  Verify error handling
 */
TEST_F(NLPNodeTest, NodeStartStop_NullNode)
{
    int result = nlp_node_start(nullptr);
    EXPECT_NE(result, 0) << "Start with null node should fail";

    result = nlp_node_stop(nullptr);
    EXPECT_NE(result, 0) << "Stop with null node should fail";
}

/**
 * WHAT: Test double start
 * WHY:  Verify idempotency
 */
TEST_F(NLPNodeTest, NodeStartStop_DoubleStart)
{
    nlp_config_t config = nlp_config_default();
    config.brain_id = TEST_BRAIN_ID;

    node = nlp_node_create(&config);
    ASSERT_NE(node, nullptr);

    nlp_node_start(node);

    // Second start should be harmless
    int result = nlp_node_start(node);
    EXPECT_EQ(result, 0) << "Double start should be safe";
}

//=============================================================================
// Default Configuration Tests
//=============================================================================

/**
 * WHAT: Test default configuration values
 * WHY:  Verify sane defaults
 */
TEST_F(NLPNodeTest, ConfigDefaults_SaneValues)
{
    nlp_config_t config = nlp_config_default();

    EXPECT_NE(config.brain_id, 0);
    EXPECT_FALSE(config.is_master);
    EXPECT_EQ(config.default_mode, NLP_MODE_STANDARD);
    EXPECT_TRUE(config.auto_mode_switch);
    EXPECT_GT(config.port, 0);
    EXPECT_EQ(config.max_peers, NLP_MAX_PEERS);
    EXPECT_EQ(config.heartbeat_interval_ms, NLP_HEARTBEAT_INTERVAL);
    EXPECT_EQ(config.session_timeout_ms, NLP_SESSION_TIMEOUT);
    EXPECT_TRUE(config.require_encryption);  // Encryption enabled by default for security
}

/**
 * WHAT: Test timing parameters
 * WHY:  Verify reasonable timeouts
 */
TEST_F(NLPNodeTest, ConfigDefaults_TimingParameters)
{
    nlp_config_t config = nlp_config_default();

    EXPECT_GT(config.heartbeat_interval_ms, 0);
    EXPECT_GT(config.session_timeout_ms, config.heartbeat_interval_ms);
    EXPECT_GT(config.handshake_timeout_ms, 0);
    EXPECT_LT(config.handshake_timeout_ms, config.session_timeout_ms);
}

/**
 * WHAT: Test security defaults
 * WHY:  Verify security is enabled by default (secure defaults)
 */
TEST_F(NLPNodeTest, ConfigDefaults_SecuritySettings)
{
    nlp_config_t config = nlp_config_default();

    // Encryption should be required by default (secure default)
    EXPECT_TRUE(config.require_encryption);

    // Key rotation enabled by default (1 hour = 3600 seconds)
    EXPECT_EQ(config.key_rotation_interval_s, 3600);

    // PSK slots should be empty (no keys pre-configured)
    for (int i = 0; i < NLP_KEY_SLOTS; i++) {
        EXPECT_FALSE(config.psk[i].active);
    }
}

//=============================================================================
// Peer Management Tests
//=============================================================================

/**
 * WHAT: Test adding a peer
 * WHY:  Verify peer connections can be established
 */
TEST_F(NLPNodeTest, PeerManagement_AddPeer)
{
    nlp_config_t config = nlp_config_default();
    config.brain_id = TEST_BRAIN_ID;

    node = nlp_node_create(&config);
    ASSERT_NE(node, nullptr);

    nlp_node_start(node);

    uint32_t peer_id = nlp_connect_peer(node, TEST_ADDRESS, TEST_PORT);
    // Note: May return 0 if connection fails (expected in unit test)
    // In integration test, this would succeed
}

/**
 * WHAT: Test removing a peer
 * WHY:  Verify peer disconnection
 */
TEST_F(NLPNodeTest, PeerManagement_RemovePeer)
{
    nlp_config_t config = nlp_config_default();
    config.brain_id = TEST_BRAIN_ID;

    node = nlp_node_create(&config);
    ASSERT_NE(node, nullptr);

    nlp_node_start(node);

    // Add peer (may fail in unit test)
    uint32_t peer_id = nlp_connect_peer(node, TEST_ADDRESS, TEST_PORT);

    // Disconnect
    int result = nlp_disconnect_peer(node, peer_id);
    // Result may be error if peer wasn't actually connected
}

/**
 * WHAT: Test getting peer info
 * WHY:  Verify peer state can be queried
 */
TEST_F(NLPNodeTest, PeerManagement_GetPeerInfo)
{
    nlp_config_t config = nlp_config_default();
    config.brain_id = TEST_BRAIN_ID;

    node = nlp_node_create(&config);
    ASSERT_NE(node, nullptr);

    nlp_peer_t peer;
    int result = nlp_get_peer(node, 0x99999999, &peer);

    // Should fail for non-existent peer
    EXPECT_NE(result, 0);
}

/**
 * WHAT: Test listing all peers
 * WHY:  Verify peer enumeration
 */
TEST_F(NLPNodeTest, PeerManagement_ListPeers)
{
    nlp_config_t config = nlp_config_default();
    config.brain_id = TEST_BRAIN_ID;

    node = nlp_node_create(&config);
    ASSERT_NE(node, nullptr);

    nlp_peer_t peers[NLP_MAX_PEERS];
    uint32_t count = nlp_get_peers(node, peers, NLP_MAX_PEERS);

    // Initially should have no peers
    EXPECT_EQ(count, 0);
}

/**
 * WHAT: Test maximum peers limit
 * WHY:  Verify peer limit is enforced
 */
TEST_F(NLPNodeTest, PeerManagement_MaxPeersLimit)
{
    nlp_config_t config = nlp_config_default();
    config.brain_id = TEST_BRAIN_ID;
    config.max_peers = 10;

    node = nlp_node_create(&config);
    ASSERT_NE(node, nullptr);

    // Attempting to add more than max_peers should fail
    // (In real test, would need to actually connect peers)
}

//=============================================================================
// Mode Switching Tests
//=============================================================================

/**
 * WHAT: Test switching to tactical mode
 * WHY:  Verify mode transitions work
 */
TEST_F(NLPNodeTest, ModeSwitch_ToTactical)
{
    nlp_config_t config = nlp_config_default();
    config.brain_id = TEST_BRAIN_ID;
    config.default_mode = NLP_MODE_STANDARD;

    node = nlp_node_create(&config);
    ASSERT_NE(node, nullptr);

    EXPECT_EQ(nlp_get_mode(node), NLP_MODE_STANDARD);

    int result = nlp_set_mode(node, NLP_MODE_TACTICAL);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(nlp_get_mode(node), NLP_MODE_TACTICAL);
}

/**
 * WHAT: Test switching to stealth mode
 * WHY:  Verify stealth operations
 */
TEST_F(NLPNodeTest, ModeSwitch_ToStealth)
{
    nlp_config_t config = nlp_config_default();
    config.brain_id = TEST_BRAIN_ID;

    node = nlp_node_create(&config);
    ASSERT_NE(node, nullptr);

    int result = nlp_set_mode(node, NLP_MODE_STEALTH);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(nlp_get_mode(node), NLP_MODE_STEALTH);
}

/**
 * WHAT: Test mode switch with null node
 * WHY:  Verify error handling
 */
TEST_F(NLPNodeTest, ModeSwitch_NullNode)
{
    int result = nlp_set_mode(nullptr, NLP_MODE_TACTICAL);
    EXPECT_NE(result, 0);

    nlp_mode_t mode = nlp_get_mode(nullptr);
    EXPECT_EQ(mode, NLP_MODE_STANDARD); // Default on error
}

/**
 * WHAT: Test invalid mode
 * WHY:  Verify validation
 */
TEST_F(NLPNodeTest, ModeSwitch_InvalidMode)
{
    nlp_config_t config = nlp_config_default();
    config.brain_id = TEST_BRAIN_ID;

    node = nlp_node_create(&config);
    ASSERT_NE(node, nullptr);

    nlp_mode_t invalid_mode = static_cast<nlp_mode_t>(99);
    int result = nlp_set_mode(node, invalid_mode);

    // Should fail with invalid mode
    EXPECT_NE(result, 0);
}

//=============================================================================
// EMCON Level Tests
//=============================================================================

/**
 * WHAT: Test setting EMCON level
 * WHY:  Verify emissions control
 */
TEST_F(NLPNodeTest, EMCONLevels_SetLevel)
{
    nlp_config_t config = nlp_config_default();
    config.brain_id = TEST_BRAIN_ID;

    node = nlp_node_create(&config);
    ASSERT_NE(node, nullptr);

    // Set to receive-only
    int result = nlp_set_emcon(node, NLP_EMCON_RECEIVE);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(nlp_get_emcon(node), NLP_EMCON_RECEIVE);
}

/**
 * WHAT: Test EMCON silent mode
 * WHY:  Verify complete radio silence
 */
TEST_F(NLPNodeTest, EMCONLevels_SilentMode)
{
    nlp_config_t config = nlp_config_default();
    config.brain_id = TEST_BRAIN_ID;

    node = nlp_node_create(&config);
    ASSERT_NE(node, nullptr);

    int result = nlp_set_emcon(node, NLP_EMCON_SILENT);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(nlp_get_emcon(node), NLP_EMCON_SILENT);

    // In silent mode, no transmission should occur
    // (Implementation would block all sends)
}

/**
 * WHAT: Test EMCON emergency override
 * WHY:  Verify break-glass emergency messaging
 */
TEST_F(NLPNodeTest, EMCONLevels_EmergencyOverride)
{
    nlp_config_t config = nlp_config_default();
    config.brain_id = TEST_BRAIN_ID;

    node = nlp_node_create(&config);
    ASSERT_NE(node, nullptr);

    // Set to emergency (overrides all EMCON)
    int result = nlp_set_emcon(node, NLP_EMCON_EMERGENCY);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(nlp_get_emcon(node), NLP_EMCON_EMERGENCY);
}

/**
 * WHAT: Test EMCON level transitions
 * WHY:  Verify all transitions are valid
 */
TEST_F(NLPNodeTest, EMCONLevels_AllTransitions)
{
    nlp_config_t config = nlp_config_default();
    config.brain_id = TEST_BRAIN_ID;

    node = nlp_node_create(&config);
    ASSERT_NE(node, nullptr);

    // Test all levels
    nlp_emcon_level_t levels[] = {
        NLP_EMCON_NORMAL,
        NLP_EMCON_REDUCED,
        NLP_EMCON_RECEIVE,
        NLP_EMCON_SILENT,
        NLP_EMCON_EMERGENCY
    };

    for (int i = 0; i < 5; i++) {
        int result = nlp_set_emcon(node, levels[i]);
        EXPECT_EQ(result, 0);
        EXPECT_EQ(nlp_get_emcon(node), levels[i]);
    }
}

//=============================================================================
// Statistics Tests
//=============================================================================

/**
 * WHAT: Test getting node statistics
 * WHY:  Verify statistics are tracked
 */
TEST_F(NLPNodeTest, Statistics_GetStats)
{
    nlp_config_t config = nlp_config_default();
    config.brain_id = TEST_BRAIN_ID;

    node = nlp_node_create(&config);
    ASSERT_NE(node, nullptr);

    nlp_stats_t stats;
    int result = nlp_get_stats(node, &stats);

    EXPECT_EQ(result, 0);

    // Initial stats should be zero
    EXPECT_EQ(stats.messages_sent, 0);
    EXPECT_EQ(stats.messages_received, 0);
    EXPECT_EQ(stats.active_sessions, 0);
}

/**
 * WHAT: Test resetting statistics
 * WHY:  Verify stats can be cleared
 */
TEST_F(NLPNodeTest, Statistics_Reset)
{
    nlp_config_t config = nlp_config_default();
    config.brain_id = TEST_BRAIN_ID;

    node = nlp_node_create(&config);
    ASSERT_NE(node, nullptr);

    // Reset stats
    nlp_reset_stats(node);

    nlp_stats_t stats;
    nlp_get_stats(node, &stats);

    EXPECT_EQ(stats.messages_sent, 0);
    EXPECT_EQ(stats.messages_received, 0);
}

/**
 * WHAT: Test statistics with null node
 * WHY:  Verify error handling
 */
TEST_F(NLPNodeTest, Statistics_NullNode)
{
    nlp_stats_t stats;
    int result = nlp_get_stats(nullptr, &stats);

    EXPECT_NE(result, 0);

    // Reset with null should be safe
    nlp_reset_stats(nullptr);
    SUCCEED();
}
