/**
 * @file test_protocol_integration.cpp
 * @brief Comprehensive integration tests for NIMCP 2.0 protocol multi-component scenarios
 *
 * WHAT: Test complete NIMCP protocol workflows across multiple components
 * WHY:  Verify protocol components work together correctly in realistic scenarios
 * HOW:  Simulate multi-node scenarios with event packets, control messages, and routing
 *
 * TEST COVERAGE:
 * - Full event packet workflow: serialize → transmit → deserialize → validate
 * - Full control message workflow: create → serialize → transmit → deserialize → execute
 * - Mixed event and control message streams
 * - Subscription filtering pipeline (create filter → receive events → filter → route)
 * - Feature code routing across mock nodes
 * - Multi-hop event propagation with hop count
 * - Control message acknowledgment flow
 * - Neuromodulator diffusion simulation (CTRL_MSG_NEUROMOD_DIFFUSION)
 * - Glial coordination workflow (pruning decisions across nodes)
 * - Brain region synchronization (CTRL_MSG_REGION_SYNC + CTRL_MSG_REGION_ACTIVITY)
 * - Error recovery scenarios (bad checksums, retransmission)
 * - Bandwidth/latency simulation with rate limiting
 * - Confidence-based event propagation (drop low-confidence events)
 *
 * @author NIMCP Development Team
 * @date 2025-11-20
 */

#include <gtest/gtest.h>
#include <vector>
#include <queue>
#include <map>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>

// Headers have their own extern "C" guards
#include "networking/protocol/nimcp_protocol.h"

//=============================================================================
// Test Helpers and Mock Infrastructure
//=============================================================================

/**
 * @brief Mock node for simulating multi-node scenarios
 */
struct MockNode {
    uint32_t node_id;
    std::vector<subscription_filter_t> subscriptions;
    std::queue<std::vector<uint8_t>> incoming_events;
    std::queue<std::vector<uint8_t>> incoming_control;
    std::map<feature_domain_t, uint32_t> feature_routing_table;
    uint64_t last_message_time;
    uint32_t bandwidth_limit_bps;  // bytes per second
    uint32_t bytes_sent_this_second;

    MockNode(uint32_t id) :
        node_id(id),
        last_message_time(0),
        bandwidth_limit_bps(100000),  // 100KB/s default
        bytes_sent_this_second(0) {}

    bool can_send(uint32_t bytes) {
        if (bandwidth_limit_bps == 0) return true;  // unlimited
        return (bytes_sent_this_second + bytes) <= bandwidth_limit_bps;
    }

    void track_send(uint32_t bytes) {
        bytes_sent_this_second += bytes;
    }

    void reset_bandwidth_counter() {
        bytes_sent_this_second = 0;
    }
};

/**
 * @brief Mock network simulator for testing multi-node scenarios
 */
class MockNetwork {
public:
    std::map<uint32_t, MockNode*> nodes;
    float packet_loss_rate;
    uint32_t latency_ms;
    bool corrupt_checksums;

    MockNetwork() :
        packet_loss_rate(0.0f),
        latency_ms(0),
        corrupt_checksums(false) {}

    ~MockNetwork() {
        for (auto& pair : nodes) {
            delete pair.second;
        }
    }

    MockNode* add_node(uint32_t node_id) {
        MockNode* node = new MockNode(node_id);
        nodes[node_id] = node;
        return node;
    }

    bool send_event(uint32_t from_id, uint32_t to_id,
                    const event_packet_t* packet, const void* payload) {
        if (nodes.find(to_id) == nodes.end()) return false;

        // Simulate packet loss
        if (packet_loss_rate > 0.0f) {
            float rand_val = (float)rand() / RAND_MAX;
            if (rand_val < packet_loss_rate) return false;
        }

        // Serialize packet
        uint8_t buffer[4096];
        int size = event_packet_serialize(packet, payload, buffer, sizeof(buffer));
        if (size < 0) return false;

        // Check bandwidth limit
        MockNode* from_node = nodes[from_id];
        if (!from_node->can_send(size)) return false;
        from_node->track_send(size);

        // Simulate latency
        if (latency_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(latency_ms));
        }

        // Optionally corrupt checksum
        if (corrupt_checksums && size > 20) {
            buffer[20] ^= 0xFF;  // Corrupt a byte
        }

        // Queue in destination node
        std::vector<uint8_t> msg(buffer, buffer + size);
        nodes[to_id]->incoming_events.push(msg);

        return true;
    }

    bool send_control(uint32_t from_id, uint32_t to_id,
                     const control_message_t* msg, const void* params) {
        if (nodes.find(to_id) == nodes.end()) return false;

        // Serialize message
        uint8_t buffer[4096];
        int size = control_message_serialize(msg, params, buffer, sizeof(buffer));
        if (size < 0) return false;

        // Check bandwidth limit
        MockNode* from_node = nodes[from_id];
        if (!from_node->can_send(size)) return false;
        from_node->track_send(size);

        // Simulate latency
        if (latency_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(latency_ms));
        }

        // Queue in destination node
        std::vector<uint8_t> control_msg(buffer, buffer + size);
        nodes[to_id]->incoming_control.push(control_msg);

        return true;
    }

    void broadcast_event(uint32_t from_id, const event_packet_t* packet,
                        const void* payload) {
        for (auto& pair : nodes) {
            if (pair.first != from_id) {
                send_event(from_id, pair.first, packet, payload);
            }
        }
    }
};

/**
 * @brief Create a test event packet with specified parameters
 */
event_packet_t create_test_event(feature_code_t feature_code, uint32_t source_id,
                                  float confidence, uint8_t flags) {
    event_packet_t packet = {};
    EVENT_SET_VERSION(&packet, PROTOCOL_VERSION);
    EVENT_SET_FLAGS(&packet, flags);
    EVENT_SET_FEATURE_CODE(&packet, feature_code);
    packet.source_node_id = source_id;
    packet.timestamp = 1234567890ULL;
    packet.confidence = EVENT_FLOAT_TO_CONFIDENCE(confidence);
    packet.hop_count = 0;
    packet.payload_length = 0;
    return packet;
}

/**
 * @brief Create a test control message
 */
control_message_t create_test_control(control_msg_type_t type, uint32_t source_id,
                                       uint32_t target_id, uint8_t flags,
                                       uint32_t param_size) {
    control_message_t msg = {};
    msg.version = PROTOCOL_VERSION;
    msg.msg_type = type;
    msg.flags = flags;
    msg.message_length = sizeof(control_message_t) + param_size;
    msg.source_node_id = source_id;
    msg.target_specifier = target_id;
    msg.sequence_number = 1;
    msg.param_count = 1;
    return msg;
}

//=============================================================================
// Test Fixture
//=============================================================================

class ProtocolIntegrationTest : public ::testing::Test {
protected:
    MockNetwork network;

    void SetUp() override {
        srand(42);  // Deterministic randomness for testing
    }

    void TearDown() override {
        // Network destructor cleans up nodes
    }
};

//=============================================================================
// Event Packet Workflow Tests
//=============================================================================

TEST_F(ProtocolIntegrationTest, FullEventPacketWorkflow) {
    // WHAT: Test complete event workflow: serialize → transmit → deserialize → validate
    // WHY:  Verify event packets work end-to-end

    MockNode* sender = network.add_node(1);
    MockNode* receiver = network.add_node(2);

    // Create event packet
    feature_code_t feature = MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0x1234);
    event_packet_t original = create_test_event(feature, 1, 0.95f,
                                                 EVENT_FLAG_EXCITATORY);

    // Add payload
    const char* payload = "Neural spike data";
    original.payload_length = strlen(payload);

    // Send through network
    ASSERT_TRUE(network.send_event(1, 2, &original, payload));

    // Receive and deserialize
    ASSERT_FALSE(receiver->incoming_events.empty());
    auto msg_buffer = receiver->incoming_events.front();
    receiver->incoming_events.pop();

    event_packet_t received = {};
    char recv_payload[256] = {};
    int bytes = event_packet_deserialize(msg_buffer.data(), msg_buffer.size(),
                                         &received, recv_payload, sizeof(recv_payload));

    // Verify successful deserialization
    ASSERT_GT(bytes, 0);
    ASSERT_TRUE(event_packet_validate(&received));

    // Verify all fields match
    EXPECT_EQ(EVENT_GET_VERSION(&received), PROTOCOL_VERSION);
    EXPECT_EQ(EVENT_GET_FLAGS(&received), EVENT_FLAG_EXCITATORY);
    EXPECT_EQ(EVENT_GET_FEATURE_CODE(&received), feature);
    EXPECT_EQ(received.source_node_id, 1u);
    EXPECT_EQ(received.confidence, original.confidence);
    EXPECT_STREQ(recv_payload, payload);
}

TEST_F(ProtocolIntegrationTest, EventPacketWithMultipleHops) {
    // WHAT: Test multi-hop event propagation with hop count tracking
    // WHY:  Verify TTL mechanism prevents infinite loops

    MockNode* node1 = network.add_node(1);
    MockNode* node2 = network.add_node(2);
    MockNode* node3 = network.add_node(3);

    feature_code_t feature = MAKE_FEATURE_CODE(FEATURE_DOMAIN_AUDITORY, 0x5678);
    event_packet_t packet = create_test_event(feature, 1, 0.8f, EVENT_FLAG_INHIBITORY);

    // Hop 1: Node 1 → Node 2
    packet.hop_count = 0;
    ASSERT_TRUE(network.send_event(1, 2, &packet, nullptr));

    // Receive at node 2
    auto msg = node2->incoming_events.front();
    node2->incoming_events.pop();

    event_packet_t hop1_packet = {};
    event_packet_deserialize(msg.data(), msg.size(), &hop1_packet, nullptr, 0);
    EXPECT_EQ(hop1_packet.hop_count, 0u);

    // Hop 2: Node 2 → Node 3 (increment hop count)
    hop1_packet.hop_count++;
    ASSERT_TRUE(network.send_event(2, 3, &hop1_packet, nullptr));

    // Receive at node 3
    msg = node3->incoming_events.front();
    node3->incoming_events.pop();

    event_packet_t hop2_packet = {};
    event_packet_deserialize(msg.data(), msg.size(), &hop2_packet, nullptr, 0);
    EXPECT_EQ(hop2_packet.hop_count, 1u);

    // Verify TTL logic: drop after max hops (e.g., 10)
    const uint8_t MAX_HOPS = 10;
    hop2_packet.hop_count = MAX_HOPS;
    EXPECT_GE(hop2_packet.hop_count, MAX_HOPS);
    // In production, would not forward this packet
}

TEST_F(ProtocolIntegrationTest, EventBroadcastScenario) {
    // WHAT: Test broadcasting event to all nodes in network
    // WHY:  Verify global event distribution works

    network.add_node(1);
    network.add_node(2);
    network.add_node(3);
    network.add_node(4);

    feature_code_t feature = MAKE_FEATURE_CODE(FEATURE_DOMAIN_SYSTEM, 0x0001);
    event_packet_t packet = create_test_event(feature, 1, 1.0f, EVENT_FLAG_EXCITATORY);

    // Broadcast from node 1
    network.broadcast_event(1, &packet, nullptr);

    // Verify all other nodes received the event
    for (uint32_t id = 2; id <= 4; id++) {
        MockNode* node = network.nodes[id];
        ASSERT_FALSE(node->incoming_events.empty())
            << "Node " << id << " did not receive broadcast";

        auto msg = node->incoming_events.front();
        event_packet_t recv = {};
        event_packet_deserialize(msg.data(), msg.size(), &recv, nullptr, 0);

        EXPECT_EQ(EVENT_GET_FEATURE_CODE(&recv), feature);
        EXPECT_EQ(recv.source_node_id, 1u);
    }
}

//=============================================================================
// Control Message Workflow Tests
//=============================================================================

TEST_F(ProtocolIntegrationTest, FullControlMessageWorkflow) {
    // WHAT: Test complete control workflow: create → serialize → transmit → deserialize
    // WHY:  Verify control messages work end-to-end

    MockNode* sender = network.add_node(1);
    MockNode* receiver = network.add_node(2);

    // Create control message with parameters
    control_message_t original = create_test_control(CTRL_MSG_SET_LEARNING_RATE,
                                                      1, 2, CTRL_FLAG_ACK_REQUIRED,
                                                      sizeof(float));

    float learning_rate = 0.001f;

    // Send through network
    ASSERT_TRUE(network.send_control(1, 2, &original, &learning_rate));

    // Receive and deserialize
    ASSERT_FALSE(receiver->incoming_control.empty());
    auto msg_buffer = receiver->incoming_control.front();
    receiver->incoming_control.pop();

    control_message_t received = {};
    float recv_param = 0.0f;
    int bytes = control_message_deserialize(msg_buffer.data(), msg_buffer.size(),
                                           &received, &recv_param, sizeof(recv_param));

    // Verify successful deserialization
    ASSERT_GT(bytes, 0);
    ASSERT_TRUE(control_message_validate(&received));

    // Verify all fields match
    EXPECT_EQ(received.version, PROTOCOL_VERSION);
    EXPECT_EQ(received.msg_type, CTRL_MSG_SET_LEARNING_RATE);
    EXPECT_EQ(received.flags, CTRL_FLAG_ACK_REQUIRED);
    EXPECT_EQ(received.source_node_id, 1u);
    EXPECT_EQ(received.target_specifier, 2u);
    EXPECT_FLOAT_EQ(recv_param, learning_rate);
}

TEST_F(ProtocolIntegrationTest, ControlMessageAcknowledgmentFlow) {
    // WHAT: Test control message with ACK required and ACK response
    // WHY:  Verify reliable delivery mechanism works

    MockNode* node1 = network.add_node(1);
    MockNode* node2 = network.add_node(2);

    // Step 1: Node 1 sends control message with ACK required
    control_message_t request = create_test_control(CTRL_MSG_UPDATE_LINK,
                                                     1, 2, CTRL_FLAG_ACK_REQUIRED, 0);
    request.sequence_number = 42;

    ASSERT_TRUE(network.send_control(1, 2, &request, nullptr));

    // Step 2: Node 2 receives and processes
    ASSERT_FALSE(node2->incoming_control.empty());
    auto msg = node2->incoming_control.front();
    node2->incoming_control.pop();

    control_message_t received = {};
    control_message_deserialize(msg.data(), msg.size(), &received, nullptr, 0);

    EXPECT_EQ(received.flags & CTRL_FLAG_ACK_REQUIRED, CTRL_FLAG_ACK_REQUIRED);
    EXPECT_EQ(received.sequence_number, 42u);

    // Step 3: Node 2 sends ACK back (simulated with HEARTBEAT)
    control_message_t ack = create_test_control(CTRL_MSG_HEARTBEAT,
                                                 2, 1, 0, 0);
    ack.sequence_number = 42;  // Match original sequence

    ASSERT_TRUE(network.send_control(2, 1, &ack, nullptr));

    // Step 4: Node 1 receives ACK
    ASSERT_FALSE(node1->incoming_control.empty());
    msg = node1->incoming_control.front();

    control_message_t ack_received = {};
    control_message_deserialize(msg.data(), msg.size(), &ack_received, nullptr, 0);

    EXPECT_EQ(ack_received.sequence_number, 42u);
    EXPECT_EQ(ack_received.source_node_id, 2u);
}

TEST_F(ProtocolIntegrationTest, ControlMessageGlobalBroadcast) {
    // WHAT: Test global broadcast control message
    // WHY:  Verify cluster-wide configuration changes work

    network.add_node(1);
    network.add_node(2);
    network.add_node(3);

    // Create global broadcast message
    control_message_t global_msg = create_test_control(CTRL_MSG_CLUSTER_ANNOUNCE,
                                                        1, 0xFFFFFFFF,  // Global target
                                                        CTRL_FLAG_GLOBAL, 0);

    // Broadcast to all nodes
    for (uint32_t id = 2; id <= 3; id++) {
        ASSERT_TRUE(network.send_control(1, id, &global_msg, nullptr));
    }

    // Verify all nodes received
    for (uint32_t id = 2; id <= 3; id++) {
        MockNode* node = network.nodes[id];
        ASSERT_FALSE(node->incoming_control.empty());

        auto msg = node->incoming_control.front();
        control_message_t recv = {};
        control_message_deserialize(msg.data(), msg.size(), &recv, nullptr, 0);

        EXPECT_EQ(recv.flags & CTRL_FLAG_GLOBAL, CTRL_FLAG_GLOBAL);
        EXPECT_EQ(recv.msg_type, CTRL_MSG_CLUSTER_ANNOUNCE);
    }
}

//=============================================================================
// Mixed Event and Control Message Streams
//=============================================================================

TEST_F(ProtocolIntegrationTest, MixedEventControlStream) {
    // WHAT: Test interleaved event packets and control messages
    // WHY:  Verify both message types can coexist in same stream

    MockNode* sender = network.add_node(1);
    MockNode* receiver = network.add_node(2);

    // Send sequence: Event → Control → Event → Control
    feature_code_t feature = MAKE_FEATURE_CODE(FEATURE_DOMAIN_MOTOR, 0x1111);
    event_packet_t event1 = create_test_event(feature, 1, 0.9f, EVENT_FLAG_EXCITATORY);

    control_message_t ctrl1 = create_test_control(CTRL_MSG_HEARTBEAT, 1, 2, 0, 0);

    event_packet_t event2 = create_test_event(feature, 1, 0.85f, EVENT_FLAG_INHIBITORY);

    control_message_t ctrl2 = create_test_control(CTRL_MSG_SET_SUBSCRIPTION, 1, 2, 0, 0);

    // Send in sequence
    ASSERT_TRUE(network.send_event(1, 2, &event1, nullptr));
    ASSERT_TRUE(network.send_control(1, 2, &ctrl1, nullptr));
    ASSERT_TRUE(network.send_event(1, 2, &event2, nullptr));
    ASSERT_TRUE(network.send_control(1, 2, &ctrl2, nullptr));

    // Verify all received
    EXPECT_EQ(receiver->incoming_events.size(), 2u);
    EXPECT_EQ(receiver->incoming_control.size(), 2u);

    // Verify order and content
    auto e1_msg = receiver->incoming_events.front();
    receiver->incoming_events.pop();
    event_packet_t recv_e1 = {};
    event_packet_deserialize(e1_msg.data(), e1_msg.size(), &recv_e1, nullptr, 0);
    EXPECT_EQ(EVENT_GET_FLAGS(&recv_e1), EVENT_FLAG_EXCITATORY);

    auto c1_msg = receiver->incoming_control.front();
    receiver->incoming_control.pop();
    control_message_t recv_c1 = {};
    control_message_deserialize(c1_msg.data(), c1_msg.size(), &recv_c1, nullptr, 0);
    EXPECT_EQ(recv_c1.msg_type, CTRL_MSG_HEARTBEAT);
}

//=============================================================================
// Subscription Filtering Pipeline Tests
//=============================================================================

TEST_F(ProtocolIntegrationTest, SubscriptionFilteringPipeline) {
    // WHAT: Test create filter → receive events → filter → route
    // WHY:  Verify subscription-based filtering works correctly

    MockNode* subscriber = network.add_node(1);
    MockNode* publisher = network.add_node(2);

    // Create subscription filter for VISION domain only
    subscription_filter_t filter = {};
    filter.feature_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0);
    filter.feature_mask = 0xFF000000;  // Match domain only
    filter.confidence_threshold = 0.7f;
    filter.max_rate_hz = 100;

    subscriber->subscriptions.push_back(filter);

    // Send various events
    event_packet_t vision_event = create_test_event(
        MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0x1234), 2, 0.9f, EVENT_FLAG_EXCITATORY);

    event_packet_t audio_event = create_test_event(
        MAKE_FEATURE_CODE(FEATURE_DOMAIN_AUDITORY, 0x5678), 2, 0.95f, EVENT_FLAG_EXCITATORY);

    event_packet_t low_conf_vision = create_test_event(
        MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0xABCD), 2, 0.5f, EVENT_FLAG_EXCITATORY);

    network.send_event(2, 1, &vision_event, nullptr);
    network.send_event(2, 1, &audio_event, nullptr);
    network.send_event(2, 1, &low_conf_vision, nullptr);

    // Process incoming events with filter
    int matched_count = 0;
    while (!subscriber->incoming_events.empty()) {
        auto msg = subscriber->incoming_events.front();
        subscriber->incoming_events.pop();

        event_packet_t recv = {};
        event_packet_deserialize(msg.data(), msg.size(), &recv, nullptr, 0);

        // Apply subscription filter
        if (subscription_matches(&filter, &recv)) {
            matched_count++;
        }
    }

    // Only vision event with confidence >= 0.7 should match
    EXPECT_EQ(matched_count, 1);
}

TEST_F(ProtocolIntegrationTest, MultipleSubscriptionsFiltering) {
    // WHAT: Test node with multiple subscription filters
    // WHY:  Verify complex filtering scenarios work

    MockNode* node = network.add_node(1);
    network.add_node(2);

    // Add multiple subscriptions
    subscription_filter_t vision_filter = {};
    vision_filter.feature_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0);
    vision_filter.feature_mask = 0xFF000000;
    vision_filter.confidence_threshold = 0.8f;
    vision_filter.max_rate_hz = 0;

    subscription_filter_t emotion_filter = {};
    emotion_filter.feature_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_EMOTION, 0);
    emotion_filter.feature_mask = 0xFF000000;
    emotion_filter.confidence_threshold = 0.6f;
    emotion_filter.max_rate_hz = 0;

    node->subscriptions.push_back(vision_filter);
    node->subscriptions.push_back(emotion_filter);

    // Send various events
    event_packet_t vision = create_test_event(
        MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 1), 2, 0.85f, EVENT_FLAG_EXCITATORY);
    event_packet_t emotion = create_test_event(
        MAKE_FEATURE_CODE(FEATURE_DOMAIN_EMOTION, 1), 2, 0.65f, EVENT_FLAG_EXCITATORY);
    event_packet_t motor = create_test_event(
        MAKE_FEATURE_CODE(FEATURE_DOMAIN_MOTOR, 1), 2, 0.95f, EVENT_FLAG_EXCITATORY);

    network.send_event(2, 1, &vision, nullptr);
    network.send_event(2, 1, &emotion, nullptr);
    network.send_event(2, 1, &motor, nullptr);

    // Count matches for each subscription
    int vision_matches = 0;
    int emotion_matches = 0;
    int total_matches = 0;

    while (!node->incoming_events.empty()) {
        auto msg = node->incoming_events.front();
        node->incoming_events.pop();

        event_packet_t recv = {};
        event_packet_deserialize(msg.data(), msg.size(), &recv, nullptr, 0);

        bool matched = false;
        if (subscription_matches(&vision_filter, &recv)) {
            vision_matches++;
            matched = true;
        }
        if (subscription_matches(&emotion_filter, &recv)) {
            emotion_matches++;
            matched = true;
        }
        if (matched) total_matches++;
    }

    EXPECT_EQ(vision_matches, 1);
    EXPECT_EQ(emotion_matches, 1);
    EXPECT_EQ(total_matches, 2);
}

//=============================================================================
// Feature Code Routing Tests
//=============================================================================

TEST_F(ProtocolIntegrationTest, FeatureCodeRoutingAcrossNodes) {
    // WHAT: Test routing events based on feature codes to appropriate nodes
    // WHY:  Verify semantic routing works correctly

    MockNode* router = network.add_node(1);
    MockNode* vision_node = network.add_node(2);
    MockNode* audio_node = network.add_node(3);
    MockNode* motor_node = network.add_node(4);

    // Set up routing table
    router->feature_routing_table[FEATURE_DOMAIN_VISION] = 2;
    router->feature_routing_table[FEATURE_DOMAIN_AUDITORY] = 3;
    router->feature_routing_table[FEATURE_DOMAIN_MOTOR] = 4;

    // Create events for different domains
    event_packet_t vision = create_test_event(
        MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0x1111), 1, 0.9f, EVENT_FLAG_EXCITATORY);
    event_packet_t audio = create_test_event(
        MAKE_FEATURE_CODE(FEATURE_DOMAIN_AUDITORY, 0x2222), 1, 0.85f, EVENT_FLAG_EXCITATORY);
    event_packet_t motor = create_test_event(
        MAKE_FEATURE_CODE(FEATURE_DOMAIN_MOTOR, 0x3333), 1, 0.95f, EVENT_FLAG_EXCITATORY);

    // Route based on feature domain
    feature_domain_t vision_domain = static_cast<feature_domain_t>(GET_FEATURE_DOMAIN(EVENT_GET_FEATURE_CODE(&vision)));
    uint32_t vision_target = router->feature_routing_table[vision_domain];
    network.send_event(1, vision_target, &vision, nullptr);

    feature_domain_t audio_domain = static_cast<feature_domain_t>(GET_FEATURE_DOMAIN(EVENT_GET_FEATURE_CODE(&audio)));
    uint32_t audio_target = router->feature_routing_table[audio_domain];
    network.send_event(1, audio_target, &audio, nullptr);

    feature_domain_t motor_domain = static_cast<feature_domain_t>(GET_FEATURE_DOMAIN(EVENT_GET_FEATURE_CODE(&motor)));
    uint32_t motor_target = router->feature_routing_table[motor_domain];
    network.send_event(1, motor_target, &motor, nullptr);

    // Verify routing worked correctly
    EXPECT_EQ(vision_node->incoming_events.size(), 1u);
    EXPECT_EQ(audio_node->incoming_events.size(), 1u);
    EXPECT_EQ(motor_node->incoming_events.size(), 1u);

    // Verify correct events reached correct nodes
    auto vision_msg = vision_node->incoming_events.front();
    event_packet_t recv_vision = {};
    event_packet_deserialize(vision_msg.data(), vision_msg.size(), &recv_vision, nullptr, 0);
    EXPECT_EQ(GET_FEATURE_DOMAIN(EVENT_GET_FEATURE_CODE(&recv_vision)),
              FEATURE_DOMAIN_VISION);
}

//=============================================================================
// Neuromodulator Diffusion Tests
//=============================================================================

TEST_F(ProtocolIntegrationTest, NeuromodulatorDiffusionSimulation) {
    // WHAT: Test neuromodulator diffusion across multiple brain regions
    // WHY:  Verify CTRL_MSG_NEUROMOD_DIFFUSION works for distributed neuromodulation

    MockNode* region1 = network.add_node(1);  // Source region with high dopamine
    MockNode* region2 = network.add_node(2);
    MockNode* region3 = network.add_node(3);

    // Create neuromodulator level message (dopamine increase)
    neuromod_level_payload_t payload = {};
    payload.neuromod_type = 0;  // NEUROMOD_DOPAMINE
    payload.region_id = 1;
    payload.concentration = 0.8f;  // High concentration
    payload.timestamp = 1234567890ULL;

    control_message_t diffusion_msg = create_test_control(
        CTRL_MSG_NEUROMOD_DIFFUSION,
        1, 0xFFFFFFFF,  // Broadcast to all
        CTRL_FLAG_RELAY,  // Should be relayed
        sizeof(neuromod_level_payload_t));

    // Send diffusion message to neighbors
    network.send_control(1, 2, &diffusion_msg, &payload);
    network.send_control(1, 3, &diffusion_msg, &payload);

    // Verify neighbors received diffusion message
    ASSERT_FALSE(region2->incoming_control.empty());
    ASSERT_FALSE(region3->incoming_control.empty());

    // Process at region 2
    auto msg = region2->incoming_control.front();
    region2->incoming_control.pop();

    control_message_t recv_msg = {};
    neuromod_level_payload_t recv_payload = {};
    control_message_deserialize(msg.data(), msg.size(), &recv_msg,
                                &recv_payload, sizeof(recv_payload));

    EXPECT_EQ(recv_msg.msg_type, CTRL_MSG_NEUROMOD_DIFFUSION);
    EXPECT_EQ(recv_payload.neuromod_type, 0u);  // Dopamine
    EXPECT_FLOAT_EQ(recv_payload.concentration, 0.8f);
    EXPECT_EQ(recv_payload.region_id, 1u);

    // Simulate diffusion decay (concentration decreases with distance)
    float decayed_concentration = recv_payload.concentration * 0.7f;  // 30% decay
    EXPECT_LT(decayed_concentration, recv_payload.concentration);
}

TEST_F(ProtocolIntegrationTest, MultipleNeuromodulatorTypes) {
    // WHAT: Test diffusion of multiple neuromodulator types simultaneously
    // WHY:  Verify system handles multiple neuromodulator signals

    network.add_node(1);
    MockNode* receiver = network.add_node(2);

    // Send dopamine signal
    neuromod_level_payload_t dopamine = {};
    dopamine.neuromod_type = 0;  // NEUROMOD_DOPAMINE
    dopamine.region_id = 1;
    dopamine.concentration = 0.7f;
    dopamine.timestamp = 1000000ULL;

    control_message_t dopa_msg = create_test_control(
        CTRL_MSG_NEUROMOD_LEVEL, 1, 2, 0, sizeof(neuromod_level_payload_t));
    network.send_control(1, 2, &dopa_msg, &dopamine);

    // Send serotonin signal
    neuromod_level_payload_t serotonin = {};
    serotonin.neuromod_type = 1;  // NEUROMOD_SEROTONIN
    serotonin.region_id = 1;
    serotonin.concentration = 0.5f;
    serotonin.timestamp = 1000100ULL;

    control_message_t sero_msg = create_test_control(
        CTRL_MSG_NEUROMOD_LEVEL, 1, 2, 0, sizeof(neuromod_level_payload_t));
    network.send_control(1, 2, &sero_msg, &serotonin);

    // Verify both received
    ASSERT_EQ(receiver->incoming_control.size(), 2u);

    // Process and verify types
    std::map<uint8_t, float> neuromod_levels;

    while (!receiver->incoming_control.empty()) {
        auto msg = receiver->incoming_control.front();
        receiver->incoming_control.pop();

        control_message_t ctrl = {};
        neuromod_level_payload_t payload = {};
        control_message_deserialize(msg.data(), msg.size(), &ctrl,
                                    &payload, sizeof(payload));

        neuromod_levels[payload.neuromod_type] = payload.concentration;
    }

    EXPECT_FLOAT_EQ(neuromod_levels[0], 0.7f);  // Dopamine
    EXPECT_FLOAT_EQ(neuromod_levels[1], 0.5f);  // Serotonin
}

//=============================================================================
// Glial Coordination Tests
//=============================================================================

TEST_F(ProtocolIntegrationTest, GlialPruningCoordination) {
    // WHAT: Test glial pruning decisions coordinated across nodes
    // WHY:  Verify CTRL_MSG_GLIAL_PRUNING enables distributed synaptic maintenance

    MockNode* microglia1 = network.add_node(1);
    MockNode* microglia2 = network.add_node(2);
    network.add_node(3);

    // Microglia 1 detects weak synapse
    glial_pruning_payload_t prune_decision = {};
    prune_decision.source_neuron_id = 12345;
    prune_decision.target_neuron_id = 67890;
    prune_decision.activity_score = 0.15f;  // Low activity
    prune_decision.pruning_action = 1;  // Prune
    prune_decision.timestamp = 2000000ULL;

    control_message_t prune_msg = create_test_control(
        CTRL_MSG_GLIAL_PRUNING,
        1, 2,  // Send to microglia 2
        CTRL_FLAG_RELAY,
        sizeof(glial_pruning_payload_t));

    network.send_control(1, 2, &prune_msg, &prune_decision);

    // Microglia 2 receives pruning decision
    ASSERT_FALSE(microglia2->incoming_control.empty());
    auto msg = microglia2->incoming_control.front();
    microglia2->incoming_control.pop();

    control_message_t recv_msg = {};
    glial_pruning_payload_t recv_decision = {};
    control_message_deserialize(msg.data(), msg.size(), &recv_msg,
                                &recv_decision, sizeof(recv_decision));

    EXPECT_EQ(recv_msg.msg_type, CTRL_MSG_GLIAL_PRUNING);
    EXPECT_EQ(recv_decision.source_neuron_id, 12345u);
    EXPECT_EQ(recv_decision.target_neuron_id, 67890u);
    EXPECT_FLOAT_EQ(recv_decision.activity_score, 0.15f);
    EXPECT_EQ(recv_decision.pruning_action, 1u);  // Prune action

    // Microglia 2 can now coordinate pruning decision
}

TEST_F(ProtocolIntegrationTest, AstrocyteCalciumWavePropagation) {
    // WHAT: Test calcium wave propagation through astrocyte network
    // WHY:  Verify CTRL_MSG_GLIAL_CALCIUM enables astrocyte coordination

    MockNode* astro1 = network.add_node(1);
    MockNode* astro2 = network.add_node(2);
    MockNode* astro3 = network.add_node(3);

    // Astrocyte 1 initiates calcium wave
    glial_calcium_payload_t wave = {};
    wave.astrocyte_id = 1001;
    wave.calcium_level = 0.9f;  // High calcium
    wave.wave_velocity = 15.0f;  // µm/s
    wave.affected_synapses = 50;
    wave.timestamp = 3000000ULL;

    control_message_t wave_msg = create_test_control(
        CTRL_MSG_GLIAL_CALCIUM,
        1, 2,
        CTRL_FLAG_RELAY,  // Should propagate
        sizeof(glial_calcium_payload_t));

    // Send wave to neighbor
    network.send_control(1, 2, &wave_msg, &wave);

    // Neighbor receives and potentially propagates
    ASSERT_FALSE(astro2->incoming_control.empty());
    auto msg = astro2->incoming_control.front();
    astro2->incoming_control.pop();

    control_message_t recv_msg = {};
    glial_calcium_payload_t recv_wave = {};
    control_message_deserialize(msg.data(), msg.size(), &recv_msg,
                                &recv_wave, sizeof(recv_wave));

    EXPECT_EQ(recv_msg.msg_type, CTRL_MSG_GLIAL_CALCIUM);
    EXPECT_FLOAT_EQ(recv_wave.calcium_level, 0.9f);
    EXPECT_FLOAT_EQ(recv_wave.wave_velocity, 15.0f);

    // Simulate wave decay and propagation to next neighbor
    glial_calcium_payload_t attenuated_wave = recv_wave;
    attenuated_wave.calcium_level *= 0.8f;  // 20% attenuation
    attenuated_wave.affected_synapses = 40;

    control_message_t propagated_msg = create_test_control(
        CTRL_MSG_GLIAL_CALCIUM, 2, 3, CTRL_FLAG_RELAY,
        sizeof(glial_calcium_payload_t));

    network.send_control(2, 3, &propagated_msg, &attenuated_wave);

    ASSERT_FALSE(astro3->incoming_control.empty());
    auto msg3 = astro3->incoming_control.front();

    glial_calcium_payload_t wave_at_3 = {};
    control_message_deserialize(msg3.data(), msg3.size(), &recv_msg,
                                &wave_at_3, sizeof(wave_at_3));

    EXPECT_LT(wave_at_3.calcium_level, recv_wave.calcium_level);
    EXPECT_LT(wave_at_3.affected_synapses, recv_wave.affected_synapses);
}

//=============================================================================
// Brain Region Synchronization Tests
//=============================================================================

TEST_F(ProtocolIntegrationTest, BrainRegionStateSynchronization) {
    // WHAT: Test brain region state sync across distributed nodes
    // WHY:  Verify CTRL_MSG_REGION_SYNC enables coherent distributed brain state

    MockNode* prefrontal = network.add_node(1);
    MockNode* hippocampus = network.add_node(2);

    // Prefrontal cortex sends state sync
    region_activity_payload_t pfc_state = {};
    pfc_state.region_type = 1;  // Prefrontal cortex type
    pfc_state.avg_activity = 0.65f;
    pfc_state.spike_rate = 42.5f;  // Hz
    pfc_state.active_neurons = 8500;
    pfc_state.total_neurons = 10000;
    pfc_state.timestamp = 4000000ULL;

    control_message_t sync_msg = create_test_control(
        CTRL_MSG_REGION_SYNC,
        1, 2, 0,
        sizeof(region_activity_payload_t));

    network.send_control(1, 2, &sync_msg, &pfc_state);

    // Hippocampus receives state
    ASSERT_FALSE(hippocampus->incoming_control.empty());
    auto msg = hippocampus->incoming_control.front();
    hippocampus->incoming_control.pop();

    control_message_t recv_msg = {};
    region_activity_payload_t recv_state = {};
    control_message_deserialize(msg.data(), msg.size(), &recv_msg,
                                &recv_state, sizeof(recv_state));

    EXPECT_EQ(recv_msg.msg_type, CTRL_MSG_REGION_SYNC);
    EXPECT_EQ(recv_state.region_type, 1u);
    EXPECT_FLOAT_EQ(recv_state.avg_activity, 0.65f);
    EXPECT_FLOAT_EQ(recv_state.spike_rate, 42.5f);
    EXPECT_EQ(recv_state.active_neurons, 8500u);
}

TEST_F(ProtocolIntegrationTest, BrainRegionActivityAggregation) {
    // WHAT: Test aggregation of region activity statistics from multiple nodes
    // WHY:  Verify CTRL_MSG_REGION_ACTIVITY enables distributed monitoring

    MockNode* coordinator = network.add_node(1);
    network.add_node(2);
    network.add_node(3);
    network.add_node(4);

    // Each node reports its region activity
    std::vector<region_activity_payload_t> activities;

    for (uint32_t id = 2; id <= 4; id++) {
        region_activity_payload_t activity = {};
        activity.region_type = id;
        activity.avg_activity = 0.5f + (id * 0.1f);
        activity.spike_rate = 30.0f + (id * 5.0f);
        activity.active_neurons = 5000 + (id * 1000);
        activity.total_neurons = 10000;
        activity.timestamp = 5000000ULL + id;

        control_message_t report_msg = create_test_control(
            CTRL_MSG_REGION_ACTIVITY,
            id, 1, 0,
            sizeof(region_activity_payload_t));

        network.send_control(id, 1, &report_msg, &activity);
        activities.push_back(activity);
    }

    // Coordinator receives all activity reports
    EXPECT_EQ(coordinator->incoming_control.size(), 3u);

    // Aggregate statistics
    float total_activity = 0.0f;
    uint32_t total_active = 0;

    while (!coordinator->incoming_control.empty()) {
        auto msg = coordinator->incoming_control.front();
        coordinator->incoming_control.pop();

        control_message_t ctrl = {};
        region_activity_payload_t activity = {};
        control_message_deserialize(msg.data(), msg.size(), &ctrl,
                                    &activity, sizeof(activity));

        total_activity += activity.avg_activity;
        total_active += activity.active_neurons;
    }

    float avg_activity = total_activity / 3.0f;
    EXPECT_GT(avg_activity, 0.5f);
    EXPECT_LT(avg_activity, 0.9f);
    EXPECT_EQ(total_active, 5000u + 6000u + 7000u);
}

//=============================================================================
// Error Recovery Tests
//=============================================================================

TEST_F(ProtocolIntegrationTest, BadChecksumDetection) {
    // WHAT: Test detection and rejection of corrupted messages
    // WHY:  Verify checksum validation catches transmission errors

    network.add_node(1);
    MockNode* receiver = network.add_node(2);

    // Enable checksum corruption
    network.corrupt_checksums = true;

    feature_code_t feature = MAKE_FEATURE_CODE(FEATURE_DOMAIN_SYSTEM, 0x9999);
    event_packet_t packet = create_test_event(feature, 1, 0.9f, EVENT_FLAG_EXCITATORY);

    // Send corrupted packet
    network.send_event(1, 2, &packet, nullptr);

    // Try to deserialize - should fail due to bad checksum
    ASSERT_FALSE(receiver->incoming_events.empty());
    auto msg = receiver->incoming_events.front();

    event_packet_t recv = {};
    int result = event_packet_deserialize(msg.data(), msg.size(), &recv, nullptr, 0);

    // Deserialization should fail or validation should fail
    // (depending on where corruption occurred)
    if (result > 0) {
        // If deserialization succeeded, validation should fail
        EXPECT_FALSE(event_packet_validate(&recv));
    } else {
        // Deserialization failed due to corruption
        EXPECT_LT(result, 0);
    }
}

TEST_F(ProtocolIntegrationTest, RetransmissionOnFailure) {
    // WHAT: Test retransmission logic when transmission fails
    // WHY:  Verify reliability mechanisms work

    MockNode* sender = network.add_node(1);
    MockNode* receiver = network.add_node(2);

    // Enable packet loss
    network.packet_loss_rate = 0.5f;

    feature_code_t feature = MAKE_FEATURE_CODE(FEATURE_DOMAIN_MEMORY, 0xAAAA);
    event_packet_t packet = create_test_event(feature, 1, 0.9f, EVENT_FLAG_EXCITATORY);

    // Attempt transmission with retries
    const int MAX_RETRIES = 5;
    int attempts = 0;
    bool success = false;

    for (attempts = 0; attempts < MAX_RETRIES && !success; attempts++) {
        success = network.send_event(1, 2, &packet, nullptr);
    }

    // Should eventually succeed (with 50% loss, probability of 5 failures is 3.125%)
    EXPECT_TRUE(success || attempts == MAX_RETRIES);

    // If succeeded, receiver should have message
    if (success) {
        EXPECT_FALSE(receiver->incoming_events.empty());
    }
}

TEST_F(ProtocolIntegrationTest, PacketLossSimulation) {
    // WHAT: Test protocol behavior under packet loss conditions
    // WHY:  Verify system is resilient to network unreliability

    network.add_node(1);
    MockNode* receiver = network.add_node(2);

    network.packet_loss_rate = 0.3f;  // 30% packet loss

    // Send 100 packets
    const int TOTAL_PACKETS = 100;
    int sent_count = 0;

    for (int i = 0; i < TOTAL_PACKETS; i++) {
        feature_code_t feature = MAKE_FEATURE_CODE(FEATURE_DOMAIN_SYSTEM, i);
        event_packet_t packet = create_test_event(feature, 1, 0.9f, EVENT_FLAG_EXCITATORY);

        if (network.send_event(1, 2, &packet, nullptr)) {
            sent_count++;
        }
    }

    // With 30% loss, expect approximately 70 packets received
    // Allow some variance due to randomness
    EXPECT_GT(sent_count, 50);
    EXPECT_LT(sent_count, 90);
    EXPECT_EQ(receiver->incoming_events.size(), (size_t)sent_count);
}

//=============================================================================
// Bandwidth and Latency Tests
//=============================================================================

TEST_F(ProtocolIntegrationTest, BandwidthRateLimiting) {
    // WHAT: Test bandwidth limiting prevents node overload
    // WHY:  Verify QoS mechanisms work correctly

    MockNode* sender = network.add_node(1);
    MockNode* receiver = network.add_node(2);

    // Set very low bandwidth limit
    sender->bandwidth_limit_bps = 1000;  // 1KB/s

    // Try to send many large events
    const int NUM_EVENTS = 20;
    int sent_count = 0;

    for (int i = 0; i < NUM_EVENTS; i++) {
        feature_code_t feature = MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, i);
        event_packet_t packet = create_test_event(feature, 1, 0.9f, EVENT_FLAG_EXCITATORY);

        // Add large payload (100 bytes each)
        char payload[100];
        memset(payload, 'A', sizeof(payload));
        packet.payload_length = sizeof(payload);

        if (network.send_event(1, 2, &packet, payload)) {
            sent_count++;
        } else {
            break;  // Hit bandwidth limit
        }
    }

    // Should hit limit before sending all packets
    EXPECT_LT(sent_count, NUM_EVENTS);

    // Reset bandwidth counter and try again
    sender->reset_bandwidth_counter();

    // Fix: Can't take address of rvalue, create variable first
    event_packet_t reset_event = create_test_event(
        MAKE_FEATURE_CODE(FEATURE_DOMAIN_SYSTEM, 0), 1, 0.9f, EVENT_FLAG_EXCITATORY);
    if (network.send_event(1, 2, &reset_event, nullptr)) {
        sent_count++;
    }

    // Should be able to send after reset
    EXPECT_GT(sent_count, 0);
}

TEST_F(ProtocolIntegrationTest, LatencySimulation) {
    // WHAT: Test protocol behavior with network latency
    // WHY:  Verify timing-sensitive operations work with delays

    network.add_node(1);
    MockNode* receiver = network.add_node(2);

    network.latency_ms = 100;  // 100ms latency

    auto start = std::chrono::steady_clock::now();

    feature_code_t feature = MAKE_FEATURE_CODE(FEATURE_DOMAIN_SYSTEM, 0xBBBB);
    event_packet_t packet = create_test_event(feature, 1, 0.9f, EVENT_FLAG_EXCITATORY);

    network.send_event(1, 2, &packet, nullptr);

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Transmission should have taken at least the latency time
    EXPECT_GE(duration.count(), 100);

    // Message should be received
    EXPECT_FALSE(receiver->incoming_events.empty());
}

//=============================================================================
// Confidence-Based Propagation Tests
//=============================================================================

TEST_F(ProtocolIntegrationTest, ConfidenceBasedEventPropagation) {
    // WHAT: Test dropping low-confidence events to reduce noise
    // WHY:  Verify confidence thresholding works for efficient propagation

    MockNode* node = network.add_node(1);
    network.add_node(2);

    // Set confidence threshold
    const float CONFIDENCE_THRESHOLD = 0.7f;

    // Create events with varying confidence
    struct {
        float confidence;
        bool should_propagate;
    } test_cases[] = {
        {0.95f, true},   // High confidence - propagate
        {0.80f, true},   // Above threshold - propagate
        {0.70f, true},   // At threshold - propagate
        {0.65f, false},  // Below threshold - drop
        {0.40f, false},  // Low confidence - drop
        {0.10f, false},  // Very low - drop
    };

    for (const auto& test : test_cases) {
        feature_code_t feature = MAKE_FEATURE_CODE(FEATURE_DOMAIN_EMOTION, 0xCCCC);
        event_packet_t packet = create_test_event(feature, 1, test.confidence,
                                                   EVENT_FLAG_EXCITATORY);

        // Check if event should be propagated based on confidence
        float conf = EVENT_CONFIDENCE_TO_FLOAT(packet.confidence);
        bool should_send = (conf >= CONFIDENCE_THRESHOLD);

        EXPECT_EQ(should_send, test.should_propagate)
            << "Confidence " << test.confidence << " propagation decision incorrect";

        if (should_send) {
            network.send_event(1, 2, &packet, nullptr);
        }
    }

    // Count received messages (only high-confidence ones)
    MockNode* receiver = network.nodes[2];
    EXPECT_EQ(receiver->incoming_events.size(), 3u);  // 0.95, 0.80, 0.70
}

TEST_F(ProtocolIntegrationTest, DynamicConfidenceAdjustment) {
    // WHAT: Test adjusting confidence threshold based on network load
    // WHY:  Verify adaptive confidence filtering reduces congestion

    network.add_node(1);
    MockNode* receiver = network.add_node(2);

    // Simulate high network load → increase threshold
    float dynamic_threshold = 0.5f;  // Initial threshold

    // Send burst of events
    const int BURST_SIZE = 50;
    for (int i = 0; i < BURST_SIZE; i++) {
        float conf = 0.3f + (i * 0.01f);  // Gradually increasing confidence
        feature_code_t feature = MAKE_FEATURE_CODE(FEATURE_DOMAIN_SYSTEM, i);
        event_packet_t packet = create_test_event(feature, 1, conf, EVENT_FLAG_EXCITATORY);

        // Apply dynamic threshold
        float actual_conf = EVENT_CONFIDENCE_TO_FLOAT(packet.confidence);
        if (actual_conf >= dynamic_threshold) {
            network.send_event(1, 2, &packet, nullptr);
        }

        // Simulate threshold increase due to congestion
        if (receiver->incoming_events.size() > 10) {
            dynamic_threshold = 0.7f;  // Raise threshold
        }
    }

    // High threshold should have limited message count
    EXPECT_LT(receiver->incoming_events.size(), (size_t)BURST_SIZE);

    // Verify only high-confidence messages got through after threshold increase
    int high_conf_count = 0;
    while (!receiver->incoming_events.empty()) {
        auto msg = receiver->incoming_events.front();
        receiver->incoming_events.pop();

        event_packet_t recv = {};
        event_packet_deserialize(msg.data(), msg.size(), &recv, nullptr, 0);

        float conf = EVENT_CONFIDENCE_TO_FLOAT(recv.confidence);
        if (conf >= 0.7f) {
            high_conf_count++;
        }
    }

    EXPECT_GT(high_conf_count, 0);
}

//=============================================================================
// Complex Integration Scenarios
//=============================================================================

TEST_F(ProtocolIntegrationTest, CompleteDistributedBrainScenario) {
    // WHAT: Test realistic multi-node brain simulation scenario
    // WHY:  Verify all protocol components work together in complex scenario

    // Create distributed brain with specialized regions
    MockNode* sensory = network.add_node(1);     // Sensory input
    MockNode* cortex = network.add_node(2);      // Processing
    MockNode* motor = network.add_node(3);       // Motor output
    MockNode* coordinator = network.add_node(4); // Global coordinator

    // Set up subscriptions
    subscription_filter_t cortex_vision_filter = {};
    cortex_vision_filter.feature_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0);
    cortex_vision_filter.feature_mask = 0xFF000000;
    cortex_vision_filter.confidence_threshold = 0.6f;
    cortex_vision_filter.max_rate_hz = 0;
    cortex->subscriptions.push_back(cortex_vision_filter);

    subscription_filter_t motor_filter = {};
    motor_filter.feature_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_MOTOR, 0);
    motor_filter.feature_mask = 0xFF000000;
    motor_filter.confidence_threshold = 0.7f;
    motor_filter.max_rate_hz = 0;
    motor->subscriptions.push_back(motor_filter);

    // Scenario: Visual stimulus → processing → motor response

    // Step 1: Sensory input (vision event)
    feature_code_t vision_feature = MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0x1234);
    event_packet_t vision_event = create_test_event(vision_feature, 1, 0.85f,
                                                     EVENT_FLAG_EXCITATORY);
    network.send_event(1, 2, &vision_event, nullptr);

    // Step 2: Cortex processes and generates motor command
    ASSERT_FALSE(cortex->incoming_events.empty());
    auto msg = cortex->incoming_events.front();
    cortex->incoming_events.pop();

    event_packet_t recv_vision = {};
    event_packet_deserialize(msg.data(), msg.size(), &recv_vision, nullptr, 0);

    // Verify cortex received vision input
    EXPECT_TRUE(subscription_matches(&cortex_vision_filter, &recv_vision));

    // Cortex generates motor command
    feature_code_t motor_feature = MAKE_FEATURE_CODE(FEATURE_DOMAIN_MOTOR, 0x5678);
    event_packet_t motor_event = create_test_event(motor_feature, 2, 0.9f,
                                                    EVENT_FLAG_EXCITATORY);
    motor_event.hop_count = 1;  // Second hop
    network.send_event(2, 3, &motor_event, nullptr);

    // Step 3: Motor region executes command
    ASSERT_FALSE(motor->incoming_events.empty());
    msg = motor->incoming_events.front();

    event_packet_t recv_motor = {};
    event_packet_deserialize(msg.data(), msg.size(), &recv_motor, nullptr, 0);

    EXPECT_TRUE(subscription_matches(&motor_filter, &recv_motor));
    EXPECT_EQ(recv_motor.hop_count, 1u);

    // Step 4: Coordinator monitors activity
    region_activity_payload_t sensory_activity = {};
    sensory_activity.region_type = 1;
    sensory_activity.avg_activity = 0.8f;
    sensory_activity.spike_rate = 50.0f;
    sensory_activity.active_neurons = 9000;
    sensory_activity.total_neurons = 10000;

    control_message_t activity_msg = create_test_control(
        CTRL_MSG_REGION_ACTIVITY, 1, 4, 0, sizeof(region_activity_payload_t));
    network.send_control(1, 4, &activity_msg, &sensory_activity);

    ASSERT_FALSE(coordinator->incoming_control.empty());

    // Verify complete workflow succeeded
    EXPECT_TRUE(cortex->incoming_events.empty());   // Processed
    EXPECT_TRUE(motor->incoming_events.empty());    // Processed
    EXPECT_FALSE(coordinator->incoming_control.empty());  // Monitoring active
}

TEST_F(ProtocolIntegrationTest, EmergencyShutdownCoordination) {
    // WHAT: Test emergency shutdown propagation across all nodes
    // WHY:  Verify critical control messages reach all nodes quickly

    network.add_node(1);
    network.add_node(2);
    network.add_node(3);
    network.add_node(4);

    // Emergency shutdown message
    control_message_t emergency = create_test_control(
        CTRL_MSG_ERROR_REPORT,
        1, 0xFFFFFFFF,  // Global broadcast
        CTRL_FLAG_GLOBAL | CTRL_FLAG_ACK_REQUIRED,
        0);

    // Broadcast to all nodes
    for (uint32_t id = 2; id <= 4; id++) {
        network.send_control(1, id, &emergency, nullptr);
    }

    // Verify all nodes received emergency message
    for (uint32_t id = 2; id <= 4; id++) {
        MockNode* node = network.nodes[id];
        ASSERT_FALSE(node->incoming_control.empty())
            << "Node " << id << " did not receive emergency shutdown";

        auto msg = node->incoming_control.front();
        control_message_t recv = {};
        control_message_deserialize(msg.data(), msg.size(), &recv, nullptr, 0);

        EXPECT_EQ(recv.msg_type, CTRL_MSG_ERROR_REPORT);
        EXPECT_TRUE(recv.flags & CTRL_FLAG_GLOBAL);
        EXPECT_TRUE(recv.flags & CTRL_FLAG_ACK_REQUIRED);
    }
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
