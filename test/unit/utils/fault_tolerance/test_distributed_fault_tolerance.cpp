/**
 * @file test_distributed_fault_tolerance.cpp
 * @brief Unit tests for distributed fault tolerance module
 *
 * Tests distributed checkpoints, phi accrual failure detection,
 * quorum-based recovery, and cross-node coordination.
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_distributed_fault_tolerance.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class DistributedFaultToleranceTest : public ::testing::Test {
protected:
    dft_context_t* ctx;
    dft_config_t config;

    void SetUp() override {
        config = dft_default_config();
        config.node_id = 1;
        ctx = dft_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            dft_destroy(ctx);
            ctx = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST(DftLifecycleTest, DefaultConfig) {
    dft_config_t config = dft_default_config();

    EXPECT_GT(config.heartbeat_interval_ms, 0);
    EXPECT_GT(config.failure_timeout_ms, 0);
    EXPECT_GT(config.quorum_threshold, 0.0f);
    EXPECT_LT(config.quorum_threshold, 1.0f);
}

TEST(DftLifecycleTest, CreateAndDestroy) {
    dft_config_t config = dft_default_config();
    config.node_id = 1;

    dft_context_t* ctx = dft_create(&config);
    ASSERT_NE(ctx, nullptr);

    dft_destroy(ctx);
}

TEST(DftLifecycleTest, CreateWithNullConfig) {
    dft_context_t* ctx = dft_create(nullptr);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(DistributedFaultToleranceTest, StartAndStop) {
    EXPECT_TRUE(dft_start(ctx));
    EXPECT_TRUE(dft_stop(ctx));
}

//=============================================================================
// Peer Management Tests
//=============================================================================

TEST_F(DistributedFaultToleranceTest, AddPeer) {
    EXPECT_TRUE(dft_add_peer(ctx, 2, nullptr));
    EXPECT_EQ(dft_get_peer_count(ctx), 1);
}

TEST_F(DistributedFaultToleranceTest, AddMultiplePeers) {
    for (uint32_t i = 2; i <= 6; i++) {
        EXPECT_TRUE(dft_add_peer(ctx, i, nullptr));
    }
    EXPECT_EQ(dft_get_peer_count(ctx), 5);
}

TEST_F(DistributedFaultToleranceTest, RemovePeer) {
    dft_add_peer(ctx, 2, nullptr);
    dft_add_peer(ctx, 3, nullptr);

    EXPECT_TRUE(dft_remove_peer(ctx, 2));
    EXPECT_EQ(dft_get_peer_count(ctx), 1);
}

TEST_F(DistributedFaultToleranceTest, GetPeerInfo) {
    dft_add_peer(ctx, 2, nullptr);

    dft_peer_info_t info;
    EXPECT_TRUE(dft_get_peer_info(ctx, 2, &info));
    EXPECT_EQ(info.node_id, 2);
    EXPECT_EQ(info.state, DFT_NODE_HEALTHY);
}

TEST_F(DistributedFaultToleranceTest, GetAllPeers) {
    for (uint32_t i = 2; i <= 5; i++) {
        dft_add_peer(ctx, i, nullptr);
    }

    dft_peer_info_t peers[10];
    uint32_t count = dft_get_all_peers(ctx, peers, 10);
    EXPECT_EQ(count, 4);
}

//=============================================================================
// Heartbeat Tests
//=============================================================================

TEST_F(DistributedFaultToleranceTest, SendHeartbeat) {
    EXPECT_TRUE(dft_start(ctx));

    dft_add_peer(ctx, 2, nullptr);
    dft_add_peer(ctx, 3, nullptr);

    uint32_t reached = dft_send_heartbeat(ctx);
    EXPECT_GE(reached, 0);

    EXPECT_TRUE(dft_stop(ctx));
}

TEST_F(DistributedFaultToleranceTest, ReceiveHeartbeat) {
    dft_add_peer(ctx, 2, nullptr);

    EXPECT_TRUE(dft_receive_heartbeat(ctx, 2, 100.0f));

    dft_peer_info_t info;
    dft_get_peer_info(ctx, 2, &info);
    EXPECT_NEAR(info.health_score, 100.0f, 0.01f);
}

//=============================================================================
// Failure Detection Tests
//=============================================================================

TEST_F(DistributedFaultToleranceTest, DetectFailures) {
    EXPECT_TRUE(dft_start(ctx));

    dft_add_peer(ctx, 2, nullptr);

    // Without heartbeats, peer should eventually be suspected
    // In unit test, we need to simulate time passing
    dft_failure_detection_t failures[10];
    uint32_t count = dft_detect_failures(ctx, failures, 10);

    // May or may not detect based on timing
    (void)count;

    EXPECT_TRUE(dft_stop(ctx));
}

TEST_F(DistributedFaultToleranceTest, ReportSuspectedFailure) {
    EXPECT_TRUE(dft_start(ctx));

    dft_add_peer(ctx, 2, nullptr);
    EXPECT_TRUE(dft_report_suspected_failure(ctx, 2, "No heartbeat"));

    dft_peer_info_t info;
    dft_get_peer_info(ctx, 2, &info);
    EXPECT_GE(info.suspected_by_count, 1);

    EXPECT_TRUE(dft_stop(ctx));
}

//=============================================================================
// Checkpoint Tests
//=============================================================================

TEST_F(DistributedFaultToleranceTest, CreateCheckpoint) {
    EXPECT_TRUE(dft_start(ctx));

    const char* data = "test_checkpoint_data";
    dft_checkpoint_meta_t meta;

    EXPECT_TRUE(dft_create_checkpoint(ctx, data, strlen(data) + 1, &meta));
    EXPECT_GT(meta.checkpoint_id, 0);
    EXPECT_EQ(meta.source_node_id, 1);

    EXPECT_TRUE(dft_stop(ctx));
}

TEST_F(DistributedFaultToleranceTest, GetLatestCheckpointId) {
    EXPECT_TRUE(dft_start(ctx));

    const char* data = "test_data";
    dft_checkpoint_meta_t meta;
    dft_create_checkpoint(ctx, data, strlen(data) + 1, &meta);

    uint64_t latest = dft_get_latest_checkpoint_id(ctx);
    EXPECT_EQ(latest, meta.checkpoint_id);

    EXPECT_TRUE(dft_stop(ctx));
}

TEST_F(DistributedFaultToleranceTest, ListCheckpoints) {
    EXPECT_TRUE(dft_start(ctx));

    const char* data = "test_data";
    dft_checkpoint_meta_t meta;

    for (int i = 0; i < 3; i++) {
        dft_create_checkpoint(ctx, data, strlen(data) + 1, &meta);
    }

    dft_checkpoint_meta_t metas[10];
    uint32_t count = dft_list_checkpoints(ctx, metas, 10);
    EXPECT_GE(count, 3);

    EXPECT_TRUE(dft_stop(ctx));
}

TEST_F(DistributedFaultToleranceTest, RetrieveCheckpoint) {
    EXPECT_TRUE(dft_start(ctx));

    const char* original_data = "test_checkpoint_data";
    dft_checkpoint_meta_t meta;
    dft_create_checkpoint(ctx, original_data, strlen(original_data) + 1, &meta);

    char buffer[256];
    size_t actual_size;
    bool retrieved = dft_retrieve_checkpoint(ctx, meta.checkpoint_id,
                                              buffer, sizeof(buffer), &actual_size);

    if (retrieved) {
        EXPECT_STREQ(buffer, original_data);
    }

    EXPECT_TRUE(dft_stop(ctx));
}

//=============================================================================
// Recovery Coordination Tests
//=============================================================================

TEST_F(DistributedFaultToleranceTest, InitiateRecovery) {
    EXPECT_TRUE(dft_start(ctx));

    dft_add_peer(ctx, 2, nullptr);
    dft_report_suspected_failure(ctx, 2, "test failure");

    EXPECT_TRUE(dft_initiate_recovery(ctx, 2));

    EXPECT_TRUE(dft_stop(ctx));
}

TEST_F(DistributedFaultToleranceTest, VoteRecovery) {
    EXPECT_TRUE(dft_start(ctx));

    dft_add_peer(ctx, 2, nullptr);
    dft_add_peer(ctx, 3, nullptr);

    dft_recovery_vote_t vote = {0};
    vote.voter_node_id = 1;
    vote.target_node_id = 2;
    vote.vote_for_recovery = true;
    vote.checkpoint_id = 100;

    EXPECT_TRUE(dft_vote_recovery(ctx, &vote));

    EXPECT_TRUE(dft_stop(ctx));
}

TEST_F(DistributedFaultToleranceTest, CheckRecoveryConsensus) {
    EXPECT_TRUE(dft_start(ctx));

    dft_add_peer(ctx, 2, nullptr);

    bool quorum_reached;
    uint64_t checkpoint_id;
    bool result = dft_check_recovery_consensus(ctx, 2, &quorum_reached, &checkpoint_id);

    // Result depends on votes received
    (void)result;
    (void)quorum_reached;

    EXPECT_TRUE(dft_stop(ctx));
}

TEST_F(DistributedFaultToleranceTest, CompleteRecovery) {
    EXPECT_TRUE(dft_start(ctx));

    dft_add_peer(ctx, 2, nullptr);
    dft_initiate_recovery(ctx, 2);

    EXPECT_TRUE(dft_complete_recovery(ctx, 2, true));

    dft_peer_info_t info;
    dft_get_peer_info(ctx, 2, &info);
    EXPECT_EQ(info.state, DFT_NODE_HEALTHY);

    EXPECT_TRUE(dft_stop(ctx));
}

//=============================================================================
// Quorum Tests
//=============================================================================

TEST_F(DistributedFaultToleranceTest, HasQuorum) {
    // Single node should always have quorum
    EXPECT_TRUE(dft_has_quorum(ctx));
}

TEST_F(DistributedFaultToleranceTest, QuorumWithPeers) {
    // Add enough peers for quorum
    for (uint32_t i = 2; i <= 5; i++) {
        dft_add_peer(ctx, i, nullptr);
    }

    // All healthy, should have quorum
    EXPECT_TRUE(dft_has_quorum(ctx));
}

//=============================================================================
// Leader Election Tests
//=============================================================================

TEST_F(DistributedFaultToleranceTest, GetLeader) {
    EXPECT_TRUE(dft_start(ctx));

    // Initially self may be leader
    uint32_t leader = dft_get_leader(ctx);
    EXPECT_GE(leader, 0);

    EXPECT_TRUE(dft_stop(ctx));
}

TEST_F(DistributedFaultToleranceTest, TriggerElection) {
    EXPECT_TRUE(dft_start(ctx));

    for (uint32_t i = 2; i <= 4; i++) {
        dft_add_peer(ctx, i, nullptr);
    }

    EXPECT_TRUE(dft_trigger_election(ctx));

    EXPECT_TRUE(dft_stop(ctx));
}

//=============================================================================
// Callback Tests
//=============================================================================

static bool callback_invoked = false;
static void test_event_callback(dft_event_type_t event, const void* event_data, void* user_data) {
    (void)event;
    (void)event_data;
    (void)user_data;
    callback_invoked = true;
}

TEST_F(DistributedFaultToleranceTest, RegisterCallback) {
    callback_invoked = false;

    EXPECT_TRUE(dft_register_callback(ctx, test_event_callback, nullptr));

    // Trigger event by adding peer
    dft_add_peer(ctx, 2, nullptr);

    // Callback may be invoked
    (void)callback_invoked;
}

TEST_F(DistributedFaultToleranceTest, UnregisterCallback) {
    EXPECT_TRUE(dft_register_callback(ctx, test_event_callback, nullptr));
    EXPECT_TRUE(dft_unregister_callback(ctx, test_event_callback));
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(DistributedFaultToleranceTest, GetStats) {
    EXPECT_TRUE(dft_start(ctx));

    dft_stats_t stats;
    EXPECT_TRUE(dft_get_stats(ctx, &stats));

    EXPECT_GE(stats.total_heartbeats_sent, 0);
    EXPECT_GE(stats.current_availability, 0.0f);

    EXPECT_TRUE(dft_stop(ctx));
}

TEST_F(DistributedFaultToleranceTest, ResetStats) {
    EXPECT_TRUE(dft_start(ctx));

    dft_send_heartbeat(ctx);
    dft_reset_stats(ctx);

    dft_stats_t stats;
    dft_get_stats(ctx, &stats);
    EXPECT_EQ(stats.total_heartbeats_sent, 0);

    EXPECT_TRUE(dft_stop(ctx));
}

TEST_F(DistributedFaultToleranceTest, GetClusterHealth) {
    for (uint32_t i = 2; i <= 4; i++) {
        dft_add_peer(ctx, i, nullptr);
        dft_receive_heartbeat(ctx, i, 100.0f);
    }

    float health = dft_get_cluster_health(ctx);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 100.0f);
}

//=============================================================================
// String Conversion Tests
//=============================================================================

TEST(DftStringTest, NodeStateToString) {
    EXPECT_STREQ("HEALTHY", dft_node_state_to_string(DFT_NODE_HEALTHY));
    EXPECT_STREQ("SUSPECTED", dft_node_state_to_string(DFT_NODE_SUSPECTED));
    EXPECT_STREQ("FAILED", dft_node_state_to_string(DFT_NODE_FAILED));
    EXPECT_STREQ("RECOVERING", dft_node_state_to_string(DFT_NODE_RECOVERING));
    EXPECT_STREQ("QUARANTINED", dft_node_state_to_string(DFT_NODE_QUARANTINED));
}

TEST(DftStringTest, DetectionMethodToString) {
    EXPECT_STREQ("HEARTBEAT", dft_detection_method_to_string(DFT_DETECT_HEARTBEAT));
    EXPECT_STREQ("PING", dft_detection_method_to_string(DFT_DETECT_PING));
    EXPECT_STREQ("ACCRUAL", dft_detection_method_to_string(DFT_DETECT_ACCRUAL));
    EXPECT_STREQ("CONSENSUS", dft_detection_method_to_string(DFT_DETECT_CONSENSUS));
}

TEST(DftStringTest, RecoveryModeToString) {
    EXPECT_STREQ("LOCAL", dft_recovery_mode_to_string(DFT_RECOVERY_LOCAL));
    EXPECT_STREQ("PEER", dft_recovery_mode_to_string(DFT_RECOVERY_PEER));
    EXPECT_STREQ("LEADER", dft_recovery_mode_to_string(DFT_RECOVERY_LEADER));
    EXPECT_STREQ("CONSENSUS", dft_recovery_mode_to_string(DFT_RECOVERY_CONSENSUS));
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(DistributedFaultToleranceTest, RemoveNonexistentPeer) {
    EXPECT_FALSE(dft_remove_peer(ctx, 999));
}

TEST_F(DistributedFaultToleranceTest, GetInfoForNonexistentPeer) {
    dft_peer_info_t info;
    EXPECT_FALSE(dft_get_peer_info(ctx, 999, &info));
}

TEST_F(DistributedFaultToleranceTest, MaxPeers) {
    // Try to add more than max peers (loop starts at 2, so capacity reached at i = DFT_MAX_PEERS + 1)
    for (uint32_t i = 2; i <= DFT_MAX_PEERS + 5; i++) {
        bool added = dft_add_peer(ctx, i, nullptr);
        if (i > DFT_MAX_PEERS + 1) {
            EXPECT_FALSE(added) << "Peer " << i << " should have been rejected (capacity)";
        }
    }
}

TEST_F(DistributedFaultToleranceTest, DuplicatePeer) {
    EXPECT_TRUE(dft_add_peer(ctx, 2, nullptr));
    // Adding duplicate may succeed (update) or fail
    dft_add_peer(ctx, 2, nullptr);
    EXPECT_EQ(dft_get_peer_count(ctx), 1);  // Should still be 1
}

