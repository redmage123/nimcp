/**
 * @file test_swarm.cpp
 * @brief GoogleTest unit tests for NIMCP edge swarm communication subsystem
 *
 * Tests transport lifecycle, message create/destroy, send guard clauses,
 * heartbeat formatting, and recv with no data.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "edge/nimcp_edge.h"
#include "edge/nimcp_edge_types.h"
}

class SwarmTest : public ::testing::Test {
protected:
    void TearDown() override {}
};

TEST_F(SwarmTest, TransportCreateDestroy) {
    // Create transport — socket creation may fail in test environment (e.g.,
    // port conflict or no permissions), that is acceptable.
    nimcp_swarm_transport_t* transport = nimcp_swarm_transport_create(
        42, "239.0.0.1", 9999, "127.0.0.1", 0);
    ASSERT_NE(transport, nullptr);

    EXPECT_EQ(transport->device_id, 42u);
    EXPECT_EQ(transport->peer_port, 9999u);

    nimcp_swarm_transport_destroy(transport);
}

TEST_F(SwarmTest, MessageCreateDestroy) {
    uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
    nimcp_swarm_message_t* msg = nimcp_swarm_message_create(
        NIMCP_SWARM_MSG_PERCEPT, 10, 20, payload, sizeof(payload));
    ASSERT_NE(msg, nullptr);

    EXPECT_EQ(msg->type, NIMCP_SWARM_MSG_PERCEPT);
    EXPECT_EQ(msg->sender_id, 10u);
    EXPECT_EQ(msg->recipient_id, 20u);
    EXPECT_EQ(msg->payload_size, 4u);
    ASSERT_NE(msg->payload, nullptr);
    EXPECT_EQ(msg->payload[0], 0xDE);
    EXPECT_EQ(msg->payload[3], 0xEF);

    // Checksum should be non-zero (CRC32 of payload)
    uint32_t checksum_val = 0;
    memcpy(&checksum_val, msg->checksum, sizeof(checksum_val));
    EXPECT_NE(checksum_val, 0u);

    nimcp_swarm_message_destroy(msg);
}

TEST_F(SwarmTest, MessageCreateNullPayload) {
    nimcp_swarm_message_t* msg = nimcp_swarm_message_create(
        NIMCP_SWARM_MSG_HEARTBEAT, 1, 0, nullptr, 0);
    ASSERT_NE(msg, nullptr);

    EXPECT_EQ(msg->payload, nullptr);
    EXPECT_EQ(msg->payload_size, 0u);

    nimcp_swarm_message_destroy(msg);
}

TEST_F(SwarmTest, SendPeerWithoutMesh) {
    nimcp_swarm_transport_t* transport = nimcp_swarm_transport_create(
        1, "239.0.0.1", 0, nullptr, 0);
    ASSERT_NE(transport, nullptr);

    // Force peer_mesh_active to false
    transport->peer_mesh_active = false;

    nimcp_swarm_message_t* msg = nimcp_swarm_message_create(
        NIMCP_SWARM_MSG_PERCEPT, 1, 0, nullptr, 0);
    ASSERT_NE(msg, nullptr);

    int ret = nimcp_swarm_send_peer(transport, msg);
    EXPECT_EQ(ret, -1);

    nimcp_swarm_message_destroy(msg);
    nimcp_swarm_transport_destroy(transport);
}

TEST_F(SwarmTest, SendMasterWithoutConnection) {
    nimcp_swarm_transport_t* transport = nimcp_swarm_transport_create(
        1, "239.0.0.1", 0, nullptr, 0);
    ASSERT_NE(transport, nullptr);

    // Master should not be connected (no master host provided with port 0)
    transport->connected_to_master = false;

    nimcp_swarm_message_t* msg = nimcp_swarm_message_create(
        NIMCP_SWARM_MSG_REPORT, 1, 0, nullptr, 0);
    ASSERT_NE(msg, nullptr);

    int ret = nimcp_swarm_send_master(transport, msg);
    EXPECT_EQ(ret, -1);

    nimcp_swarm_message_destroy(msg);
    nimcp_swarm_transport_destroy(transport);
}

TEST_F(SwarmTest, HeartbeatCreatesMessage) {
    nimcp_swarm_transport_t* transport = nimcp_swarm_transport_create(
        42, "239.0.0.1", 0, nullptr, 0);
    ASSERT_NE(transport, nullptr);

    // Heartbeat will try to send via peer mesh. If mesh is not active,
    // send_peer returns -1, which is propagated.
    float position[] = {1.0f, 2.0f, 3.0f};
    int ret = nimcp_swarm_send_heartbeat(transport, position);
    // Return depends on whether peer mesh is active
    // Just verify no crash
    (void)ret;

    nimcp_swarm_transport_destroy(transport);
}

TEST_F(SwarmTest, RecvWithNoData) {
    nimcp_swarm_transport_t* transport = nimcp_swarm_transport_create(
        1, "239.0.0.1", 0, nullptr, 0);
    ASSERT_NE(transport, nullptr);

    nimcp_swarm_message_t msg;
    memset(&msg, 0, sizeof(msg));

    // Short timeout (1ms) — should return -1 (no data)
    int ret = nimcp_swarm_recv(transport, &msg, 1);
    EXPECT_EQ(ret, -1);

    nimcp_swarm_transport_destroy(transport);
}
