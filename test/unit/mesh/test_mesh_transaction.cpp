/**
 * @file test_mesh_transaction.cpp
 * @brief Unit tests for mesh transaction manager
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "mesh/nimcp_mesh_transaction.h"
#include "mesh/nimcp_mesh_participant.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MeshTransactionTest : public ::testing::Test {
protected:
    mesh_tx_manager_t* manager = nullptr;
    mesh_participant_registry_t* registry = nullptr;

    void SetUp() override {
        /* Create registry */
        mesh_registry_config_t reg_config;
        mesh_registry_default_config(&reg_config);
        reg_config.enable_logging = false;
        registry = mesh_registry_create(&reg_config);

        /* Create transaction manager */
        mesh_tx_manager_config_t config;
        mesh_tx_manager_default_config(&config);
        config.enable_logging = false;
        manager = mesh_tx_manager_create(&config, registry);
    }

    void TearDown() override {
        mesh_tx_manager_destroy(manager);
        mesh_registry_destroy(registry);
        manager = nullptr;
        registry = nullptr;
    }
};

/* ============================================================================
 * Manager Lifecycle Tests
 * ============================================================================ */

TEST_F(MeshTransactionTest, CreateManager) {
    EXPECT_NE(manager, nullptr);
}

TEST(MeshTransactionBasicTest, CreateManagerWithDefaults) {
    mesh_tx_manager_t* mgr = mesh_tx_manager_create(NULL, NULL);
    EXPECT_NE(mgr, nullptr);
    mesh_tx_manager_destroy(mgr);
}

TEST(MeshTransactionBasicTest, DestroyNullManager) {
    /* Should not crash */
    mesh_tx_manager_destroy(NULL);
}

TEST(MeshTransactionBasicTest, DefaultConfig) {
    mesh_tx_manager_config_t config;
    nimcp_error_t err = mesh_tx_manager_default_config(&config);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(config.max_pending, 0);
    EXPECT_GT(config.max_batch_size, 0);
    EXPECT_GT(config.default_timeout_ms, 0);
}

TEST(MeshTransactionBasicTest, DefaultConfigNullPtr) {
    nimcp_error_t err = mesh_tx_manager_default_config(NULL);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Transaction Creation Tests
 * ============================================================================ */

TEST(MeshTransactionBasicTest, CreateTransaction) {
    mesh_participant_id_t proposer = mesh_make_participant_id(
        MESH_CHANNEL_LEFT_HEMISPHERE,
        MESH_PARTICIPANT_MODULE,
        42
    );

    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_BELIEF_UPDATE,
        proposer,
        MESH_CHANNEL_LEFT_HEMISPHERE
    );

    EXPECT_NE(tx, nullptr);
    EXPECT_EQ(tx->type, MESH_TX_BELIEF_UPDATE);
    EXPECT_EQ(tx->proposer_id, proposer);
    EXPECT_EQ(tx->source_channel, MESH_CHANNEL_LEFT_HEMISPHERE);
    EXPECT_EQ(tx->status, MESH_TX_STATUS_NONE);

    mesh_transaction_destroy(tx);
}

TEST(MeshTransactionBasicTest, DestroyNullTransaction) {
    /* Should not crash */
    mesh_transaction_destroy(NULL);
}

TEST(MeshTransactionBasicTest, SetPayload) {
    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_STATE_CHANGE,
        0x1234,
        MESH_CHANNEL_SYSTEM
    );

    const char* data = "test payload data";
    nimcp_error_t err = mesh_transaction_set_payload(tx, data, strlen(data));

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(tx->payload_size, strlen(data));
    EXPECT_EQ(memcmp(tx->payload, data, strlen(data)), 0);

    mesh_transaction_destroy(tx);
}

TEST(MeshTransactionBasicTest, SetPayloadTooLarge) {
    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_STATE_CHANGE,
        0x1234,
        MESH_CHANNEL_SYSTEM
    );

    uint8_t* large_data = (uint8_t*)malloc(MESH_MAX_PAYLOAD_SIZE + 100);
    nimcp_error_t err = mesh_transaction_set_payload(tx, large_data, MESH_MAX_PAYLOAD_SIZE + 100);

    EXPECT_EQ(err, NIMCP_ERROR_BUFFER_OVERFLOW);

    free(large_data);
    mesh_transaction_destroy(tx);
}

TEST(MeshTransactionBasicTest, SetPolicy) {
    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_CONSENSUS_VOTE,
        0x1234,
        MESH_CHANNEL_SYSTEM
    );

    nimcp_error_t err = mesh_transaction_set_policy(tx, "cognitive_decision");

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_STREQ(tx->endorsement_policy, "cognitive_decision");

    mesh_transaction_destroy(tx);
}

TEST(MeshTransactionBasicTest, SetTimeout) {
    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_STATE_CHANGE,
        0x1234,
        MESH_CHANNEL_SYSTEM
    );

    nimcp_error_t err = mesh_transaction_set_timeout(tx, 1000);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(tx->timeout_ns, tx->created_ns);

    mesh_transaction_destroy(tx);
}

/* ============================================================================
 * Transaction Proposal Tests
 * ============================================================================ */

TEST_F(MeshTransactionTest, ProposeTransaction) {
    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_BELIEF_UPDATE,
        0x1234,
        MESH_CHANNEL_LEFT_HEMISPHERE
    );

    nimcp_error_t err = mesh_tx_propose(manager, tx);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(tx->status, MESH_TX_STATUS_PROPOSED);
}

TEST_F(MeshTransactionTest, ProposeNullTransaction) {
    nimcp_error_t err = mesh_tx_propose(manager, NULL);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshTransactionTest, ProposeNullManager) {
    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_BELIEF_UPDATE,
        0x1234,
        MESH_CHANNEL_SYSTEM
    );

    nimcp_error_t err = mesh_tx_propose(NULL, tx);
    EXPECT_EQ(err, NIMCP_ERROR_INVALID_PARAM);

    mesh_transaction_destroy(tx);
}

/* ============================================================================
 * Endorsement Tests
 * ============================================================================ */

TEST_F(MeshTransactionTest, AddEndorsement) {
    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_BELIEF_UPDATE,
        0x1234,
        MESH_CHANNEL_LEFT_HEMISPHERE
    );

    EXPECT_EQ(mesh_tx_propose(manager, tx), NIMCP_SUCCESS);

    mesh_endorsement_t endorsement = {0};
    endorsement.endorser_id = 0x5678;
    endorsement.result = ENDORSEMENT_APPROVED;
    endorsement.timestamp_ns = 1000000;

    nimcp_error_t err = mesh_tx_add_endorsement(manager, &tx->id, &endorsement);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(tx->endorsements.count, 1);
}

TEST_F(MeshTransactionTest, MultipleEndorsements) {
    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_CONSENSUS_VOTE,
        0x1234,
        MESH_CHANNEL_SYSTEM
    );

    EXPECT_EQ(mesh_tx_propose(manager, tx), NIMCP_SUCCESS);

    /* Add multiple endorsements */
    for (int i = 0; i < 5; i++) {
        mesh_endorsement_t endorsement = {0};
        endorsement.endorser_id = 0x1000 + i;
        endorsement.result = ENDORSEMENT_APPROVED;

        EXPECT_EQ(mesh_tx_add_endorsement(manager, &tx->id, &endorsement), NIMCP_SUCCESS);
    }

    EXPECT_GE(tx->endorsements.count, 5);
    EXPECT_TRUE(tx->endorsements.policy_satisfied);
}

TEST_F(MeshTransactionTest, CheckEndorsed) {
    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_BELIEF_UPDATE,
        0x1234,
        MESH_CHANNEL_SYSTEM
    );

    EXPECT_EQ(mesh_tx_propose(manager, tx), NIMCP_SUCCESS);
    EXPECT_FALSE(mesh_tx_is_endorsed(manager, &tx->id));

    mesh_endorsement_t endorsement = {0};
    endorsement.endorser_id = 0x5678;
    endorsement.result = ENDORSEMENT_APPROVED;

    EXPECT_EQ(mesh_tx_add_endorsement(manager, &tx->id, &endorsement), NIMCP_SUCCESS);
    EXPECT_TRUE(mesh_tx_is_endorsed(manager, &tx->id));
}

/* ============================================================================
 * Ordering Tests
 * ============================================================================ */

TEST_F(MeshTransactionTest, MarkOrdered) {
    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_STATE_CHANGE,
        0x1234,
        MESH_CHANNEL_SYSTEM
    );

    EXPECT_EQ(mesh_tx_propose(manager, tx), NIMCP_SUCCESS);

    /* Add endorsement to satisfy policy */
    mesh_endorsement_t endorsement = {0};
    endorsement.endorser_id = 0x5678;
    endorsement.result = ENDORSEMENT_APPROVED;
    EXPECT_EQ(mesh_tx_add_endorsement(manager, &tx->id, &endorsement), NIMCP_SUCCESS);

    /* Submit for ordering */
    EXPECT_EQ(mesh_tx_submit_for_ordering(manager, &tx->id), NIMCP_SUCCESS);
    EXPECT_EQ(tx->status, MESH_TX_STATUS_ORDERING);

    /* Mark as ordered */
    EXPECT_EQ(mesh_tx_mark_ordered(manager, &tx->id, 100, NULL), NIMCP_SUCCESS);
    EXPECT_EQ(tx->status, MESH_TX_STATUS_ORDERED);
    EXPECT_EQ(tx->sequence_number, 100);
}

TEST_F(MeshTransactionTest, SubmitNotEndorsed) {
    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_STATE_CHANGE,
        0x1234,
        MESH_CHANNEL_SYSTEM
    );

    EXPECT_EQ(mesh_tx_propose(manager, tx), NIMCP_SUCCESS);

    /* Should fail - not endorsed */
    EXPECT_EQ(mesh_tx_submit_for_ordering(manager, &tx->id), NIMCP_ERROR_PERMISSION_DENIED);
}

/* ============================================================================
 * Commit Tests
 * ============================================================================ */

TEST_F(MeshTransactionTest, CommitTransaction) {
    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_STATE_CHANGE,
        0x1234,
        MESH_CHANNEL_SYSTEM
    );

    EXPECT_EQ(mesh_tx_propose(manager, tx), NIMCP_SUCCESS);

    /* Endorse */
    mesh_endorsement_t endorsement = {0};
    endorsement.endorser_id = 0x5678;
    endorsement.result = ENDORSEMENT_APPROVED;
    EXPECT_EQ(mesh_tx_add_endorsement(manager, &tx->id, &endorsement), NIMCP_SUCCESS);

    /* Order */
    EXPECT_EQ(mesh_tx_submit_for_ordering(manager, &tx->id), NIMCP_SUCCESS);
    EXPECT_EQ(mesh_tx_mark_ordered(manager, &tx->id, 100, NULL), NIMCP_SUCCESS);

    /* Commit */
    EXPECT_EQ(mesh_tx_commit(manager, &tx->id), NIMCP_SUCCESS);
    EXPECT_EQ(tx->status, MESH_TX_STATUS_COMMITTED);
    EXPECT_GT(tx->commit_timestamp_ns, 0);
}

TEST_F(MeshTransactionTest, FailTransaction) {
    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_STATE_CHANGE,
        0x1234,
        MESH_CHANNEL_SYSTEM
    );

    EXPECT_EQ(mesh_tx_propose(manager, tx), NIMCP_SUCCESS);

    EXPECT_EQ(mesh_tx_fail(manager, &tx->id, NIMCP_ERROR_OPERATION_FAILED, "test failure"),
              NIMCP_SUCCESS);
    EXPECT_EQ(tx->status, MESH_TX_STATUS_FAILED);
}

/* ============================================================================
 * Query Tests
 * ============================================================================ */

TEST_F(MeshTransactionTest, GetTransaction) {
    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_BELIEF_UPDATE,
        0x1234,
        MESH_CHANNEL_LEFT_HEMISPHERE
    );

    EXPECT_EQ(mesh_tx_propose(manager, tx), NIMCP_SUCCESS);

    const mesh_transaction_t* found = mesh_tx_get(manager, &tx->id);
    EXPECT_EQ(found, tx);
}

TEST_F(MeshTransactionTest, GetStatus) {
    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_BELIEF_UPDATE,
        0x1234,
        MESH_CHANNEL_SYSTEM
    );

    /* Not yet proposed */
    mesh_tx_id_t fake_id = {0};
    EXPECT_EQ(mesh_tx_get_status(manager, &fake_id), MESH_TX_STATUS_NONE);

    /* Proposed */
    EXPECT_EQ(mesh_tx_propose(manager, tx), NIMCP_SUCCESS);
    EXPECT_EQ(mesh_tx_get_status(manager, &tx->id), MESH_TX_STATUS_PROPOSED);
}

TEST_F(MeshTransactionTest, IsComplete) {
    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_STATE_CHANGE,
        0x1234,
        MESH_CHANNEL_SYSTEM
    );

    EXPECT_EQ(mesh_tx_propose(manager, tx), NIMCP_SUCCESS);
    EXPECT_FALSE(mesh_tx_is_complete(manager, &tx->id));

    /* Fail it */
    EXPECT_EQ(mesh_tx_fail(manager, &tx->id, NIMCP_ERROR_OPERATION_FAILED, "test"),
              NIMCP_SUCCESS);
    EXPECT_TRUE(mesh_tx_is_complete(manager, &tx->id));
}

TEST_F(MeshTransactionTest, GetResult) {
    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_STATE_CHANGE,
        0x1234,
        MESH_CHANNEL_SYSTEM
    );

    EXPECT_EQ(mesh_tx_propose(manager, tx), NIMCP_SUCCESS);

    mesh_result_t result;
    EXPECT_EQ(mesh_tx_get_result(manager, &tx->id, &result), NIMCP_SUCCESS);
    EXPECT_EQ(result.status, MESH_TX_STATUS_PROPOSED);
}

/* ============================================================================
 * Batch Tests
 * ============================================================================ */

TEST(MeshTransactionBatchTest, CreateBatch) {
    mesh_tx_batch_t* batch = mesh_tx_batch_create(MESH_CHANNEL_SYSTEM, 10);
    EXPECT_NE(batch, nullptr);
    EXPECT_EQ(batch->count, 0);
    EXPECT_EQ(batch->capacity, 10);
    EXPECT_EQ(batch->channel, MESH_CHANNEL_SYSTEM);
    mesh_tx_batch_destroy(batch);
}

TEST(MeshTransactionBatchTest, DestroyNullBatch) {
    /* Should not crash */
    mesh_tx_batch_destroy(NULL);
}

TEST(MeshTransactionBatchTest, AddToBatch) {
    mesh_tx_batch_t* batch = mesh_tx_batch_create(MESH_CHANNEL_SYSTEM, 10);

    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_BELIEF_UPDATE,
        0x1234,
        MESH_CHANNEL_SYSTEM
    );

    EXPECT_EQ(mesh_tx_batch_add(batch, tx), NIMCP_SUCCESS);
    EXPECT_EQ(batch->count, 1);

    mesh_tx_batch_destroy(batch);
    mesh_transaction_destroy(tx);
}

TEST(MeshTransactionBatchTest, BatchFull) {
    mesh_tx_batch_t* batch = mesh_tx_batch_create(MESH_CHANNEL_SYSTEM, 2);

    mesh_transaction_t* tx1 = mesh_transaction_create(MESH_TX_BELIEF_UPDATE, 0x1, MESH_CHANNEL_SYSTEM);
    mesh_transaction_t* tx2 = mesh_transaction_create(MESH_TX_BELIEF_UPDATE, 0x2, MESH_CHANNEL_SYSTEM);
    mesh_transaction_t* tx3 = mesh_transaction_create(MESH_TX_BELIEF_UPDATE, 0x3, MESH_CHANNEL_SYSTEM);

    EXPECT_EQ(mesh_tx_batch_add(batch, tx1), NIMCP_SUCCESS);
    EXPECT_EQ(mesh_tx_batch_add(batch, tx2), NIMCP_SUCCESS);
    EXPECT_EQ(mesh_tx_batch_add(batch, tx3), NIMCP_ERROR_NO_MEMORY);

    mesh_tx_batch_destroy(batch);
    mesh_transaction_destroy(tx1);
    mesh_transaction_destroy(tx2);
    mesh_transaction_destroy(tx3);
}

TEST(MeshTransactionBatchTest, ClearBatch) {
    mesh_tx_batch_t* batch = mesh_tx_batch_create(MESH_CHANNEL_SYSTEM, 10);

    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_BELIEF_UPDATE,
        0x1234,
        MESH_CHANNEL_SYSTEM
    );

    EXPECT_EQ(mesh_tx_batch_add(batch, tx), NIMCP_SUCCESS);
    EXPECT_EQ(batch->count, 1);

    mesh_tx_batch_clear(batch);
    EXPECT_EQ(batch->count, 0);

    mesh_tx_batch_destroy(batch);
    mesh_transaction_destroy(tx);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(MeshTransactionTest, GetStatistics) {
    /* Propose some transactions */
    for (int i = 0; i < 5; i++) {
        mesh_transaction_t* tx = mesh_transaction_create(
            MESH_TX_BELIEF_UPDATE,
            0x1000 + i,
            MESH_CHANNEL_SYSTEM
        );
        mesh_tx_propose(manager, tx);
    }

    mesh_tx_manager_stats_t stats;
    EXPECT_EQ(mesh_tx_manager_get_stats(manager, &stats), NIMCP_SUCCESS);

    EXPECT_EQ(stats.transactions_proposed, 5);
    EXPECT_EQ(stats.pending_count, 5);
}

TEST_F(MeshTransactionTest, ResetStatistics) {
    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_BELIEF_UPDATE,
        0x1234,
        MESH_CHANNEL_SYSTEM
    );
    mesh_tx_propose(manager, tx);

    mesh_tx_manager_reset_stats(manager);

    mesh_tx_manager_stats_t stats;
    EXPECT_EQ(mesh_tx_manager_get_stats(manager, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.transactions_proposed, 0);
}

/* ============================================================================
 * Callback Tests
 * ============================================================================ */

static bool callback_invoked = false;
static mesh_tx_status_t callback_status = MESH_TX_STATUS_NONE;

static void test_callback(const mesh_result_t* result, void* ctx) {
    (void)ctx;
    callback_invoked = true;
    callback_status = result->status;
}

TEST_F(MeshTransactionTest, CallbackOnCommit) {
    callback_invoked = false;
    callback_status = MESH_TX_STATUS_NONE;

    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_STATE_CHANGE,
        0x1234,
        MESH_CHANNEL_SYSTEM
    );

    mesh_transaction_set_callback(tx, test_callback, NULL);

    EXPECT_EQ(mesh_tx_propose(manager, tx), NIMCP_SUCCESS);

    /* Endorse */
    mesh_endorsement_t endorsement = {0};
    endorsement.endorser_id = 0x5678;
    endorsement.result = ENDORSEMENT_APPROVED;
    EXPECT_EQ(mesh_tx_add_endorsement(manager, &tx->id, &endorsement), NIMCP_SUCCESS);

    /* Order and commit */
    EXPECT_EQ(mesh_tx_submit_for_ordering(manager, &tx->id), NIMCP_SUCCESS);
    EXPECT_EQ(mesh_tx_mark_ordered(manager, &tx->id, 100, NULL), NIMCP_SUCCESS);
    EXPECT_EQ(mesh_tx_commit(manager, &tx->id), NIMCP_SUCCESS);

    EXPECT_TRUE(callback_invoked);
    EXPECT_EQ(callback_status, MESH_TX_STATUS_COMMITTED);
}

TEST_F(MeshTransactionTest, CallbackOnFail) {
    callback_invoked = false;
    callback_status = MESH_TX_STATUS_NONE;

    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_STATE_CHANGE,
        0x1234,
        MESH_CHANNEL_SYSTEM
    );

    mesh_transaction_set_callback(tx, test_callback, NULL);

    EXPECT_EQ(mesh_tx_propose(manager, tx), NIMCP_SUCCESS);
    EXPECT_EQ(mesh_tx_fail(manager, &tx->id, NIMCP_ERROR_OPERATION_FAILED, "test failure"),
              NIMCP_SUCCESS);

    EXPECT_TRUE(callback_invoked);
    EXPECT_EQ(callback_status, MESH_TX_STATUS_FAILED);
}

/* ============================================================================
 * Utility Tests
 * ============================================================================ */

TEST(MeshTransactionUtilTest, GenerateId) {
    mesh_tx_id_t id;
    nimcp_error_t err = mesh_tx_generate_id(0x1234, MESH_CHANNEL_LEFT_HEMISPHERE, &id);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(id.channel, MESH_CHANNEL_LEFT_HEMISPHERE);
    EXPECT_EQ(id.proposer, 0x1234);
    EXPECT_GT(id.timestamp_ns, 0);
}

TEST(MeshTransactionUtilTest, ComputeHash) {
    const char* data = "test data for hashing";
    uint8_t hash[32];

    nimcp_error_t err = mesh_tx_compute_hash(data, strlen(data), hash);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Hash should be non-zero */
    bool all_zero = true;
    for (int i = 0; i < 32; i++) {
        if (hash[i] != 0) {
            all_zero = false;
            break;
        }
    }
    EXPECT_FALSE(all_zero);
}

TEST(MeshTransactionUtilTest, ComputeHashEmpty) {
    uint8_t hash[32];
    nimcp_error_t err = mesh_tx_compute_hash(NULL, 0, hash);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Empty input should produce zero hash */
    for (int i = 0; i < 32; i++) {
        EXPECT_EQ(hash[i], 0);
    }
}
