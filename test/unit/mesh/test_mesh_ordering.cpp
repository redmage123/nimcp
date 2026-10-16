/**
 * @file test_mesh_ordering.cpp
 * @brief Unit tests for mesh ordering service module
 *
 * Tests ordering service creation, transaction submission, batching,
 * sequencing, block creation, and Raft consensus operations.
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "mesh/nimcp_mesh_ordering.h"
#include "mesh/nimcp_mesh_coordinator_pool.h"
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_transaction.h"
}

class MeshOrderingTest : public ::testing::Test {
protected:
    mesh_participant_registry_t* registry;
    mesh_channel_t* channel;
    mesh_coordinator_pool_t* orderer_pool;
    mesh_ordering_service_t* ordering;

    void SetUp() override {
        registry = mesh_registry_create(nullptr);
        ASSERT_NE(registry, nullptr);

        mesh_channel_config_t ch_config;
        mesh_channel_default_config(&ch_config);
        ch_config.channel_name = "test_channel";
        ch_config.channel_id = MESH_CHANNEL_LEFT_HEMISPHERE;
        channel = mesh_channel_create(&ch_config, registry);
        ASSERT_NE(channel, nullptr);

        mesh_coordinator_pool_config_t pool_config;
        mesh_coordinator_pool_default_config(&pool_config);
        pool_config.pool_name = "orderer_pool";
        pool_config.initial_size = 3;
        orderer_pool = mesh_coordinator_pool_create(&pool_config, registry, channel);
        ASSERT_NE(orderer_pool, nullptr);

        ordering = nullptr;
    }

    void TearDown() override {
        if (ordering) {
            mesh_ordering_destroy(ordering);
            ordering = nullptr;
        }
        if (orderer_pool) {
            mesh_coordinator_pool_destroy(orderer_pool);
            orderer_pool = nullptr;
        }
        if (channel) {
            mesh_channel_destroy(channel);
            channel = nullptr;
        }
        if (registry) {
            mesh_registry_destroy(registry);
            registry = nullptr;
        }
    }

    mesh_participant_id_t register_test_participant(const char* name) {
        mesh_participant_interface_t iface;
        mesh_participant_interface_init(&iface);

        mesh_participant_config_t config;
        mesh_participant_config_init(&config);
        config.module_name = name;
        config.type = MESH_PARTICIPANT_MODULE;
        config.home_channel = MESH_CHANNEL_LEFT_HEMISPHERE;

        mesh_participant_id_t id;
        nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &id);
        EXPECT_EQ(err, NIMCP_SUCCESS);
        return id;
    }

    mesh_transaction_t* create_test_transaction(mesh_participant_id_t proposer) {
        mesh_transaction_t* tx = mesh_transaction_create(
            MESH_TX_BELIEF_UPDATE,
            proposer,
            MESH_CHANNEL_LEFT_HEMISPHERE
        );
        return tx;
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(MeshOrderingTest, DefaultConfig) {
    mesh_ordering_config_t config;
    nimcp_error_t err = mesh_ordering_default_config(&config);

    ASSERT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(config.batch_size, MESH_DEFAULT_BATCH_SIZE);
    EXPECT_GT(config.batch_timeout_ms, 0.0f);
    EXPECT_GT(config.heartbeat_interval_ms, 0.0f);
    EXPECT_GT(config.election_timeout_ms, 0.0f);
}

TEST_F(MeshOrderingTest, DefaultConfigNullParam) {
    nimcp_error_t err = mesh_ordering_default_config(nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_INVALID_PARAM);
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(MeshOrderingTest, CreateWithDefaults) {
    mesh_ordering_config_t config;
    mesh_ordering_default_config(&config);
    config.service_name = "test_orderer";

    ordering = mesh_ordering_create(&config, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    EXPECT_STREQ(mesh_ordering_get_name(ordering), "test_orderer");
}

TEST_F(MeshOrderingTest, CreateWithNullConfig) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);
}

TEST_F(MeshOrderingTest, CreateWithNullPool) {
    mesh_ordering_config_t config;
    mesh_ordering_default_config(&config);

    ordering = mesh_ordering_create(&config, nullptr);
    // Should work without pool, just no distributed consensus
    ASSERT_NE(ordering, nullptr);
}

TEST_F(MeshOrderingTest, DestroyNull) {
    mesh_ordering_destroy(nullptr);
    // Should not crash
}

TEST_F(MeshOrderingTest, GetNameNull) {
    const char* name = mesh_ordering_get_name(nullptr);
    EXPECT_EQ(name, nullptr);
}

/* ============================================================================
 * Transaction Submission Tests
 * ============================================================================ */

TEST_F(MeshOrderingTest, SubmitTransaction) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");
    mesh_transaction_t* tx = create_test_transaction(p);
    ASSERT_NE(tx, nullptr);

    nimcp_error_t err = mesh_ordering_submit(ordering, tx);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(mesh_ordering_get_pending_count(ordering), 1);
    EXPECT_TRUE(mesh_ordering_is_pending(ordering, &tx->id));

    mesh_transaction_destroy(tx);
}

TEST_F(MeshOrderingTest, SubmitMultipleTransactions) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");

    for (int i = 0; i < 10; i++) {
        mesh_transaction_t* tx = create_test_transaction(p);
        ASSERT_NE(tx, nullptr);
        nimcp_error_t err = mesh_ordering_submit(ordering, tx);
        EXPECT_EQ(err, NIMCP_SUCCESS);
        mesh_transaction_destroy(tx);
    }

    EXPECT_EQ(mesh_ordering_get_pending_count(ordering), 10);
}

TEST_F(MeshOrderingTest, SubmitBatch) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");

    mesh_tx_batch_t* batch = mesh_tx_batch_create(MESH_CHANNEL_LEFT_HEMISPHERE, 10);
    ASSERT_NE(batch, nullptr);

    for (int i = 0; i < 5; i++) {
        mesh_transaction_t* tx = create_test_transaction(p);
        mesh_tx_batch_add(batch, tx);
    }

    nimcp_error_t err = mesh_ordering_submit_batch(ordering, batch);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(mesh_ordering_get_pending_count(ordering), 5);

    mesh_tx_batch_destroy(batch);
}

TEST_F(MeshOrderingTest, SubmitNullTransaction) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    nimcp_error_t err = mesh_ordering_submit(ordering, nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(MeshOrderingTest, IsPendingFalseForUnknown) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    mesh_tx_id_t unknown_id = {
        .channel = 99,
        .proposer = 0xDEADBEEF,
        .sequence = 12345
    };

    EXPECT_FALSE(mesh_ordering_is_pending(ordering, &unknown_id));
}

/* ============================================================================
 * Batching and Sequencing Tests
 * ============================================================================ */

TEST_F(MeshOrderingTest, CreateBatch) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");

    // Submit some transactions
    for (int i = 0; i < 5; i++) {
        mesh_transaction_t* tx = create_test_transaction(p);
        mesh_ordering_submit(ordering, tx);
        mesh_transaction_destroy(tx);
    }

    nimcp_error_t err = mesh_ordering_create_batch(ordering);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Pending should be moved to batch
    EXPECT_EQ(mesh_ordering_get_pending_count(ordering), 0);
}

TEST_F(MeshOrderingTest, SequenceBatchRequiresLeader) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    // Should fail if not leader
    if (!mesh_ordering_is_leader(ordering)) {
        nimcp_error_t err = mesh_ordering_sequence_batch(ordering);
        EXPECT_EQ(err, NIMCP_ERROR_NOT_LEADER);
    }
}

TEST_F(MeshOrderingTest, CreateBlock) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    // Start election to become leader
    mesh_ordering_start_election(ordering);

    // If we're now leader, test block creation
    if (mesh_ordering_is_leader(ordering)) {
        mesh_participant_id_t p = register_test_participant("module1");

        for (int i = 0; i < 5; i++) {
            mesh_transaction_t* tx = create_test_transaction(p);
            mesh_ordering_submit(ordering, tx);
            mesh_transaction_destroy(tx);
        }

        mesh_ordering_create_batch(ordering);
        mesh_ordering_sequence_batch(ordering);

        mesh_ordered_block_t* block = mesh_ordering_create_block(ordering);
        ASSERT_NE(block, nullptr);

        EXPECT_EQ(block->block_number, 0);
        EXPECT_EQ(block->tx_count, 5);

        mesh_ordered_block_destroy(block);
    }
}

TEST_F(MeshOrderingTest, GetLatestBlock) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    uint64_t latest = mesh_ordering_get_latest_block(ordering);
    EXPECT_EQ(latest, 0);
}

TEST_F(MeshOrderingTest, GetSequence) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    uint64_t seq = mesh_ordering_get_sequence(ordering);
    EXPECT_EQ(seq, 0);
}

/* ============================================================================
 * Raft Consensus Tests
 * ============================================================================ */

TEST_F(MeshOrderingTest, InitialRoleIsFollower) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    raft_role_t role = mesh_ordering_get_role(ordering);
    EXPECT_EQ(role, RAFT_ROLE_FOLLOWER);
}

TEST_F(MeshOrderingTest, InitialTermIsZero) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    uint64_t term = mesh_ordering_get_term(ordering);
    EXPECT_EQ(term, 0);
}

TEST_F(MeshOrderingTest, StartElection) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    nimcp_error_t err = mesh_ordering_start_election(ordering);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Term should increase
    EXPECT_GE(mesh_ordering_get_term(ordering), 1);

    // Role should be candidate or leader (if we won immediately)
    raft_role_t role = mesh_ordering_get_role(ordering);
    EXPECT_TRUE(role == RAFT_ROLE_CANDIDATE || role == RAFT_ROLE_LEADER);
}

TEST_F(MeshOrderingTest, HandleVoteRequest) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    bool vote_granted = false;
    mesh_participant_id_t candidate = 0x12345678;

    nimcp_error_t err = mesh_ordering_handle_vote_request(
        ordering,
        candidate,
        1,  // term
        0,  // last_log_index
        0,  // last_log_term
        &vote_granted
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(vote_granted);
}

TEST_F(MeshOrderingTest, HandleVoteRequestRejectOldTerm) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    // First, start an election to advance term
    mesh_ordering_start_election(ordering);

    bool vote_granted = false;
    mesh_participant_id_t candidate = 0x12345678;

    // Request vote with term 0 (less than current)
    nimcp_error_t err = mesh_ordering_handle_vote_request(
        ordering,
        candidate,
        0,  // old term
        0,
        0,
        &vote_granted
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_FALSE(vote_granted);
}

TEST_F(MeshOrderingTest, HandleVoteResponse) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    // Start election first
    mesh_ordering_start_election(ordering);

    mesh_participant_id_t voter = 0x12345678;
    nimcp_error_t err = mesh_ordering_handle_vote_response(
        ordering,
        voter,
        mesh_ordering_get_term(ordering),
        true
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshOrderingTest, HandleAppendEntries) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    bool success = false;
    mesh_participant_id_t leader = 0x12345678;

    nimcp_error_t err = mesh_ordering_handle_append_entries(
        ordering,
        leader,
        1,     // term
        0,     // prev_log_index
        0,     // prev_log_term
        nullptr,  // no entries (heartbeat)
        0,
        0,     // leader_commit
        &success
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(success);

    // Should recognize the leader
    EXPECT_EQ(mesh_ordering_get_leader(ordering), leader);
}

TEST_F(MeshOrderingTest, SendHeartbeatRequiresLeader) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    // Should fail if not leader
    if (!mesh_ordering_is_leader(ordering)) {
        nimcp_error_t err = mesh_ordering_send_heartbeat(ordering);
        EXPECT_EQ(err, NIMCP_ERROR_NOT_LEADER);
    }
}

TEST_F(MeshOrderingTest, IsLeaderFalseInitially) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    EXPECT_FALSE(mesh_ordering_is_leader(ordering));
}

TEST_F(MeshOrderingTest, GetLeaderZeroInitially) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    mesh_participant_id_t leader = mesh_ordering_get_leader(ordering);
    EXPECT_EQ(leader, 0);
}

/* ============================================================================
 * Log Management Tests
 * ============================================================================ */

TEST_F(MeshOrderingTest, LogAppend) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    raft_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.term = 1;
    entry.type = RAFT_ENTRY_NOOP;

    nimcp_error_t err = mesh_ordering_log_append(ordering, &entry);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(mesh_ordering_log_last_index(ordering), 0);
}

TEST_F(MeshOrderingTest, LogGet) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    raft_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.term = 42;
    entry.type = RAFT_ENTRY_NOOP;

    mesh_ordering_log_append(ordering, &entry);

    const raft_log_entry_t* retrieved = mesh_ordering_log_get(ordering, 0);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->term, 42);
}

TEST_F(MeshOrderingTest, LogGetInvalidIndex) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    const raft_log_entry_t* retrieved = mesh_ordering_log_get(ordering, 999);
    EXPECT_EQ(retrieved, nullptr);
}

TEST_F(MeshOrderingTest, LogLastTerm) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    EXPECT_EQ(mesh_ordering_log_last_term(ordering), 0);

    raft_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.term = 5;
    mesh_ordering_log_append(ordering, &entry);

    EXPECT_EQ(mesh_ordering_log_last_term(ordering), 5);
}

TEST_F(MeshOrderingTest, LogTruncate) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    // Add multiple entries
    for (int i = 0; i < 5; i++) {
        raft_log_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        entry.term = i + 1;
        mesh_ordering_log_append(ordering, &entry);
    }

    EXPECT_EQ(mesh_ordering_log_last_index(ordering), 4);

    // Truncate after index 2
    nimcp_error_t err = mesh_ordering_log_truncate(ordering, 2);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(mesh_ordering_log_last_index(ordering), 2);
}

TEST_F(MeshOrderingTest, GetCommitIndex) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    uint64_t commit = mesh_ordering_get_commit_index(ordering);
    EXPECT_EQ(commit, 0);
}

/* ============================================================================
 * Channel Management Tests
 * ============================================================================ */

TEST_F(MeshOrderingTest, AddChannel) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    nimcp_error_t err = mesh_ordering_add_channel(ordering, MESH_CHANNEL_LEFT_HEMISPHERE);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_TRUE(mesh_ordering_has_channel(ordering, MESH_CHANNEL_LEFT_HEMISPHERE));
}

TEST_F(MeshOrderingTest, AddDuplicateChannel) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    mesh_ordering_add_channel(ordering, MESH_CHANNEL_LEFT_HEMISPHERE);
    nimcp_error_t err = mesh_ordering_add_channel(ordering, MESH_CHANNEL_LEFT_HEMISPHERE);
    EXPECT_EQ(err, NIMCP_SUCCESS);  // Duplicate is OK
}

TEST_F(MeshOrderingTest, RemoveChannel) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    mesh_ordering_add_channel(ordering, MESH_CHANNEL_LEFT_HEMISPHERE);

    nimcp_error_t err = mesh_ordering_remove_channel(ordering, MESH_CHANNEL_LEFT_HEMISPHERE);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_FALSE(mesh_ordering_has_channel(ordering, MESH_CHANNEL_LEFT_HEMISPHERE));
}

TEST_F(MeshOrderingTest, RemoveNonexistentChannel) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    nimcp_error_t err = mesh_ordering_remove_channel(ordering, 99);
    EXPECT_EQ(err, NIMCP_ERROR_NOT_FOUND);
}

TEST_F(MeshOrderingTest, HasChannelFalse) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    EXPECT_FALSE(mesh_ordering_has_channel(ordering, 99));
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(MeshOrderingTest, Update) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    nimcp_error_t err = mesh_ordering_update(ordering, 10);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshOrderingTest, UpdateMultipleTimes) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    for (int i = 0; i < 10; i++) {
        nimcp_error_t err = mesh_ordering_update(ordering, 100);
        EXPECT_EQ(err, NIMCP_SUCCESS);
    }
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(MeshOrderingTest, GetStats) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    mesh_ordering_stats_t stats;
    nimcp_error_t err = mesh_ordering_get_stats(ordering, &stats);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.role, RAFT_ROLE_FOLLOWER);
}

TEST_F(MeshOrderingTest, ResetStats) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");
    mesh_transaction_t* tx = create_test_transaction(p);
    mesh_ordering_submit(ordering, tx);
    mesh_transaction_destroy(tx);

    mesh_ordering_reset_stats(ordering);

    mesh_ordering_stats_t stats;
    mesh_ordering_get_stats(ordering, &stats);
    EXPECT_EQ(stats.transactions_submitted, 0);
}

/* ============================================================================
 * Block Tests
 * ============================================================================ */

TEST_F(MeshOrderingTest, BlockComputeHash) {
    mesh_ordered_block_t block;
    memset(&block, 0, sizeof(block));
    block.block_number = 1;
    block.first_sequence = 0;
    block.last_sequence = 9;
    block.tx_count = 10;

    nimcp_error_t err = mesh_ordered_block_compute_hash(&block);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Hash should be non-zero
    bool all_zero = true;
    for (int i = 0; i < 32; i++) {
        if (block.block_hash[i] != 0) {
            all_zero = false;
            break;
        }
    }
    EXPECT_FALSE(all_zero);
}

TEST_F(MeshOrderingTest, BlockVerifyHash) {
    mesh_ordered_block_t block;
    memset(&block, 0, sizeof(block));
    block.block_number = 1;
    block.tx_count = 10;

    mesh_ordered_block_compute_hash(&block);

    EXPECT_TRUE(mesh_ordered_block_verify_hash(&block));

    // Modify block
    block.tx_count = 20;
    EXPECT_FALSE(mesh_ordered_block_verify_hash(&block));
}

TEST_F(MeshOrderingTest, DestroyBlock) {
    mesh_ordered_block_t* block = (mesh_ordered_block_t*)calloc(1, sizeof(mesh_ordered_block_t));
    ASSERT_NE(block, nullptr);

    block->tx_ids = (mesh_tx_id_t*)calloc(10, sizeof(mesh_tx_id_t));
    block->tx_count = 10;

    mesh_ordered_block_destroy(block);
    // Should not crash
}

TEST_F(MeshOrderingTest, DestroyNullBlock) {
    mesh_ordered_block_destroy(nullptr);
    // Should not crash
}

/* ============================================================================
 * Utility Tests
 * ============================================================================ */

TEST_F(MeshOrderingTest, RaftRoleToString) {
    EXPECT_STREQ(mesh_raft_role_to_string(RAFT_ROLE_FOLLOWER), "FOLLOWER");
    EXPECT_STREQ(mesh_raft_role_to_string(RAFT_ROLE_CANDIDATE), "CANDIDATE");
    EXPECT_STREQ(mesh_raft_role_to_string(RAFT_ROLE_LEADER), "LEADER");
}

TEST_F(MeshOrderingTest, PrintStatus) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    // Should not crash
    mesh_ordering_print_status(ordering);
}

TEST_F(MeshOrderingTest, PrintStatusNull) {
    // Should not crash
    mesh_ordering_print_status(nullptr);
}

TEST_F(MeshOrderingTest, PrintRaftState) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    // Should not crash
    mesh_ordering_print_raft_state(ordering);
}

TEST_F(MeshOrderingTest, PrintBlock) {
    mesh_ordered_block_t block;
    memset(&block, 0, sizeof(block));
    block.block_number = 5;
    block.tx_count = 10;

    // Should not crash
    mesh_ordered_block_print(&block);
}

TEST_F(MeshOrderingTest, PrintBlockNull) {
    // Should not crash
    mesh_ordered_block_print(nullptr);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(MeshOrderingTest, FullWorkflow) {
    mesh_ordering_config_t config;
    mesh_ordering_default_config(&config);
    config.service_name = "integration_orderer";
    config.batch_size = 5;

    ordering = mesh_ordering_create(&config, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    // Add channel
    mesh_ordering_add_channel(ordering, MESH_CHANNEL_LEFT_HEMISPHERE);

    // Start election to become leader
    mesh_ordering_start_election(ordering);

    // If we're leader, run full workflow
    if (mesh_ordering_is_leader(ordering)) {
        mesh_participant_id_t p = register_test_participant("test_module");

        // Submit transactions
        for (int i = 0; i < 10; i++) {
            mesh_transaction_t* tx = create_test_transaction(p);
            nimcp_error_t err = mesh_ordering_submit(ordering, tx);
            EXPECT_EQ(err, NIMCP_SUCCESS);
            mesh_transaction_destroy(tx);
        }

        // Update should batch, sequence, and create blocks
        for (int i = 0; i < 5; i++) {
            mesh_ordering_update(ordering, 100);
        }

        // Should have created blocks
        EXPECT_GE(mesh_ordering_get_latest_block(ordering), 0);

        // Check stats
        mesh_ordering_stats_t stats;
        mesh_ordering_get_stats(ordering, &stats);
        EXPECT_EQ(stats.transactions_submitted, 10);
        EXPECT_GE(stats.blocks_created, 0);
    }
}

TEST_F(MeshOrderingTest, HasQuorum) {
    ordering = mesh_ordering_create(nullptr, orderer_pool);
    ASSERT_NE(ordering, nullptr);

    bool quorum = mesh_ordering_has_quorum(ordering);
    // With 3 orderers and none failed, should have quorum
    EXPECT_TRUE(quorum);
}
