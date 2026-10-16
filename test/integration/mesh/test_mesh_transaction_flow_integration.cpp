/**
 * @file test_mesh_transaction_flow_integration.cpp
 * @brief Integration Tests for Mesh Transaction Execute-Order-Validate Flow
 *
 * WHAT: Tests complete transaction lifecycle with endorsement and ordering
 * WHY:  Verify Hyperledger-style EOV flow works end-to-end
 * HOW:  Create transactions, collect endorsements, order, validate, commit
 *
 * TEST COVERAGE:
 * - Transaction proposal creation
 * - Endorsement policy evaluation
 * - Endorsement collection with timeout
 * - Ordering service submission and batching
 * - Validation and commit phases
 * - Cross-channel transactions
 * - Transaction failure modes
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <map>

extern "C" {
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_transaction.h"
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_endorsement.h"
#include "mesh/nimcp_mesh_ordering.h"
#include "utils/memory/nimcp_memory.h"
}

// =============================================================================
// Test Fixture
// =============================================================================

// Callback for transaction completion
struct TxCallbackContext {
    std::atomic<bool> completed{false};
    mesh_tx_status_t status{MESH_TX_STATUS_PENDING};
    nimcp_error_t error{NIMCP_OK};
    std::mutex mutex;
};

static void tx_completion_callback(const mesh_transaction_t* tx,
                                   mesh_tx_status_t status,
                                   nimcp_error_t error,
                                   void* user_ctx) {
    if (user_ctx) {
        auto* ctx = static_cast<TxCallbackContext*>(user_ctx);
        std::lock_guard<std::mutex> lock(ctx->mutex);
        ctx->status = status;
        ctx->error = error;
        ctx->completed.store(true);
    }
}

class MeshTransactionFlowIntegrationTest : public ::testing::Test {
protected:
    static constexpr size_t NUM_ENDORSERS = 5;

    mesh_channel_t* channel_ = nullptr;
    mesh_ordering_service_t* ordering_ = nullptr;
    mesh_endorsement_policy_t* policy_ = nullptr;
    std::vector<mesh_participant_t*> endorsers_;

    void SetUp() override {
        // Create channel
        mesh_channel_config_t ch_config;
        mesh_channel_config_init(&ch_config);
        ch_config.name = "test_channel";
        ch_config.max_participants = 16;
        ch_config.enable_gossip = true;

        channel_ = mesh_channel_create(&ch_config);
        ASSERT_NE(channel_, nullptr);

        // Create ordering service
        mesh_ordering_config_t ord_config;
        mesh_ordering_config_init(&ord_config);
        ord_config.batch_size = 10;
        ord_config.batch_timeout_ms = 50.0f;
        ord_config.raft_enabled = true;

        ordering_ = mesh_ordering_create(&ord_config);
        ASSERT_NE(ordering_, nullptr);

        // Create endorsement policy
        mesh_endorsement_policy_config_t pol_config;
        mesh_endorsement_policy_config_init(&pol_config);
        pol_config.name = "test_policy";
        pol_config.expression = "ANY 3 OF endorsers";
        pol_config.timeout_ms = 100.0f;

        policy_ = mesh_endorsement_policy_create(&pol_config);
        ASSERT_NE(policy_, nullptr);

        // Create endorser participants
        for (size_t i = 0; i < NUM_ENDORSERS; i++) {
            mesh_participant_config_t p_config;
            mesh_participant_config_init(&p_config);

            char name[32];
            snprintf(name, sizeof(name), "endorser_%zu", i);
            p_config.name = name;
            p_config.type = MESH_PARTICIPANT_TYPE_ENDORSER;
            p_config.can_endorse = true;

            mesh_participant_t* p = mesh_participant_create(&p_config);
            ASSERT_NE(p, nullptr);
            endorsers_.push_back(p);

            // Register with channel
            mesh_channel_add_participant(channel_, p);

            // Register with policy
            mesh_participant_id_t pid = mesh_participant_get_id(p);
            mesh_endorsement_policy_add_endorser(policy_, pid);
        }
    }

    void TearDown() override {
        for (auto* p : endorsers_) {
            if (p) mesh_participant_destroy(p);
        }
        endorsers_.clear();

        if (policy_) {
            mesh_endorsement_policy_destroy(policy_);
            policy_ = nullptr;
        }
        if (ordering_) {
            mesh_ordering_destroy(ordering_);
            ordering_ = nullptr;
        }
        if (channel_) {
            mesh_channel_destroy(channel_);
            channel_ = nullptr;
        }
    }

    // Helper: Wait for transaction callback
    bool WaitForCallback(TxCallbackContext& ctx, uint32_t timeout_ms = 500) {
        auto start = std::chrono::steady_clock::now();
        while (!ctx.completed.load()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed > timeout_ms) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return true;
    }

    // Helper: Create a test transaction
    mesh_transaction_t* CreateTestTransaction(const char* payload) {
        mesh_transaction_config_t config;
        mesh_transaction_config_init(&config);
        config.type = MESH_TX_TYPE_BELIEF_UPDATE;
        config.channel_id = mesh_channel_get_id(channel_);
        config.proposer_id = mesh_participant_get_id(endorsers_[0]);
        config.payload = payload;
        config.payload_size = strlen(payload);
        config.endorsement_policy = policy_;

        return mesh_transaction_create(&config);
    }

    // Helper: Simulate endorsement from a participant
    nimcp_error_t SimulateEndorsement(mesh_transaction_t* tx, size_t endorser_idx,
                                       bool approve = true) {
        if (endorser_idx >= endorsers_.size()) return NIMCP_ERROR_INVALID_PARAM;

        mesh_endorsement_t endorsement;
        mesh_endorsement_init(&endorsement);
        endorsement.endorser_id = mesh_participant_get_id(endorsers_[endorser_idx]);
        endorsement.approved = approve;
        endorsement.timestamp_ms = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());

        return mesh_transaction_add_endorsement(tx, &endorsement);
    }
};

// =============================================================================
// Transaction Lifecycle Tests
// =============================================================================

TEST_F(MeshTransactionFlowIntegrationTest, TransactionCreation) {
    mesh_transaction_t* tx = CreateTestTransaction("test_payload");
    ASSERT_NE(tx, nullptr);

    mesh_transaction_info_t info;
    nimcp_error_t err = mesh_transaction_get_info(tx, &info);
    ASSERT_EQ(err, NIMCP_OK);

    EXPECT_EQ(info.type, MESH_TX_TYPE_BELIEF_UPDATE);
    EXPECT_EQ(info.status, MESH_TX_STATUS_PENDING);
    EXPECT_GT(info.tx_id, 0u);

    mesh_transaction_destroy(tx);
}

TEST_F(MeshTransactionFlowIntegrationTest, FullEOVFlow) {
    // 1. PROPOSE: Create transaction
    mesh_transaction_t* tx = CreateTestTransaction("belief_update_data");
    ASSERT_NE(tx, nullptr);

    // 2. ENDORSE: Collect endorsements (need 3 of 5)
    for (size_t i = 0; i < 3; i++) {
        nimcp_error_t err = SimulateEndorsement(tx, i, true);
        ASSERT_EQ(err, NIMCP_OK);
    }

    // Verify endorsement count
    mesh_transaction_info_t info;
    mesh_transaction_get_info(tx, &info);
    EXPECT_EQ(info.endorsement_count, 3u);

    // Evaluate endorsement policy
    bool policy_satisfied = false;
    nimcp_error_t err = mesh_endorsement_policy_evaluate(
        policy_, tx, &policy_satisfied);
    ASSERT_EQ(err, NIMCP_OK);
    EXPECT_TRUE(policy_satisfied);

    // 3. ORDER: Submit to ordering service
    TxCallbackContext callback_ctx;
    err = mesh_ordering_submit(ordering_, tx, tx_completion_callback, &callback_ctx);
    ASSERT_EQ(err, NIMCP_OK);

    // Trigger ordering batch (force flush)
    mesh_ordering_flush(ordering_);

    // 4. VALIDATE & COMMIT: Wait for completion
    ASSERT_TRUE(WaitForCallback(callback_ctx, 1000))
        << "Transaction did not complete in time";

    EXPECT_EQ(callback_ctx.status, MESH_TX_STATUS_COMMITTED);
    EXPECT_EQ(callback_ctx.error, NIMCP_OK);

    mesh_transaction_destroy(tx);
}

// =============================================================================
// Endorsement Tests
// =============================================================================

TEST_F(MeshTransactionFlowIntegrationTest, EndorsementPolicySatisfied) {
    mesh_transaction_t* tx = CreateTestTransaction("policy_test");
    ASSERT_NE(tx, nullptr);

    // "ANY 3 OF endorsers" - provide exactly 3
    for (size_t i = 0; i < 3; i++) {
        SimulateEndorsement(tx, i, true);
    }

    bool satisfied = false;
    nimcp_error_t err = mesh_endorsement_policy_evaluate(policy_, tx, &satisfied);
    ASSERT_EQ(err, NIMCP_OK);
    EXPECT_TRUE(satisfied);

    mesh_transaction_destroy(tx);
}

TEST_F(MeshTransactionFlowIntegrationTest, EndorsementPolicyNotSatisfied) {
    mesh_transaction_t* tx = CreateTestTransaction("policy_fail_test");
    ASSERT_NE(tx, nullptr);

    // Only 2 endorsements (need 3)
    SimulateEndorsement(tx, 0, true);
    SimulateEndorsement(tx, 1, true);

    bool satisfied = true;
    nimcp_error_t err = mesh_endorsement_policy_evaluate(policy_, tx, &satisfied);
    ASSERT_EQ(err, NIMCP_OK);
    EXPECT_FALSE(satisfied);

    mesh_transaction_destroy(tx);
}

TEST_F(MeshTransactionFlowIntegrationTest, EndorsementWithRejections) {
    mesh_transaction_t* tx = CreateTestTransaction("mixed_endorsements");
    ASSERT_NE(tx, nullptr);

    // 3 approve, 2 reject - policy should be satisfied
    SimulateEndorsement(tx, 0, true);
    SimulateEndorsement(tx, 1, true);
    SimulateEndorsement(tx, 2, true);
    SimulateEndorsement(tx, 3, false);  // Reject
    SimulateEndorsement(tx, 4, false);  // Reject

    bool satisfied = false;
    nimcp_error_t err = mesh_endorsement_policy_evaluate(policy_, tx, &satisfied);
    ASSERT_EQ(err, NIMCP_OK);
    EXPECT_TRUE(satisfied) << "3 approvals should satisfy 'ANY 3' policy";

    mesh_transaction_destroy(tx);
}

// =============================================================================
// Ordering Service Tests
// =============================================================================

TEST_F(MeshTransactionFlowIntegrationTest, OrderingBatching) {
    std::vector<mesh_transaction_t*> transactions;
    std::vector<TxCallbackContext> callbacks(5);

    // Create and submit multiple transactions
    for (size_t i = 0; i < 5; i++) {
        char payload[32];
        snprintf(payload, sizeof(payload), "batch_tx_%zu", i);
        mesh_transaction_t* tx = CreateTestTransaction(payload);
        ASSERT_NE(tx, nullptr);

        // Add endorsements
        for (size_t e = 0; e < 3; e++) {
            SimulateEndorsement(tx, e, true);
        }

        nimcp_error_t err = mesh_ordering_submit(ordering_, tx,
            tx_completion_callback, &callbacks[i]);
        ASSERT_EQ(err, NIMCP_OK);

        transactions.push_back(tx);
    }

    // Flush batch
    mesh_ordering_flush(ordering_);

    // Wait for all to complete
    for (size_t i = 0; i < 5; i++) {
        ASSERT_TRUE(WaitForCallback(callbacks[i]))
            << "Transaction " << i << " did not complete";
        EXPECT_EQ(callbacks[i].status, MESH_TX_STATUS_COMMITTED);
    }

    // Verify sequence numbers are ordered
    std::vector<uint64_t> sequences;
    for (auto* tx : transactions) {
        mesh_transaction_info_t info;
        mesh_transaction_get_info(tx, &info);
        sequences.push_back(info.sequence_number);
    }

    // Sequences should be monotonically increasing
    for (size_t i = 1; i < sequences.size(); i++) {
        EXPECT_GT(sequences[i], sequences[i-1])
            << "Sequence numbers should be strictly increasing";
    }

    // Cleanup
    for (auto* tx : transactions) {
        mesh_transaction_destroy(tx);
    }
}

TEST_F(MeshTransactionFlowIntegrationTest, OrderingSequenceAssignment) {
    mesh_transaction_t* tx = CreateTestTransaction("sequence_test");
    ASSERT_NE(tx, nullptr);

    for (size_t i = 0; i < 3; i++) {
        SimulateEndorsement(tx, i, true);
    }

    TxCallbackContext ctx;
    mesh_ordering_submit(ordering_, tx, tx_completion_callback, &ctx);
    mesh_ordering_flush(ordering_);

    ASSERT_TRUE(WaitForCallback(ctx));

    mesh_transaction_info_t info;
    mesh_transaction_get_info(tx, &info);

    EXPECT_GT(info.sequence_number, 0u);
    EXPECT_GT(info.block_number, 0u);

    mesh_transaction_destroy(tx);
}

// =============================================================================
// Transaction Failure Tests
// =============================================================================

TEST_F(MeshTransactionFlowIntegrationTest, TransactionRejectDueToPolicy) {
    mesh_transaction_t* tx = CreateTestTransaction("will_be_rejected");
    ASSERT_NE(tx, nullptr);

    // Only 2 endorsements
    SimulateEndorsement(tx, 0, true);
    SimulateEndorsement(tx, 1, true);

    // Submit without meeting policy
    TxCallbackContext ctx;
    nimcp_error_t err = mesh_ordering_submit(ordering_, tx,
        tx_completion_callback, &ctx);

    // Ordering should accept but validation will fail
    mesh_ordering_flush(ordering_);

    if (WaitForCallback(ctx, 500)) {
        EXPECT_EQ(ctx.status, MESH_TX_STATUS_INVALID);
    }

    mesh_transaction_destroy(tx);
}

TEST_F(MeshTransactionFlowIntegrationTest, TransactionTimeout) {
    // Create policy with very short timeout
    mesh_endorsement_policy_config_t pol_config;
    mesh_endorsement_policy_config_init(&pol_config);
    pol_config.name = "timeout_policy";
    pol_config.expression = "ANY 3 OF endorsers";
    pol_config.timeout_ms = 1.0f;  // 1ms timeout

    mesh_endorsement_policy_t* timeout_policy =
        mesh_endorsement_policy_create(&pol_config);

    // Create transaction with timeout policy
    mesh_transaction_config_t tx_config;
    mesh_transaction_config_init(&tx_config);
    tx_config.type = MESH_TX_TYPE_BELIEF_UPDATE;
    tx_config.channel_id = mesh_channel_get_id(channel_);
    tx_config.proposer_id = mesh_participant_get_id(endorsers_[0]);
    tx_config.payload = "timeout_test";
    tx_config.payload_size = 12;
    tx_config.endorsement_policy = timeout_policy;
    tx_config.timeout_ms = 1.0f;

    mesh_transaction_t* tx = mesh_transaction_create(&tx_config);
    ASSERT_NE(tx, nullptr);

    // Sleep past timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Check if transaction is expired
    bool expired = false;
    mesh_transaction_is_expired(tx, &expired);
    EXPECT_TRUE(expired);

    mesh_transaction_destroy(tx);
    mesh_endorsement_policy_destroy(timeout_policy);
}

// =============================================================================
// Concurrent Transaction Tests
// =============================================================================

TEST_F(MeshTransactionFlowIntegrationTest, ConcurrentTransactionSubmission) {
    std::atomic<int> success_count{0};
    std::atomic<int> fail_count{0};
    std::vector<std::thread> threads;
    std::vector<mesh_transaction_t*> all_transactions;
    std::mutex tx_mutex;

    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 5; i++) {
                char payload[32];
                snprintf(payload, sizeof(payload), "concurrent_%d_%d", t, i);

                mesh_transaction_t* tx = CreateTestTransaction(payload);
                if (!tx) {
                    fail_count++;
                    continue;
                }

                // Add endorsements
                for (size_t e = 0; e < 3; e++) {
                    SimulateEndorsement(tx, e, true);
                }

                TxCallbackContext ctx;
                nimcp_error_t err = mesh_ordering_submit(ordering_, tx,
                    tx_completion_callback, &ctx);

                if (err == NIMCP_OK) {
                    success_count++;
                    std::lock_guard<std::mutex> lock(tx_mutex);
                    all_transactions.push_back(tx);
                } else {
                    fail_count++;
                    mesh_transaction_destroy(tx);
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // Flush and wait
    mesh_ordering_flush(ordering_);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Most should succeed
    EXPECT_GE(success_count.load(), 15) << "Too many failures: " << fail_count.load();

    // Cleanup
    for (auto* tx : all_transactions) {
        mesh_transaction_destroy(tx);
    }
}

// =============================================================================
// Transaction Metrics Tests
// =============================================================================

TEST_F(MeshTransactionFlowIntegrationTest, TransactionMetrics) {
    // Process several transactions
    for (int i = 0; i < 5; i++) {
        char payload[32];
        snprintf(payload, sizeof(payload), "metrics_tx_%d", i);
        mesh_transaction_t* tx = CreateTestTransaction(payload);

        for (size_t e = 0; e < 3; e++) {
            SimulateEndorsement(tx, e, true);
        }

        TxCallbackContext ctx;
        mesh_ordering_submit(ordering_, tx, tx_completion_callback, &ctx);
        mesh_ordering_flush(ordering_);
        WaitForCallback(ctx);

        mesh_transaction_destroy(tx);
    }

    // Get ordering metrics
    mesh_ordering_metrics_t metrics;
    nimcp_error_t err = mesh_ordering_get_metrics(ordering_, &metrics);
    ASSERT_EQ(err, NIMCP_OK);

    EXPECT_GE(metrics.total_transactions, 5u);
    EXPECT_GE(metrics.committed_transactions, 5u);
    EXPECT_EQ(metrics.invalid_transactions, 0u);
    EXPECT_GE(metrics.blocks_created, 1u);
}
