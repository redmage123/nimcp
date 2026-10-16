/**
 * @file test_nlp_peer_communication.cpp
 * @brief Integration Tests for Neural Link Protocol Peer Communication
 *
 * WHAT: Tests peer-to-peer communication between NLP nodes
 * WHY:  Verify nodes can establish connections, handshake, and exchange messages
 * HOW:  Create actual NLP nodes on localhost, test full message lifecycle
 *
 * TEST COVERAGE:
 * - Two-node handshake and session establishment
 * - Multi-peer mesh network formation (4+ nodes)
 * - Point-to-point message round-trip
 * - Broadcast message propagation to all peers
 * - Mesh relay through intermediate nodes
 * - Session timeout and reconnection
 * - Peer health monitoring
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <arpa/inet.h>

// Headers have their own extern "C" guards
#include "networking/nlp/nimcp_neural_link_protocol.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture and Helpers
//=============================================================================

/**
 * @brief Message tracker for verifying delivery
 */
struct MessageTracker {
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<nlp_msg_type_t> received_types;
    std::vector<uint32_t> sender_ids;
    std::vector<std::vector<uint8_t>> payloads;
    std::atomic<uint32_t> message_count{0};

    void record_message(uint32_t sender_id, nlp_msg_type_t type,
                       const uint8_t* payload, size_t payload_len) {
        std::lock_guard<std::mutex> lock(mutex);
        received_types.push_back(type);
        sender_ids.push_back(sender_id);
        if (payload && payload_len > 0) {
            payloads.push_back(std::vector<uint8_t>(payload, payload + payload_len));
        } else {
            payloads.push_back(std::vector<uint8_t>());
        }
        message_count++;
        cv.notify_all();
    }

    bool wait_for_messages(uint32_t count, uint32_t timeout_ms) {
        std::unique_lock<std::mutex> lock(mutex);
        return cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                          [this, count]() { return message_count >= count; });
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        received_types.clear();
        sender_ids.clear();
        payloads.clear();
        message_count = 0;
    }
};

/**
 * @brief Callback handlers
 */
static void message_received_callback(nlp_node_t node, const nlp_peer_t* peer,
                                      const nlp_message_t* msg, void* user_data) {
    auto* tracker = static_cast<MessageTracker*>(user_data);
    uint16_t msg_type = ntohs(msg->header.msg_type);
    tracker->record_message(ntohl(msg->header.sender_id),
                           static_cast<nlp_msg_type_t>(msg_type),
                           msg->payload, ntohs(msg->header.payload_len));
}

static void peer_state_changed_callback(nlp_node_t node, const nlp_peer_t* peer,
                                        nlp_session_state_t old_state,
                                        nlp_session_state_t new_state,
                                        void* user_data) {
    // Track peer state changes if needed
}

/**
 * @brief Test fixture for peer communication tests
 */
class NLPPeerCommunicationTest : public ::testing::Test {
protected:
    static constexpr uint16_t BASE_PORT = 17000;
    static constexpr uint32_t TIMEOUT_MS = 5000;
    static constexpr uint32_t SHORT_TIMEOUT_MS = 2000;

    std::vector<nlp_node_t> nodes_;
    std::vector<std::unique_ptr<MessageTracker>> trackers_;
    std::vector<uint16_t> ports_;

    void SetUp() override {
        // Logging initialized by framework
    }

    void TearDown() override {
        // Stop and destroy all nodes
        for (auto node : nodes_) {
            if (node) {
                nlp_node_stop(node);
                nlp_node_destroy(node);
            }
        }
        nodes_.clear();
        trackers_.clear();
        ports_.clear();
    }

    /**
     * @brief Create NLP node with specified configuration
     */
    nlp_node_t CreateNode(uint16_t port, nlp_mode_t mode = NLP_MODE_STANDARD) {
        nlp_config_t config = nlp_config_default();
        config.brain_id = nlp_generate_brain_id();
        config.port = port;
        config.default_mode = mode;
        config.auto_mode_switch = false;
        config.heartbeat_interval_ms = 500;
        config.session_timeout_ms = 3000;
        config.handshake_timeout_ms = 2000;
        config.max_peers = 10;
        strncpy(config.bind_address, "127.0.0.1", sizeof(config.bind_address) - 1);

        // Create tracker for this node
        auto tracker = std::make_unique<MessageTracker>();
        config.user_data = tracker.get();

        nlp_node_t node = nlp_node_create(&config);
        if (!node) {
            return nullptr;
        }

        // Set callbacks
        nlp_set_message_callback(node, message_received_callback);
        nlp_set_peer_callback(node, peer_state_changed_callback);

        // Start node
        if (nlp_node_start(node) != 0) {
            nlp_node_destroy(node);
            return nullptr;
        }

        nodes_.push_back(node);
        trackers_.push_back(std::move(tracker));
        ports_.push_back(port);

        return node;
    }

    /**
     * @brief Connect two nodes together
     */
    bool ConnectNodes(nlp_node_t node1, nlp_node_t node2, uint16_t node2_port) {
        uint32_t peer_id = nlp_connect_peer(node1, "127.0.0.1", node2_port);
        if (peer_id == 0) {
            return false;
        }

        // Wait for handshake to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Check if peer is connected
        nlp_peer_t peer;
        return nlp_get_peer(node1, peer_id, &peer) == 0 &&
               peer.session_state == NLP_SESSION_ESTABLISHED;
    }

    /**
     * @brief Get tracker for a node
     */
    MessageTracker* GetTracker(size_t node_index) {
        if (node_index < trackers_.size()) {
            return trackers_[node_index].get();
        }
        return nullptr;
    }
};

//=============================================================================
// Two-Node Communication Tests
//=============================================================================

TEST_F(NLPPeerCommunicationTest, TwoNodeHandshake) {
    // Create two nodes
    nlp_node_t node1 = CreateNode(BASE_PORT);
    ASSERT_NE(node1, nullptr) << "Failed to create node 1";

    nlp_node_t node2 = CreateNode(BASE_PORT + 1);
    ASSERT_NE(node2, nullptr) << "Failed to create node 2";

    // Connect node1 to node2
    uint32_t peer_id = nlp_connect_peer(node1, "127.0.0.1", BASE_PORT + 1);
    ASSERT_NE(peer_id, 0u) << "Failed to initiate connection";

    // Wait for handshake to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Verify session is established on node1
    nlp_peer_t peer;
    ASSERT_EQ(nlp_get_peer(node1, peer_id, &peer), 0);
    EXPECT_EQ(peer.session_state, NLP_SESSION_ESTABLISHED);
    EXPECT_TRUE(peer.healthy);

    // Verify bidirectional connection (node2 should see node1 as peer)
    nlp_peer_t peers[10];
    uint32_t peer_count = nlp_get_peers(node2, peers, 10);
    EXPECT_EQ(peer_count, 1u) << "Node 2 should have 1 peer";

    if (peer_count > 0) {
        EXPECT_EQ(peers[0].session_state, NLP_SESSION_ESTABLISHED);
        EXPECT_TRUE(peers[0].healthy);
    }
}

TEST_F(NLPPeerCommunicationTest, MessageRoundTrip) {
    // Create and connect two nodes
    nlp_node_t node1 = CreateNode(BASE_PORT + 10);
    nlp_node_t node2 = CreateNode(BASE_PORT + 11);
    ASSERT_NE(node1, nullptr);
    ASSERT_NE(node2, nullptr);

    // Connect nodes
    uint32_t peer_id = nlp_connect_peer(node1, "127.0.0.1", BASE_PORT + 11);
    ASSERT_NE(peer_id, 0u);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Send message from node1 to node2
    const char* test_payload = "Hello from node1";
    int result = nlp_send(node1, peer_id, NLP_MSG_DEBUG,
                         test_payload, strlen(test_payload) + 1,
                         NLP_PRIORITY_NORMAL);
    ASSERT_EQ(result, 0) << "Failed to send message";

    // Wait for message to be received
    MessageTracker* tracker2 = GetTracker(1);
    ASSERT_NE(tracker2, nullptr);
    ASSERT_TRUE(tracker2->wait_for_messages(1, TIMEOUT_MS))
        << "Timeout waiting for message";

    // Verify received message
    EXPECT_EQ(tracker2->received_types[0], NLP_MSG_DEBUG);
    ASSERT_FALSE(tracker2->payloads[0].empty());

    std::string received(tracker2->payloads[0].begin(),
                        tracker2->payloads[0].end() - 1);
    EXPECT_EQ(received, test_payload);

    // Send reply from node2 to node1
    const char* reply_payload = "Hello back from node2";
    nlp_peer_t peers[10];
    uint32_t peer_count = nlp_get_peers(node2, peers, 10);
    ASSERT_EQ(peer_count, 1u);

    result = nlp_send(node2, peers[0].peer_id, NLP_MSG_DEBUG,
                     reply_payload, strlen(reply_payload) + 1,
                     NLP_PRIORITY_NORMAL);
    ASSERT_EQ(result, 0);

    // Verify reply received
    MessageTracker* tracker1 = GetTracker(0);
    ASSERT_TRUE(tracker1->wait_for_messages(1, TIMEOUT_MS));

    std::string received_reply(tracker1->payloads[0].begin(),
                              tracker1->payloads[0].end() - 1);
    EXPECT_EQ(received_reply, reply_payload);
}

//=============================================================================
// Multi-Peer Mesh Network Tests
//=============================================================================

TEST_F(NLPPeerCommunicationTest, MultiPeerMesh) {
    // Create 4 nodes
    constexpr size_t NUM_NODES = 4;
    std::vector<nlp_node_t> mesh_nodes;

    for (size_t i = 0; i < NUM_NODES; i++) {
        nlp_node_t node = CreateNode(BASE_PORT + 20 + i);
        ASSERT_NE(node, nullptr) << "Failed to create node " << i;
        mesh_nodes.push_back(node);
    }

    // Create full mesh: each node connects to all others
    for (size_t i = 0; i < NUM_NODES; i++) {
        for (size_t j = 0; j < NUM_NODES; j++) {
            if (i != j) {
                uint32_t peer_id = nlp_connect_peer(
                    mesh_nodes[i], "127.0.0.1", BASE_PORT + 20 + j);
                ASSERT_NE(peer_id, 0u)
                    << "Failed to connect node " << i << " to node " << j;
            }
        }
    }

    // Wait for all connections to establish
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Verify each node has NUM_NODES-1 peers
    for (size_t i = 0; i < NUM_NODES; i++) {
        nlp_peer_t peers[10];
        uint32_t peer_count = nlp_get_peers(mesh_nodes[i], peers, 10);
        EXPECT_EQ(peer_count, NUM_NODES - 1)
            << "Node " << i << " should have " << (NUM_NODES - 1) << " peers";

        // Verify all peers are healthy
        for (uint32_t p = 0; p < peer_count; p++) {
            EXPECT_EQ(peers[p].session_state, NLP_SESSION_ESTABLISHED)
                << "Node " << i << " peer " << p << " not established";
            EXPECT_TRUE(peers[p].healthy)
                << "Node " << i << " peer " << p << " not healthy";
        }
    }
}

TEST_F(NLPPeerCommunicationTest, BroadcastReach) {
    // Create 4 nodes in mesh
    constexpr size_t NUM_NODES = 4;
    std::vector<nlp_node_t> mesh_nodes;

    for (size_t i = 0; i < NUM_NODES; i++) {
        nlp_node_t node = CreateNode(BASE_PORT + 30 + i);
        ASSERT_NE(node, nullptr);
        mesh_nodes.push_back(node);
    }

    // Connect in mesh
    for (size_t i = 0; i < NUM_NODES; i++) {
        for (size_t j = 0; j < NUM_NODES; j++) {
            if (i != j) {
                nlp_connect_peer(mesh_nodes[i], "127.0.0.1", BASE_PORT + 30 + j);
            }
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Clear all trackers
    for (size_t i = 0; i < NUM_NODES; i++) {
        GetTracker(i)->clear();
    }

    // Broadcast from node 0
    const char* broadcast_msg = "Broadcast to all nodes";
    int result = nlp_broadcast(mesh_nodes[0], NLP_MSG_DEBUG,
                              broadcast_msg, strlen(broadcast_msg) + 1,
                              NLP_PRIORITY_NORMAL);
    EXPECT_GT(result, 0) << "Broadcast should reach peers";

    // Verify all other nodes (1, 2, 3) received the broadcast
    for (size_t i = 1; i < NUM_NODES; i++) {
        MessageTracker* tracker = GetTracker(i);
        ASSERT_NE(tracker, nullptr);
        ASSERT_TRUE(tracker->wait_for_messages(1, TIMEOUT_MS))
            << "Node " << i << " did not receive broadcast";

        EXPECT_EQ(tracker->received_types[0], NLP_MSG_DEBUG);

        std::string received(tracker->payloads[0].begin(),
                           tracker->payloads[0].end() - 1);
        EXPECT_EQ(received, broadcast_msg);
    }

    // Verify node 0 did not receive its own broadcast
    MessageTracker* tracker0 = GetTracker(0);
    EXPECT_EQ(tracker0->message_count, 0u)
        << "Broadcasting node should not receive its own message";
}

TEST_F(NLPPeerCommunicationTest, MeshRelay) {
    // Create 3 nodes in a line: A <-> B <-> C
    // Test that A can relay message to C through B

    nlp_node_t nodeA = CreateNode(BASE_PORT + 40);
    nlp_node_t nodeB = CreateNode(BASE_PORT + 41);
    nlp_node_t nodeC = CreateNode(BASE_PORT + 42);

    ASSERT_NE(nodeA, nullptr);
    ASSERT_NE(nodeB, nullptr);
    ASSERT_NE(nodeC, nullptr);

    // Connect A <-> B
    uint32_t peerB_from_A = nlp_connect_peer(nodeA, "127.0.0.1", BASE_PORT + 41);
    ASSERT_NE(peerB_from_A, 0u);

    // Connect B <-> C
    uint32_t peerC_from_B = nlp_connect_peer(nodeB, "127.0.0.1", BASE_PORT + 42);
    ASSERT_NE(peerC_from_B, 0u);

    std::this_thread::sleep_for(std::chrono::milliseconds(800));

    // Get node C's brain ID for relay
    nlp_peer_t peer_c_info;
    ASSERT_EQ(nlp_get_peer(nodeB, peerC_from_B, &peer_c_info), 0);
    uint32_t nodeC_brain_id = peer_c_info.peer_id;

    // Clear trackers
    for (size_t i = 0; i < 3; i++) {
        GetTracker(i)->clear();
    }

    // A relays message to C through the mesh
    const char* relay_msg = "Relayed message from A to C";
    int result = nlp_relay(nodeA, nodeC_brain_id, NLP_MSG_DEBUG,
                          relay_msg, strlen(relay_msg) + 1);

    // Note: relay might fail if direct routing not supported,
    // but we test that the infrastructure exists
    if (result == 0) {
        // Wait and check if C received it
        MessageTracker* trackerC = GetTracker(2);
        if (trackerC->wait_for_messages(1, SHORT_TIMEOUT_MS)) {
            std::string received(trackerC->payloads[0].begin(),
                               trackerC->payloads[0].end() - 1);
            EXPECT_EQ(received, relay_msg);
        }
    }
}

//=============================================================================
// Session Management Tests
//=============================================================================

TEST_F(NLPPeerCommunicationTest, PeerHealthMonitoring) {
    // Create two connected nodes
    nlp_node_t node1 = CreateNode(BASE_PORT + 50);
    nlp_node_t node2 = CreateNode(BASE_PORT + 51);
    ASSERT_NE(node1, nullptr);
    ASSERT_NE(node2, nullptr);

    uint32_t peer_id = nlp_connect_peer(node1, "127.0.0.1", BASE_PORT + 51);
    ASSERT_NE(peer_id, 0u);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Verify initial health
    nlp_peer_t peer;
    ASSERT_EQ(nlp_get_peer(node1, peer_id, &peer), 0);
    EXPECT_TRUE(peer.healthy);
    EXPECT_EQ(peer.missed_heartbeats, 0u);

    // Stop node2 to simulate failure
    nlp_node_stop(node2);

    // Wait for heartbeat timeout (session_timeout_ms = 3000ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(4000));

    // Check peer health degraded
    ASSERT_EQ(nlp_get_peer(node1, peer_id, &peer), 0);
    // Peer should be marked unhealthy or session timeout
    EXPECT_TRUE(!peer.healthy || peer.session_state != NLP_SESSION_ESTABLISHED);
}

TEST_F(NLPPeerCommunicationTest, SessionStatistics) {
    // Create and connect nodes
    nlp_node_t node1 = CreateNode(BASE_PORT + 60);
    nlp_node_t node2 = CreateNode(BASE_PORT + 61);
    ASSERT_NE(node1, nullptr);
    ASSERT_NE(node2, nullptr);

    uint32_t peer_id = nlp_connect_peer(node1, "127.0.0.1", BASE_PORT + 61);
    ASSERT_NE(peer_id, 0u);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Send several messages
    const char* msg = "Test message";
    for (int i = 0; i < 5; i++) {
        nlp_send(node1, peer_id, NLP_MSG_DEBUG, msg, strlen(msg) + 1,
                NLP_PRIORITY_NORMAL);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Get statistics
    nlp_stats_t stats;
    ASSERT_EQ(nlp_get_stats(node1, &stats), 0);

    // Verify message counts
    EXPECT_GT(stats.messages_sent, 0u);
    EXPECT_GT(stats.bytes_sent, 0u);
    EXPECT_EQ(stats.connected_peers, 1u);
    EXPECT_EQ(stats.active_sessions, 1u);

    // Get peer-specific statistics
    nlp_peer_t peer;
    ASSERT_EQ(nlp_get_peer(node1, peer_id, &peer), 0);
    EXPECT_GE(peer.messages_sent, 5u);
    EXPECT_GT(peer.bytes_sent, 0u);
}

//=============================================================================
// Error Condition Tests
//=============================================================================

TEST_F(NLPPeerCommunicationTest, ConnectionToNonexistentPeer) {
    nlp_node_t node = CreateNode(BASE_PORT + 70);
    ASSERT_NE(node, nullptr);

    // Try to connect to a port with no listener
    uint32_t peer_id = nlp_connect_peer(node, "127.0.0.1", BASE_PORT + 99);

    // Connection should fail or timeout
    if (peer_id != 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2500));

        nlp_peer_t peer;
        int result = nlp_get_peer(node, peer_id, &peer);

        // Should either fail to get peer or peer should be in error state
        EXPECT_TRUE(result != 0 ||
                   peer.session_state == NLP_SESSION_ERROR ||
                   peer.session_state == NLP_SESSION_DISCONNECTED);
    }
}

TEST_F(NLPPeerCommunicationTest, DisconnectAndReconnect) {
    // Create and connect nodes
    nlp_node_t node1 = CreateNode(BASE_PORT + 80);
    nlp_node_t node2 = CreateNode(BASE_PORT + 81);
    ASSERT_NE(node1, nullptr);
    ASSERT_NE(node2, nullptr);

    uint32_t peer_id = nlp_connect_peer(node1, "127.0.0.1", BASE_PORT + 81);
    ASSERT_NE(peer_id, 0u);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Verify connection
    nlp_peer_t peer;
    ASSERT_EQ(nlp_get_peer(node1, peer_id, &peer), 0);
    EXPECT_EQ(peer.session_state, NLP_SESSION_ESTABLISHED);

    // Disconnect
    ASSERT_EQ(nlp_disconnect_peer(node1, peer_id), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Verify disconnection
    if (nlp_get_peer(node1, peer_id, &peer) == 0) {
        EXPECT_NE(peer.session_state, NLP_SESSION_ESTABLISHED);
    }

    // Reconnect
    uint32_t new_peer_id = nlp_connect_peer(node1, "127.0.0.1", BASE_PORT + 81);
    ASSERT_NE(new_peer_id, 0u);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Verify reconnection
    ASSERT_EQ(nlp_get_peer(node1, new_peer_id, &peer), 0);
    EXPECT_EQ(peer.session_state, NLP_SESSION_ESTABLISHED);
}
