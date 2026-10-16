/**
 * @file test_hyperledger_bridge.cpp
 * @brief Unit tests for Hyperledger-inspired training/inference integration bridge
 *
 * Tests three integration points:
 * 1. Execute-Order-Validate (EOV) training pipeline
 * 2. Consensus-gated inference
 * 3. Auditable weight ledger with hash chain
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "core/brain/bridges/nimcp_hyperledger_bridge.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class HyperledgerBridgeTest : public ::testing::Test {
protected:
    hyperledger_bridge_t* bridge = nullptr;

    void SetUp() override {
        hyperledger_bridge_config_t config = hyperledger_bridge_default_config();
        bridge = hyperledger_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            hyperledger_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST(HyperledgerConfig, DefaultConfig) {
    hyperledger_bridge_config_t config = hyperledger_bridge_default_config();
    EXPECT_TRUE(config.enable_eov_pipeline);
    EXPECT_TRUE(config.enable_audit_log);
    EXPECT_TRUE(config.enable_hash_chain);
    EXPECT_TRUE(config.enable_snapshots);
    EXPECT_FALSE(config.enable_consensus_gate);
    EXPECT_FALSE(config.enable_auto_rollback);
    EXPECT_FLOAT_EQ(config.anomaly_reject_threshold, 0.8f);
    EXPECT_FLOAT_EQ(config.consensus_threshold, 0.666667f);
    EXPECT_EQ(config.validation_interval, 10u);
    EXPECT_EQ(config.max_snapshots, HYPERLEDGER_MAX_ROLLBACK_DEPTH);
}

TEST(HyperledgerConfig, CreateWithNullConfig) {
    hyperledger_bridge_t* b = hyperledger_bridge_create(nullptr);
    EXPECT_EQ(b, nullptr);
}

TEST(HyperledgerConfig, DestroyNull) {
    hyperledger_bridge_destroy(nullptr);  /* Should not crash */
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(HyperledgerBridgeTest, CreateAndDestroy) {
    /* bridge created in SetUp, destroyed in TearDown */
    ASSERT_NE(bridge, nullptr);
}

TEST_F(HyperledgerBridgeTest, CreateWithAuditDisabled) {
    hyperledger_bridge_config_t config = hyperledger_bridge_default_config();
    config.enable_audit_log = false;
    hyperledger_bridge_t* b = hyperledger_bridge_create(&config);
    ASSERT_NE(b, nullptr);
    /* Should still work, just no audit logging */
    uint64_t tx = hyperledger_eov_begin(b, 0.5f);
    EXPECT_GT(tx, 0u);
    hyperledger_bridge_destroy(b);
}

//=============================================================================
// Connection Tests
//=============================================================================

TEST_F(HyperledgerBridgeTest, ConnectSecurityNull) {
    EXPECT_EQ(hyperledger_bridge_connect_security(bridge, nullptr), 0);
}

TEST_F(HyperledgerBridgeTest, ConnectGradientManagerNull) {
    EXPECT_EQ(hyperledger_bridge_connect_gradient_manager(bridge, nullptr), 0);
}

TEST_F(HyperledgerBridgeTest, ConnectConsensusNull) {
    EXPECT_EQ(hyperledger_bridge_connect_consensus(bridge, nullptr), 0);
}

TEST_F(HyperledgerBridgeTest, ConnectCollectiveNull) {
    EXPECT_EQ(hyperledger_bridge_connect_collective(bridge, nullptr), 0);
}

TEST_F(HyperledgerBridgeTest, ConnectNullBridge) {
    EXPECT_EQ(hyperledger_bridge_connect_security(nullptr, nullptr), -1);
    EXPECT_EQ(hyperledger_bridge_connect_gradient_manager(nullptr, nullptr), -1);
    EXPECT_EQ(hyperledger_bridge_connect_consensus(nullptr, nullptr), -1);
    EXPECT_EQ(hyperledger_bridge_connect_collective(nullptr, nullptr), -1);
}

//=============================================================================
// EOV Pipeline Tests (Integration Point #1)
//=============================================================================

TEST_F(HyperledgerBridgeTest, EOVBasicFlow) {
    /* Begin transaction */
    uint64_t tx = hyperledger_eov_begin(bridge, 0.5f);
    EXPECT_GT(tx, 0u);

    /* Order phase */
    EXPECT_EQ(hyperledger_eov_order(bridge, tx, 1.5f, 100), 0);

    /* Validate phase (no gradients, should pass) */
    EXPECT_TRUE(hyperledger_eov_validate(bridge, tx, nullptr, 0));

    /* Commit phase */
    EXPECT_EQ(hyperledger_eov_commit(bridge, tx, 0.01f, 0.45f), 0);

    /* Check stats */
    hyperledger_bridge_stats_t stats;
    EXPECT_EQ(hyperledger_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_transactions, 1u);
    EXPECT_EQ(stats.committed_transactions, 1u);
    EXPECT_EQ(stats.rejected_transactions, 0u);
}

TEST_F(HyperledgerBridgeTest, EOVRejectNaNGradient) {
    uint64_t tx = hyperledger_eov_begin(bridge, 0.5f);

    /* Order with NaN gradient norm → should reject */
    int rc = hyperledger_eov_order(bridge, tx, NAN, 100);
    EXPECT_EQ(rc, -1);

    /* Validate should also fail */
    EXPECT_FALSE(hyperledger_eov_validate(bridge, tx, nullptr, 0));
}

TEST_F(HyperledgerBridgeTest, EOVRejectInfGradient) {
    uint64_t tx = hyperledger_eov_begin(bridge, 0.5f);

    /* Order with Inf gradient norm → should reject */
    int rc = hyperledger_eov_order(bridge, tx, INFINITY, 100);
    EXPECT_EQ(rc, -1);
}

TEST_F(HyperledgerBridgeTest, EOVValidateWithGradients) {
    hyperledger_bridge_config_t config = hyperledger_bridge_default_config();
    config.validate_every_step = true;
    hyperledger_bridge_t* b = hyperledger_bridge_create(&config);
    ASSERT_NE(b, nullptr);

    uint64_t tx = hyperledger_eov_begin(b, 0.5f);
    hyperledger_eov_order(b, tx, 1.0f, 10);

    /* Good gradients should pass */
    float good_grads[10] = {0.1f, -0.2f, 0.15f, -0.05f, 0.3f,
                             0.1f, -0.1f, 0.2f, -0.15f, 0.05f};
    EXPECT_TRUE(hyperledger_eov_validate(b, tx, good_grads, 10));

    hyperledger_bridge_destroy(b);
}

TEST_F(HyperledgerBridgeTest, EOVValidateWithNaNGradients) {
    hyperledger_bridge_config_t config = hyperledger_bridge_default_config();
    config.validate_every_step = true;
    config.anomaly_reject_threshold = 0.3f;  /* Lower threshold to trigger rejection */
    hyperledger_bridge_t* b = hyperledger_bridge_create(&config);
    ASSERT_NE(b, nullptr);

    uint64_t tx = hyperledger_eov_begin(b, 0.5f);
    hyperledger_eov_order(b, tx, 1.0f, 10);

    /* Mostly NaN gradients should reject */
    float bad_grads[10] = {NAN, NAN, NAN, NAN, NAN,
                            NAN, NAN, NAN, NAN, NAN};
    EXPECT_FALSE(hyperledger_eov_validate(b, tx, bad_grads, 10));

    hyperledger_bridge_stats_t stats;
    hyperledger_bridge_get_stats(b, &stats);
    EXPECT_GE(stats.rejected_transactions, 1u);

    hyperledger_bridge_destroy(b);
}

TEST_F(HyperledgerBridgeTest, EOVMultipleTransactions) {
    for (int i = 0; i < 50; i++) {
        uint64_t tx = hyperledger_eov_begin(bridge, 0.5f - i * 0.005f);
        EXPECT_EQ((uint64_t)(i + 1), tx);
        hyperledger_eov_order(bridge, tx, 1.0f, 10);
        hyperledger_eov_validate(bridge, tx, nullptr, 0);
        hyperledger_eov_commit(bridge, tx, 0.01f, 0.5f - i * 0.005f);
    }

    hyperledger_bridge_stats_t stats;
    hyperledger_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_transactions, 50u);
    EXPECT_EQ(stats.committed_transactions, 50u);
}

TEST_F(HyperledgerBridgeTest, EOVGetState) {
    uint64_t tx = hyperledger_eov_begin(bridge, 0.7f);
    hyperledger_eov_order(bridge, tx, 2.5f, 100);

    eov_transaction_t state;
    EXPECT_EQ(hyperledger_eov_get_state(bridge, &state), 0);
    EXPECT_EQ(state.tx_id, tx);
    EXPECT_EQ(state.phase, EOV_PHASE_ORDER);
    EXPECT_FLOAT_EQ(state.gradient_norm, 2.5f);
    EXPECT_FLOAT_EQ(state.loss, 0.7f);
}

TEST_F(HyperledgerBridgeTest, EOVWrongTxId) {
    uint64_t tx = hyperledger_eov_begin(bridge, 0.5f);
    /* Order with wrong tx ID */
    EXPECT_EQ(hyperledger_eov_order(bridge, tx + 999, 1.0f, 10), -1);
}

TEST_F(HyperledgerBridgeTest, EOVDisabled) {
    hyperledger_bridge_config_t config = hyperledger_bridge_default_config();
    config.enable_eov_pipeline = false;
    hyperledger_bridge_t* b = hyperledger_bridge_create(&config);
    ASSERT_NE(b, nullptr);

    /* All phases should be no-ops */
    uint64_t tx = hyperledger_eov_begin(b, 0.5f);
    EXPECT_GT(tx, 0u);
    EXPECT_EQ(hyperledger_eov_order(b, tx, 1.0f, 10), 0);
    EXPECT_TRUE(hyperledger_eov_validate(b, tx, nullptr, 0));
    EXPECT_EQ(hyperledger_eov_commit(b, tx, 0.01f, 0.45f), 0);

    hyperledger_bridge_destroy(b);
}

TEST_F(HyperledgerBridgeTest, EOVValidationInterval) {
    /* Default interval = 10, should skip first 9 steps */
    for (int i = 0; i < 9; i++) {
        uint64_t tx = hyperledger_eov_begin(bridge, 0.5f);
        hyperledger_eov_order(bridge, tx, 1.0f, 10);
        /* Should pass (skipped) even with bad-ish gradients */
        EXPECT_TRUE(hyperledger_eov_validate(bridge, tx, nullptr, 0));
        hyperledger_eov_commit(bridge, tx, 0.01f, 0.45f);
    }
}

//=============================================================================
// Consensus-Gated Inference Tests (Integration Point #2)
//=============================================================================

TEST_F(HyperledgerBridgeTest, ConsensusGateSkipNoCollective) {
    float decision[4] = {0.8f, 0.1f, 0.05f, 0.05f};
    consensus_gate_result_t result;
    EXPECT_EQ(hyperledger_consensus_gate(bridge, decision, 4, 0.9f, &result), 0);
    EXPECT_EQ(result, CONSENSUS_GATE_SKIP);
}

TEST_F(HyperledgerBridgeTest, ConsensusGatePassHighConfidence) {
    /* Enable consensus and connect a fake collective */
    hyperledger_bridge_config_t config = hyperledger_bridge_default_config();
    config.enable_consensus_gate = true;
    hyperledger_bridge_t* b = hyperledger_bridge_create(&config);
    ASSERT_NE(b, nullptr);

    /* Fake collective: use a non-null pointer */
    int fake_collective = 42;
    hyperledger_bridge_connect_collective(b, (struct collective_cognition*)&fake_collective);

    float decision[4] = {0.8f, 0.1f, 0.05f, 0.05f};
    consensus_gate_result_t result;
    EXPECT_EQ(hyperledger_consensus_gate(b, decision, 4, 0.9f, &result), 0);
    EXPECT_EQ(result, CONSENSUS_GATE_PASS);

    hyperledger_bridge_destroy(b);
}

TEST_F(HyperledgerBridgeTest, ConsensusGateRejectLowConfidence) {
    hyperledger_bridge_config_t config = hyperledger_bridge_default_config();
    config.enable_consensus_gate = true;
    hyperledger_bridge_t* b = hyperledger_bridge_create(&config);
    ASSERT_NE(b, nullptr);

    int fake_collective = 42;
    hyperledger_bridge_connect_collective(b, (struct collective_cognition*)&fake_collective);

    float decision[4] = {0.3f, 0.3f, 0.2f, 0.2f};
    consensus_gate_result_t result;
    /* Confidence 0.4 < threshold 0.667 → reject */
    EXPECT_EQ(hyperledger_consensus_gate(b, decision, 4, 0.4f, &result), 0);
    EXPECT_EQ(result, CONSENSUS_GATE_REJECT);

    hyperledger_bridge_stats_t stats;
    hyperledger_bridge_get_stats(b, &stats);
    EXPECT_EQ(stats.consensus_failed, 1u);

    hyperledger_bridge_destroy(b);
}

TEST_F(HyperledgerBridgeTest, ConsensusGateNullResult) {
    EXPECT_EQ(hyperledger_consensus_gate(bridge, nullptr, 0, 0.5f, nullptr), -1);
}

TEST_F(HyperledgerBridgeTest, ConsensusVoteNoConsensus) {
    EXPECT_EQ(hyperledger_consensus_vote(bridge, 1, 0.9f, 0.8f), -1);
}

//=============================================================================
// Audit Log Tests (Integration Point #3)
//=============================================================================

TEST_F(HyperledgerBridgeTest, AuditLogEntry) {
    hyperledger_audit_entry_t entry = {};
    entry.type = AUDIT_WEIGHT_UPDATE;
    entry.data.weight_update.weight_delta_norm = 0.05f;
    entry.data.weight_update.loss_before = 0.5f;
    entry.data.weight_update.loss_after = 0.45f;

    uint64_t seq = hyperledger_audit_log(bridge, &entry);
    EXPECT_EQ(seq, 1u);

    EXPECT_EQ(hyperledger_audit_count(bridge), 1u);
}

TEST_F(HyperledgerBridgeTest, AuditLogMultiple) {
    for (int i = 0; i < 100; i++) {
        hyperledger_audit_entry_t entry = {};
        entry.type = AUDIT_WEIGHT_UPDATE;
        entry.data.weight_update.weight_delta_norm = 0.01f * i;
        uint64_t seq = hyperledger_audit_log(bridge, &entry);
        EXPECT_EQ(seq, (uint64_t)(i + 1));
    }
    EXPECT_EQ(hyperledger_audit_count(bridge), 100u);
}

TEST_F(HyperledgerBridgeTest, AuditRetrieveEntry) {
    hyperledger_audit_entry_t entry = {};
    entry.type = AUDIT_BYZANTINE_DETECTED;
    entry.data.byzantine.byzantine_count = 3;
    entry.data.byzantine.confidence = 0.95f;
    entry.data.byzantine.threat_level = 0.7f;

    uint64_t seq = hyperledger_audit_log(bridge, &entry);

    hyperledger_audit_entry_t retrieved;
    EXPECT_EQ(hyperledger_audit_get_entry(bridge, seq, &retrieved), 0);
    EXPECT_EQ(retrieved.type, AUDIT_BYZANTINE_DETECTED);
    EXPECT_EQ(retrieved.data.byzantine.byzantine_count, 3u);
    EXPECT_FLOAT_EQ(retrieved.data.byzantine.confidence, 0.95f);
}

TEST_F(HyperledgerBridgeTest, AuditRetrieveInvalid) {
    hyperledger_audit_entry_t entry;
    /* No entries yet */
    EXPECT_EQ(hyperledger_audit_get_entry(bridge, 1, &entry), -1);
    /* Sequence 0 is always invalid */
    EXPECT_EQ(hyperledger_audit_get_entry(bridge, 0, &entry), -1);
}

TEST_F(HyperledgerBridgeTest, AuditHashChainVerify) {
    /* Log several entries */
    for (int i = 0; i < 20; i++) {
        hyperledger_audit_entry_t entry = {};
        entry.type = (audit_entry_type_t)(i % 6);
        entry.data.weight_update.weight_delta_norm = 0.01f * i;
        hyperledger_audit_log(bridge, &entry);
    }

    /* Verify the chain is intact */
    uint64_t verified = 0;
    EXPECT_TRUE(hyperledger_audit_verify_chain(bridge, &verified));
    EXPECT_EQ(verified, 20u);
}

TEST_F(HyperledgerBridgeTest, AuditEmptyChainValid) {
    uint64_t verified = 0;
    EXPECT_TRUE(hyperledger_audit_verify_chain(bridge, &verified));
    EXPECT_EQ(verified, 0u);
}

TEST_F(HyperledgerBridgeTest, AuditLogFromEOV) {
    /* EOV commit should create audit entries */
    uint64_t tx = hyperledger_eov_begin(bridge, 0.5f);
    hyperledger_eov_order(bridge, tx, 1.0f, 10);
    hyperledger_eov_validate(bridge, tx, nullptr, 0);
    hyperledger_eov_commit(bridge, tx, 0.05f, 0.45f);  /* delta=0.05 > threshold=0.001 */

    EXPECT_GE(hyperledger_audit_count(bridge), 1u);
}

TEST_F(HyperledgerBridgeTest, AuditLogBelowThreshold) {
    /* Weight delta below audit threshold should NOT be logged */
    hyperledger_bridge_config_t config = hyperledger_bridge_default_config();
    config.audit_weight_threshold = 1.0f;  /* High threshold */
    hyperledger_bridge_t* b = hyperledger_bridge_create(&config);
    ASSERT_NE(b, nullptr);

    uint64_t tx = hyperledger_eov_begin(b, 0.5f);
    hyperledger_eov_order(b, tx, 1.0f, 10);
    hyperledger_eov_validate(b, tx, nullptr, 0);
    hyperledger_eov_commit(b, tx, 0.001f, 0.45f);  /* delta=0.001 < threshold=1.0 */

    EXPECT_EQ(hyperledger_audit_count(b), 0u);

    hyperledger_bridge_destroy(b);
}

TEST_F(HyperledgerBridgeTest, AuditRingBufferWrap) {
    /* Write more than ring buffer size to test wrap-around */
    uint32_t count = HYPERLEDGER_AUDIT_RING_SIZE + 100;
    for (uint32_t i = 0; i < count; i++) {
        hyperledger_audit_entry_t entry = {};
        entry.type = AUDIT_WEIGHT_UPDATE;
        entry.data.weight_update.weight_delta_norm = 0.01f * i;
        hyperledger_audit_log(bridge, &entry);
    }

    EXPECT_EQ(hyperledger_audit_count(bridge), (uint64_t)count);

    /* Oldest entries should be gone */
    hyperledger_audit_entry_t entry;
    EXPECT_EQ(hyperledger_audit_get_entry(bridge, 1, &entry), -1);

    /* Latest entries should be retrievable */
    EXPECT_EQ(hyperledger_audit_get_entry(bridge, count, &entry), 0);
}

//=============================================================================
// Snapshot Tests
//=============================================================================

TEST_F(HyperledgerBridgeTest, SnapshotCreate) {
    uint32_t idx = hyperledger_snapshot_create(bridge);
    EXPECT_GT(idx, 0u);
}

TEST_F(HyperledgerBridgeTest, SnapshotMultiple) {
    for (int i = 0; i < HYPERLEDGER_MAX_ROLLBACK_DEPTH; i++) {
        uint32_t idx = hyperledger_snapshot_create(bridge);
        EXPECT_EQ(idx, (uint32_t)(i + 1));
    }
    /* Beyond max should return 0 */
    EXPECT_EQ(hyperledger_snapshot_create(bridge), 0u);
}

TEST_F(HyperledgerBridgeTest, SnapshotRollback) {
    uint32_t idx = hyperledger_snapshot_create(bridge);
    EXPECT_EQ(hyperledger_snapshot_rollback(bridge, idx), 0);

    hyperledger_bridge_stats_t stats;
    hyperledger_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.rollbacks_performed, 1u);
}

TEST_F(HyperledgerBridgeTest, SnapshotRollbackInvalid) {
    EXPECT_EQ(hyperledger_snapshot_rollback(bridge, 0), -1);
    EXPECT_EQ(hyperledger_snapshot_rollback(bridge, 999), -1);
}

TEST_F(HyperledgerBridgeTest, SnapshotDisabled) {
    hyperledger_bridge_config_t config = hyperledger_bridge_default_config();
    config.enable_snapshots = false;
    hyperledger_bridge_t* b = hyperledger_bridge_create(&config);
    ASSERT_NE(b, nullptr);

    EXPECT_EQ(hyperledger_snapshot_create(b), 0u);

    hyperledger_bridge_destroy(b);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(HyperledgerBridgeTest, StatsInitialZero) {
    hyperledger_bridge_stats_t stats;
    EXPECT_EQ(hyperledger_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_transactions, 0u);
    EXPECT_EQ(stats.committed_transactions, 0u);
    EXPECT_EQ(stats.rejected_transactions, 0u);
    EXPECT_EQ(stats.consensus_proposals, 0u);
    EXPECT_EQ(stats.audit_entries, 0u);
}

TEST_F(HyperledgerBridgeTest, StatsNullBridge) {
    hyperledger_bridge_stats_t stats;
    EXPECT_EQ(hyperledger_bridge_get_stats(nullptr, &stats), -1);
}

TEST_F(HyperledgerBridgeTest, StatsReset) {
    /* Generate some stats */
    uint64_t tx = hyperledger_eov_begin(bridge, 0.5f);
    hyperledger_eov_order(bridge, tx, 1.0f, 10);
    hyperledger_eov_validate(bridge, tx, nullptr, 0);
    hyperledger_eov_commit(bridge, tx, 0.05f, 0.45f);

    hyperledger_bridge_stats_t stats;
    hyperledger_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_transactions, 0u);

    /* Reset */
    hyperledger_bridge_reset_stats(bridge);
    hyperledger_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_transactions, 0u);
    EXPECT_EQ(stats.committed_transactions, 0u);
}

//=============================================================================
// Combined Integration Tests
//=============================================================================

TEST_F(HyperledgerBridgeTest, FullTrainingCycle) {
    /* Simulate 20 training steps with EOV pipeline + audit */
    for (int step = 0; step < 20; step++) {
        float loss = 1.0f - step * 0.04f;

        uint64_t tx = hyperledger_eov_begin(bridge, loss);
        hyperledger_eov_order(bridge, tx, loss * 2.0f, 100);
        bool valid = hyperledger_eov_validate(bridge, tx, nullptr, 0);
        EXPECT_TRUE(valid);

        if (valid) {
            float new_loss = loss * 0.95f;
            hyperledger_eov_commit(bridge, tx, 0.05f, new_loss);
        }

        /* Create snapshot every 10 steps */
        if (step % 10 == 0) {
            uint32_t snap = hyperledger_snapshot_create(bridge);
            EXPECT_GT(snap, 0u);
        }
    }

    /* Verify audit chain */
    uint64_t verified = 0;
    EXPECT_TRUE(hyperledger_audit_verify_chain(bridge, &verified));
    EXPECT_GT(verified, 0u);

    /* Check final stats */
    hyperledger_bridge_stats_t stats;
    hyperledger_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_transactions, 20u);
    EXPECT_EQ(stats.committed_transactions, 20u);
    EXPECT_EQ(stats.rejected_transactions, 0u);
    EXPECT_EQ(stats.snapshots_created, 2u);
}

TEST_F(HyperledgerBridgeTest, TrainingWithRejections) {
    hyperledger_bridge_config_t config = hyperledger_bridge_default_config();
    config.validate_every_step = true;
    config.anomaly_reject_threshold = 0.3f;
    hyperledger_bridge_t* b = hyperledger_bridge_create(&config);
    ASSERT_NE(b, nullptr);

    /* Step 1: Good gradients → commit */
    uint64_t tx1 = hyperledger_eov_begin(b, 0.5f);
    hyperledger_eov_order(b, tx1, 1.0f, 5);
    float good[5] = {0.1f, -0.2f, 0.15f, -0.05f, 0.3f};
    EXPECT_TRUE(hyperledger_eov_validate(b, tx1, good, 5));
    hyperledger_eov_commit(b, tx1, 0.05f, 0.45f);

    /* Step 2: Bad gradients → reject */
    uint64_t tx2 = hyperledger_eov_begin(b, 0.45f);
    hyperledger_eov_order(b, tx2, 1.0f, 5);
    float bad[5] = {NAN, NAN, NAN, NAN, NAN};
    EXPECT_FALSE(hyperledger_eov_validate(b, tx2, bad, 5));

    hyperledger_bridge_stats_t stats;
    hyperledger_bridge_get_stats(b, &stats);
    EXPECT_EQ(stats.committed_transactions, 1u);
    EXPECT_GE(stats.rejected_transactions, 1u);
    EXPECT_GT(stats.rejection_rate, 0.0f);

    hyperledger_bridge_destroy(b);
}

TEST_F(HyperledgerBridgeTest, ConsensusAndAuditCombined) {
    hyperledger_bridge_config_t config = hyperledger_bridge_default_config();
    config.enable_consensus_gate = true;
    hyperledger_bridge_t* b = hyperledger_bridge_create(&config);
    ASSERT_NE(b, nullptr);

    int fake_collective = 42;
    hyperledger_bridge_connect_collective(b, (struct collective_cognition*)&fake_collective);

    /* High confidence → pass → audit entry */
    float decision[4] = {0.9f, 0.05f, 0.03f, 0.02f};
    consensus_gate_result_t result;
    hyperledger_consensus_gate(b, decision, 4, 0.85f, &result);
    EXPECT_EQ(result, CONSENSUS_GATE_PASS);

    /* Low confidence → reject → audit entry */
    hyperledger_consensus_gate(b, decision, 4, 0.3f, &result);
    EXPECT_EQ(result, CONSENSUS_GATE_REJECT);

    /* Both should have audit entries */
    EXPECT_GE(hyperledger_audit_count(b), 2u);

    /* Verify chain integrity */
    uint64_t verified = 0;
    EXPECT_TRUE(hyperledger_audit_verify_chain(b, &verified));
    EXPECT_GE(verified, 2u);

    hyperledger_bridge_destroy(b);
}
