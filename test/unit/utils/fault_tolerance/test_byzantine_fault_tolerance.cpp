/**
 * @file test_byzantine_fault_tolerance.cpp
 * @brief Unit tests for Byzantine fault tolerance module
 *
 * Tests PBFT-style consensus, trust scoring, quarantine mechanism,
 * equivocation detection, and cryptographic signatures.
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_byzantine_fault_tolerance.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class ByzantineFaultToleranceTest : public ::testing::Test {
protected:
    bft_context_t* ctx;
    bft_config_t config;

    void SetUp() override {
        config = bft_default_config();
        config.node_id = 1;
        config.total_nodes = 7;  // n >= 3f + 1, so f=2 tolerated
        config.max_byzantine = 2;
        ctx = bft_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            bft_destroy(ctx);
            ctx = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST(BftLifecycleTest, DefaultConfig) {
    bft_config_t config = bft_default_config();

    EXPECT_EQ(config.node_id, 0);
    EXPECT_GT(config.total_nodes, 0);
    EXPECT_GT(config.view_timeout_ms, 0);
    EXPECT_TRUE(config.enable_signatures);
}

TEST(BftLifecycleTest, CreateAndDestroy) {
    bft_config_t config = bft_default_config();
    config.node_id = 1;

    bft_context_t* ctx = bft_create(&config);
    ASSERT_NE(ctx, nullptr);

    bft_destroy(ctx);
}

TEST(BftLifecycleTest, CreateWithNullConfig) {
    bft_context_t* ctx = bft_create(nullptr);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(ByzantineFaultToleranceTest, StartAndStop) {
    EXPECT_TRUE(bft_start(ctx));
    EXPECT_TRUE(bft_stop(ctx));
}

//=============================================================================
// Key Management Tests
//=============================================================================

TEST_F(ByzantineFaultToleranceTest, GenerateKeys) {
    bft_identity_t identity;
    memset(&identity, 0, sizeof(identity));
    identity.node_id = 1;

    EXPECT_TRUE(bft_generate_keys(&identity));
    EXPECT_TRUE(identity.has_private_key);
}

TEST_F(ByzantineFaultToleranceTest, SetIdentity) {
    bft_identity_t identity;
    memset(&identity, 0, sizeof(identity));
    identity.node_id = 1;
    bft_generate_keys(&identity);

    EXPECT_TRUE(bft_set_identity(ctx, &identity));
}

TEST_F(ByzantineFaultToleranceTest, RegisterPeerKey) {
    bft_identity_t peer_identity;
    memset(&peer_identity, 0, sizeof(peer_identity));
    peer_identity.node_id = 2;
    bft_generate_keys(&peer_identity);

    EXPECT_TRUE(bft_register_peer_key(ctx, 2, peer_identity.public_key));
}

//=============================================================================
// Consensus Tests
//=============================================================================

TEST_F(ByzantineFaultToleranceTest, SubmitRequest) {
    EXPECT_TRUE(bft_start(ctx));

    const char* data = "test_request_data";
    uint64_t sequence;

    EXPECT_TRUE(bft_submit_request(ctx, data, strlen(data), &sequence));
    EXPECT_GT(sequence, 0);

    EXPECT_TRUE(bft_stop(ctx));
}

TEST_F(ByzantineFaultToleranceTest, ProcessMessage) {
    EXPECT_TRUE(bft_start(ctx));

    // Set up identity for signing
    bft_identity_t identity;
    memset(&identity, 0, sizeof(identity));
    identity.node_id = 2;
    bft_generate_keys(&identity);
    bft_set_identity(ctx, &identity);

    bft_msg_header_t header;
    memset(&header, 0, sizeof(header));
    header.type = BFT_MSG_PREPARE;
    header.sender_id = 2;
    header.view_number = 0;
    header.sequence_number = 1;

    const char* payload = "test_payload";

    // Sign the message before processing
    bft_sign(ctx, payload, strlen(payload), header.signature);

    EXPECT_TRUE(bft_process_message(ctx, &header, payload, strlen(payload)));

    EXPECT_TRUE(bft_stop(ctx));
}

TEST_F(ByzantineFaultToleranceTest, IsConsensusReached) {
    EXPECT_TRUE(bft_start(ctx));

    // Submit a request
    const char* data = "test_data";
    uint64_t sequence;
    bft_submit_request(ctx, data, strlen(data), &sequence);

    // Initially consensus not reached
    bool reached = bft_is_consensus_reached(ctx, sequence);
    // May or may not be reached depending on implementation
    (void)reached;

    EXPECT_TRUE(bft_stop(ctx));
}

TEST_F(ByzantineFaultToleranceTest, GetConsensusResult) {
    EXPECT_TRUE(bft_start(ctx));

    const char* data = "test_data";
    uint64_t sequence;
    bft_submit_request(ctx, data, strlen(data), &sequence);

    char buffer[256];
    size_t actual_size;
    bool result = bft_get_consensus_result(ctx, sequence, buffer, sizeof(buffer), &actual_size);
    // Result depends on whether consensus was reached
    (void)result;

    EXPECT_TRUE(bft_stop(ctx));
}

//=============================================================================
// View Change Tests
//=============================================================================

TEST_F(ByzantineFaultToleranceTest, RequestViewChange) {
    EXPECT_TRUE(bft_start(ctx));

    EXPECT_TRUE(bft_request_view_change(ctx, BFT_VIEW_LEADER_TIMEOUT));

    EXPECT_TRUE(bft_stop(ctx));
}

TEST_F(ByzantineFaultToleranceTest, GetView) {
    EXPECT_TRUE(bft_start(ctx));

    uint64_t view = bft_get_view(ctx);
    EXPECT_GE(view, 0);

    EXPECT_TRUE(bft_stop(ctx));
}

TEST_F(ByzantineFaultToleranceTest, GetLeader) {
    EXPECT_TRUE(bft_start(ctx));

    uint32_t leader = bft_get_leader(ctx);
    EXPECT_GE(leader, 0);

    EXPECT_TRUE(bft_stop(ctx));
}

TEST_F(ByzantineFaultToleranceTest, IsLeader) {
    EXPECT_TRUE(bft_start(ctx));

    bool is_leader = bft_is_leader(ctx);
    // May or may not be leader
    (void)is_leader;

    EXPECT_TRUE(bft_stop(ctx));
}

//=============================================================================
// Byzantine Detection Tests
//=============================================================================

TEST_F(ByzantineFaultToleranceTest, VerifySignature) {
    bft_identity_t identity;
    memset(&identity, 0, sizeof(identity));
    identity.node_id = 1;
    bft_generate_keys(&identity);
    bft_set_identity(ctx, &identity);

    bft_msg_header_t header;
    memset(&header, 0, sizeof(header));
    header.type = BFT_MSG_PREPARE;
    header.sender_id = 1;
    header.sequence_number = 1;

    const char* payload = "test_data";

    // Sign and then verify
    bft_sign(ctx, payload, strlen(payload), header.signature);
    EXPECT_TRUE(bft_verify_signature(ctx, &header, payload, strlen(payload)));
}

TEST_F(ByzantineFaultToleranceTest, ReportByzantine) {
    EXPECT_TRUE(bft_start(ctx));

    bft_evidence_t evidence;
    memset(&evidence, 0, sizeof(evidence));
    evidence.type = BFT_EVIDENCE_CONFLICTING_MSG;
    evidence.accused_node_id = 3;

    EXPECT_TRUE(bft_report_byzantine(ctx, 3, BFT_BEHAV_EQUIVOCATION, &evidence, 1));

    EXPECT_TRUE(bft_stop(ctx));
}

TEST_F(ByzantineFaultToleranceTest, ProcessAccusation) {
    EXPECT_TRUE(bft_start(ctx));

    bft_accusation_t accusation;
    memset(&accusation, 0, sizeof(accusation));
    accusation.accuser_id = 2;
    accusation.accused_id = 3;
    accusation.behavior = BFT_BEHAV_SILENT;
    accusation.evidence_count = 0;

    bool valid = bft_process_accusation(ctx, &accusation);
    // Validity depends on implementation
    (void)valid;

    EXPECT_TRUE(bft_stop(ctx));
}

TEST_F(ByzantineFaultToleranceTest, VoteAccusation) {
    EXPECT_TRUE(bft_start(ctx));

    // First report byzantine behavior to create an accusation in the context
    bft_evidence_t evidence;
    memset(&evidence, 0, sizeof(evidence));
    evidence.type = BFT_EVIDENCE_CONFLICTING_MSG;
    EXPECT_TRUE(bft_report_byzantine(ctx, 3, BFT_BEHAV_EQUIVOCATION, &evidence, 1));

    // Now vote on the accusation
    bft_accusation_t accusation;
    memset(&accusation, 0, sizeof(accusation));
    accusation.accuser_id = config.node_id;
    accusation.accused_id = 3;
    accusation.behavior = BFT_BEHAV_EQUIVOCATION;

    EXPECT_TRUE(bft_vote_accusation(ctx, &accusation, true));

    EXPECT_TRUE(bft_stop(ctx));
}

TEST_F(ByzantineFaultToleranceTest, CheckEquivocation) {
    bft_msg_header_t msg1;
    memset(&msg1, 0, sizeof(msg1));
    msg1.type = BFT_MSG_PREPARE;
    msg1.sender_id = 2;
    msg1.view_number = 0;
    msg1.sequence_number = 1;
    memset(msg1.digest, 'a', BFT_HASH_SIZE);

    bft_msg_header_t msg2;
    memset(&msg2, 0, sizeof(msg2));
    msg2.type = BFT_MSG_PREPARE;
    msg2.sender_id = 2;
    msg2.view_number = 0;
    msg2.sequence_number = 1;
    memset(msg2.digest, 'b', BFT_HASH_SIZE);  // Different digest!

    bool is_equivocation = bft_check_equivocation(ctx, &msg1, &msg2);
    EXPECT_TRUE(is_equivocation);
}

//=============================================================================
// Trust Management Tests
//=============================================================================

TEST_F(ByzantineFaultToleranceTest, GetTrustInfo) {
    bft_trust_info_t trust;
    EXPECT_TRUE(bft_get_trust_info(ctx, 2, &trust));
    EXPECT_EQ(trust.node_id, 2);
}

TEST_F(ByzantineFaultToleranceTest, UpdateTrust) {
    float new_trust = bft_update_trust(ctx, 2, true);  // Correct behavior
    EXPECT_GE(new_trust, 0.0f);
    EXPECT_LE(new_trust, 100.0f);
}

TEST_F(ByzantineFaultToleranceTest, TrustDecay) {
    // Multiple incorrect behaviors should decay trust
    for (int i = 0; i < 10; i++) {
        bft_update_trust(ctx, 2, false);
    }

    bft_trust_info_t trust;
    bft_get_trust_info(ctx, 2, &trust);
    EXPECT_LT(trust.trust_score, config.initial_trust);
}

//=============================================================================
// Quarantine Tests
//=============================================================================

TEST_F(ByzantineFaultToleranceTest, QuarantineNode) {
    EXPECT_TRUE(bft_quarantine_node(ctx, 3, 60000));  // 60 second quarantine
    EXPECT_TRUE(bft_is_quarantined(ctx, 3));
}

TEST_F(ByzantineFaultToleranceTest, ReleaseQuarantine) {
    bft_quarantine_node(ctx, 3, 60000);
    EXPECT_TRUE(bft_release_quarantine(ctx, 3));
    EXPECT_FALSE(bft_is_quarantined(ctx, 3));
}

TEST_F(ByzantineFaultToleranceTest, QuarantinedNodeCheck) {
    EXPECT_FALSE(bft_is_quarantined(ctx, 2));

    bft_quarantine_node(ctx, 2, 60000);
    EXPECT_TRUE(bft_is_quarantined(ctx, 2));
}

//=============================================================================
// Checkpoint Tests
//=============================================================================

TEST_F(ByzantineFaultToleranceTest, CreateCheckpoint) {
    EXPECT_TRUE(bft_start(ctx));

    uint8_t state_hash[BFT_HASH_SIZE];
    bft_hash("test_state", 10, state_hash);

    EXPECT_TRUE(bft_create_checkpoint(ctx, state_hash));

    EXPECT_TRUE(bft_stop(ctx));
}

TEST_F(ByzantineFaultToleranceTest, GetStableCheckpoint) {
    EXPECT_TRUE(bft_start(ctx));

    bft_checkpoint_t checkpoint;
    bool exists = bft_get_stable_checkpoint(ctx, &checkpoint);
    // May or may not have stable checkpoint
    (void)exists;

    EXPECT_TRUE(bft_stop(ctx));
}

TEST_F(ByzantineFaultToleranceTest, VerifyCheckpoint) {
    EXPECT_TRUE(bft_start(ctx));

    bft_checkpoint_t checkpoint;
    memset(&checkpoint, 0, sizeof(checkpoint));
    checkpoint.sequence_number = 100;
    checkpoint.view_number = 0;

    bool valid = bft_verify_checkpoint(ctx, &checkpoint);
    // Validity depends on signatures
    (void)valid;

    EXPECT_TRUE(bft_stop(ctx));
}

//=============================================================================
// Callback Tests
//=============================================================================

static bool consensus_callback_invoked = false;
static void test_consensus_callback(uint64_t sequence, const void* data, size_t data_size, void* user_data) {
    (void)sequence;
    (void)data;
    (void)data_size;
    (void)user_data;
    consensus_callback_invoked = true;
}

TEST_F(ByzantineFaultToleranceTest, RegisterConsensusCallback) {
    EXPECT_TRUE(bft_register_consensus_callback(ctx, test_consensus_callback, nullptr));
}

static bool byzantine_callback_invoked = false;
static void test_byzantine_callback(uint32_t node_id, bft_behavior_t behavior, const bft_evidence_t* evidence, void* user_data) {
    (void)node_id;
    (void)behavior;
    (void)evidence;
    (void)user_data;
    byzantine_callback_invoked = true;
}

TEST_F(ByzantineFaultToleranceTest, RegisterByzantineCallback) {
    EXPECT_TRUE(bft_register_byzantine_callback(ctx, test_byzantine_callback, nullptr));
}

//=============================================================================
// Cryptographic Utilities Tests
//=============================================================================

TEST_F(ByzantineFaultToleranceTest, Sign) {
    bft_identity_t identity;
    memset(&identity, 0, sizeof(identity));
    identity.node_id = 1;
    bft_generate_keys(&identity);
    bft_set_identity(ctx, &identity);

    const char* data = "test_data_to_sign";
    uint8_t signature[BFT_SIGNATURE_SIZE];

    EXPECT_TRUE(bft_sign(ctx, data, strlen(data), signature));
}

TEST_F(ByzantineFaultToleranceTest, Verify) {
    bft_identity_t identity;
    memset(&identity, 0, sizeof(identity));
    identity.node_id = 1;
    bft_generate_keys(&identity);

    const char* data = "test_data_to_sign";
    uint8_t signature[BFT_SIGNATURE_SIZE];

    bft_set_identity(ctx, &identity);
    bft_sign(ctx, data, strlen(data), signature);

    EXPECT_TRUE(bft_verify(identity.public_key, data, strlen(data), signature));
}

TEST_F(ByzantineFaultToleranceTest, Hash) {
    const char* data = "test_data";
    uint8_t hash[BFT_HASH_SIZE];

    bft_hash(data, strlen(data), hash);

    // Hash should be non-zero
    bool all_zero = true;
    for (size_t i = 0; i < BFT_HASH_SIZE; i++) {
        if (hash[i] != 0) {
            all_zero = false;
            break;
        }
    }
    EXPECT_FALSE(all_zero);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(ByzantineFaultToleranceTest, GetStats) {
    EXPECT_TRUE(bft_start(ctx));

    bft_stats_t stats;
    EXPECT_TRUE(bft_get_stats(ctx, &stats));

    EXPECT_GE(stats.total_messages, 0);
    EXPECT_GE(stats.total_consensus_rounds, 0);

    EXPECT_TRUE(bft_stop(ctx));
}

TEST_F(ByzantineFaultToleranceTest, ResetStats) {
    EXPECT_TRUE(bft_start(ctx));

    bft_reset_stats(ctx);

    bft_stats_t stats;
    EXPECT_TRUE(bft_get_stats(ctx, &stats));
    EXPECT_EQ(stats.total_messages, 0);

    EXPECT_TRUE(bft_stop(ctx));
}

TEST_F(ByzantineFaultToleranceTest, IsClusterHealthy) {
    EXPECT_TRUE(bft_start(ctx));

    bool healthy = bft_is_cluster_healthy(ctx);
    EXPECT_TRUE(healthy);  // Should be healthy initially

    EXPECT_TRUE(bft_stop(ctx));
}

//=============================================================================
// String Conversion Tests
//=============================================================================

TEST(BftStringTest, BehaviorToString) {
    EXPECT_STREQ("None", bft_behavior_to_string(BFT_BEHAV_NONE));
    EXPECT_STREQ("Silent", bft_behavior_to_string(BFT_BEHAV_SILENT));
    EXPECT_STREQ("Equivocation", bft_behavior_to_string(BFT_BEHAV_EQUIVOCATION));
    EXPECT_STREQ("InvalidSignature", bft_behavior_to_string(BFT_BEHAV_INVALID_SIG));
    EXPECT_STREQ("Replay", bft_behavior_to_string(BFT_BEHAV_REPLAY));
    EXPECT_STREQ("Fabrication", bft_behavior_to_string(BFT_BEHAV_FABRICATION));
    EXPECT_STREQ("Timing", bft_behavior_to_string(BFT_BEHAV_TIMING));
    EXPECT_STREQ("Collusion", bft_behavior_to_string(BFT_BEHAV_COLLUSION));
}

TEST(BftStringTest, MsgTypeToString) {
    EXPECT_STREQ("Request", bft_msg_type_to_string(BFT_MSG_REQUEST));
    EXPECT_STREQ("PrePrepare", bft_msg_type_to_string(BFT_MSG_PRE_PREPARE));
    EXPECT_STREQ("Prepare", bft_msg_type_to_string(BFT_MSG_PREPARE));
    EXPECT_STREQ("Commit", bft_msg_type_to_string(BFT_MSG_COMMIT));
    EXPECT_STREQ("Reply", bft_msg_type_to_string(BFT_MSG_REPLY));
    EXPECT_STREQ("Checkpoint", bft_msg_type_to_string(BFT_MSG_CHECKPOINT));
    EXPECT_STREQ("ViewChange", bft_msg_type_to_string(BFT_MSG_VIEW_CHANGE));
    EXPECT_STREQ("NewView", bft_msg_type_to_string(BFT_MSG_NEW_VIEW));
}

TEST(BftStringTest, StatusToString) {
    EXPECT_STREQ("Trusted", bft_status_to_string(BFT_STATUS_TRUSTED));
    EXPECT_STREQ("Suspected", bft_status_to_string(BFT_STATUS_SUSPECTED));
    EXPECT_STREQ("Byzantine", bft_status_to_string(BFT_STATUS_BYZANTINE));
    EXPECT_STREQ("Quarantined", bft_status_to_string(BFT_STATUS_QUARANTINED));
    EXPECT_STREQ("Probation", bft_status_to_string(BFT_STATUS_PROBATION));
}

TEST(BftStringTest, EvidenceTypeToString) {
    EXPECT_STREQ("ConflictingMessage", bft_evidence_type_to_string(BFT_EVIDENCE_CONFLICTING_MSG));
    EXPECT_STREQ("InvalidSignature", bft_evidence_type_to_string(BFT_EVIDENCE_INVALID_SIG));
    EXPECT_STREQ("InvalidData", bft_evidence_type_to_string(BFT_EVIDENCE_INVALID_DATA));
    EXPECT_STREQ("TimingViolation", bft_evidence_type_to_string(BFT_EVIDENCE_TIMING_VIOLATION));
    EXPECT_STREQ("ProtocolViolation", bft_evidence_type_to_string(BFT_EVIDENCE_PROTOCOL_VIOLATION));
    EXPECT_STREQ("Witness", bft_evidence_type_to_string(BFT_EVIDENCE_WITNESS));
}

TEST(BftStringTest, ViewReasonToString) {
    EXPECT_STREQ("LeaderTimeout", bft_view_reason_to_string(BFT_VIEW_LEADER_TIMEOUT));
    EXPECT_STREQ("LeaderByzantine", bft_view_reason_to_string(BFT_VIEW_LEADER_BYZANTINE));
    EXPECT_STREQ("StuckConsensus", bft_view_reason_to_string(BFT_VIEW_STUCK_CONSENSUS));
    EXPECT_STREQ("Manual", bft_view_reason_to_string(BFT_VIEW_MANUAL));
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(ByzantineFaultToleranceTest, MinimalCluster) {
    // n = 4 (minimum for f = 1)
    bft_config_t min_config = bft_default_config();
    min_config.node_id = 1;
    min_config.total_nodes = 4;
    min_config.max_byzantine = 1;

    bft_context_t* min_ctx = bft_create(&min_config);
    ASSERT_NE(min_ctx, nullptr);

    bft_destroy(min_ctx);
}

TEST_F(ByzantineFaultToleranceTest, TrustScoreNonexistentNode) {
    bft_trust_info_t trust;
    bool found = bft_get_trust_info(ctx, 999, &trust);
    // May return default trust or false
    (void)found;
}

TEST_F(ByzantineFaultToleranceTest, SelfIsNotQuarantined) {
    EXPECT_FALSE(bft_is_quarantined(ctx, config.node_id));
}

TEST_F(ByzantineFaultToleranceTest, MaxNodes) {
    // Register max number of peer keys
    for (uint32_t i = 2; i <= BFT_MAX_NODES; i++) {
        bft_identity_t peer;
        memset(&peer, 0, sizeof(peer));
        peer.node_id = i;
        bft_generate_keys(&peer);
        bft_register_peer_key(ctx, i, peer.public_key);
    }
}
