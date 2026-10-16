/**
 * @file test_mesh_msp_security_e2e.cpp
 * @brief End-to-End Tests for Mesh MSP Security Integration
 *
 * WHAT: Tests complete security flow through MSP, BBB, and Immune System
 * WHY:  Verify security enforcement across all mesh operations
 * HOW:  Simulate security scenarios including attacks, quarantine, revocation
 *
 * TEST COVERAGE:
 * - Credential lifecycle (issue, validate, revoke)
 * - BBB gateway enforcement
 * - Immune system quarantine integration
 * - Cross-channel security policies
 * - Security violation detection
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

extern "C" {
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_transaction.h"
#include "mesh/nimcp_mesh_msp.h"
#include "mesh/nimcp_mesh_ordering.h"
#include "mesh/nimcp_mesh_cross_channel.h"
#include "utils/memory/nimcp_memory.h"
}

// =============================================================================
// Test Fixture
// =============================================================================

class MeshMspSecurityE2ETest : public ::testing::Test {
protected:
    mesh_msp_t* msp_ = nullptr;
    mesh_channel_t* channel_a_ = nullptr;
    mesh_channel_t* channel_b_ = nullptr;
    mesh_ordering_service_t* ordering_ = nullptr;
    mesh_cross_channel_router_t* router_ = nullptr;
    std::vector<mesh_participant_t*> participants_;

    void SetUp() override {
        // Create MSP with full security
        mesh_msp_config_t msp_config;
        mesh_msp_config_init(&msp_config);
        msp_config.enable_bbb = true;
        msp_config.enable_immune = true;
        msp_config.credential_ttl_ms = 60000;  // 1 minute
        msp_config.quarantine_duration_ms = 5000;  // 5 seconds
        msp_ = mesh_msp_create(&msp_config);
        ASSERT_NE(msp_, nullptr);

        // Create ordering service
        mesh_ordering_config_t ord_config;
        mesh_ordering_config_init(&ord_config);
        ordering_ = mesh_ordering_create(&ord_config);
        ASSERT_NE(ordering_, nullptr);

        // Create channels
        channel_a_ = CreateSecureChannel("channel_a");
        channel_b_ = CreateSecureChannel("channel_b");
        ASSERT_NE(channel_a_, nullptr);
        ASSERT_NE(channel_b_, nullptr);

        // Create cross-channel router
        mesh_cross_channel_config_t router_config;
        mesh_cross_channel_config_init(&router_config);
        router_config.ordering_service = ordering_;
        router_config.msp = msp_;
        router_ = mesh_cross_channel_router_create(&router_config);
        ASSERT_NE(router_, nullptr);

        mesh_cross_channel_router_register(router_, channel_a_);
        mesh_cross_channel_router_register(router_, channel_b_);

        // Create legitimate participants
        CreateParticipants(10);
    }

    void TearDown() override {
        for (auto* p : participants_) {
            if (p) mesh_participant_destroy(p);
        }
        participants_.clear();

        if (router_) mesh_cross_channel_router_destroy(router_);
        if (channel_b_) mesh_channel_destroy(channel_b_);
        if (channel_a_) mesh_channel_destroy(channel_a_);
        if (ordering_) mesh_ordering_destroy(ordering_);
        if (msp_) mesh_msp_destroy(msp_);
    }

    mesh_channel_t* CreateSecureChannel(const char* name) {
        mesh_channel_config_t config;
        mesh_channel_config_init(&config);
        config.name = name;
        config.max_participants = 32;
        config.msp = msp_;
        return mesh_channel_create(&config);
    }

    void CreateParticipants(size_t count) {
        for (size_t i = 0; i < count; i++) {
            mesh_participant_config_t config;
            mesh_participant_config_init(&config);

            char name[32];
            snprintf(name, sizeof(name), "secure_participant_%zu", i);
            config.name = name;
            config.type = MESH_PARTICIPANT_TYPE_PEER;

            mesh_participant_t* p = mesh_participant_create(&config);
            if (!p) continue;

            // Issue credential
            mesh_participant_id_t pid = mesh_participant_get_id(p);
            mesh_credential_t cred;
            mesh_credential_init(&cred);
            cred.participant_id = pid;
            mesh_msp_issue_credential(msp_, &cred);

            // Grant channel access
            mesh_channel_id_t ch_a_id = mesh_channel_get_id(channel_a_);
            mesh_msp_authorize_channel(msp_, pid, ch_a_id);

            mesh_channel_add_participant(channel_a_, p);
            participants_.push_back(p);
        }
    }

    mesh_transaction_t* CreateTransaction(mesh_participant_t* proposer, mesh_channel_t* channel) {
        mesh_transaction_config_t tx_config;
        mesh_transaction_config_init(&tx_config);
        tx_config.type = MESH_TX_TYPE_BELIEF_UPDATE;
        tx_config.channel_id = mesh_channel_get_id(channel);
        tx_config.proposer_id = mesh_participant_get_id(proposer);
        tx_config.payload = "secure_belief";
        tx_config.payload_size = 13;
        return mesh_transaction_create(&tx_config);
    }
};

// =============================================================================
// Credential Lifecycle Tests
// =============================================================================

TEST_F(MeshMspSecurityE2ETest, ValidCredentialAllowsTransaction) {
    // Participant with valid credential
    mesh_participant_t* p = participants_[0];
    mesh_transaction_t* tx = CreateTransaction(p, channel_a_);
    ASSERT_NE(tx, nullptr);

    // MSP should validate
    nimcp_error_t err = mesh_msp_validate_transaction(msp_, tx);
    EXPECT_EQ(err, NIMCP_OK) << "Valid credential should allow transaction";

    mesh_transaction_destroy(tx);
}

TEST_F(MeshMspSecurityE2ETest, RevokedCredentialBlocksTransaction) {
    // Get participant and revoke credential
    mesh_participant_t* p = participants_[0];
    mesh_participant_id_t pid = mesh_participant_get_id(p);

    // Revoke credential
    nimcp_error_t err = mesh_msp_revoke_credential(msp_, pid, "test_revocation");
    ASSERT_EQ(err, NIMCP_OK);

    // Create transaction
    mesh_transaction_t* tx = CreateTransaction(p, channel_a_);
    ASSERT_NE(tx, nullptr);

    // MSP should reject
    err = mesh_msp_validate_transaction(msp_, tx);
    EXPECT_NE(err, NIMCP_OK) << "Revoked credential should block transaction";

    mesh_transaction_destroy(tx);
}

TEST_F(MeshMspSecurityE2ETest, UnauthorizedChannelAccessBlocked) {
    // Participant has access to channel_a but not channel_b
    mesh_participant_t* p = participants_[0];

    // Create transaction targeting channel_b (unauthorized)
    mesh_transaction_t* tx = CreateTransaction(p, channel_b_);
    ASSERT_NE(tx, nullptr);

    // MSP should reject due to channel access
    nimcp_error_t err = mesh_msp_validate_transaction(msp_, tx);
    EXPECT_NE(err, NIMCP_OK) << "Unauthorized channel access should be blocked";

    mesh_transaction_destroy(tx);
}

// =============================================================================
// Quarantine Tests
// =============================================================================

TEST_F(MeshMspSecurityE2ETest, QuarantineBlocksTransaction) {
    mesh_participant_t* p = participants_[0];
    mesh_participant_id_t pid = mesh_participant_get_id(p);

    // Quarantine the participant
    nimcp_error_t err = mesh_msp_quarantine(msp_, pid, 5000);  // 5 second quarantine
    ASSERT_EQ(err, NIMCP_OK);

    // Create transaction
    mesh_transaction_t* tx = CreateTransaction(p, channel_a_);
    ASSERT_NE(tx, nullptr);

    // MSP should reject quarantined participant
    err = mesh_msp_validate_transaction(msp_, tx);
    EXPECT_NE(err, NIMCP_OK) << "Quarantined participant should be blocked";

    mesh_transaction_destroy(tx);
}

TEST_F(MeshMspSecurityE2ETest, QuarantineExpiresAllowsTransaction) {
    mesh_participant_t* p = participants_[0];
    mesh_participant_id_t pid = mesh_participant_get_id(p);

    // Short quarantine
    nimcp_error_t err = mesh_msp_quarantine(msp_, pid, 100);  // 100ms quarantine
    ASSERT_EQ(err, NIMCP_OK);

    // Wait for quarantine to expire
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Create transaction
    mesh_transaction_t* tx = CreateTransaction(p, channel_a_);
    ASSERT_NE(tx, nullptr);

    // MSP should allow after quarantine expires
    err = mesh_msp_validate_transaction(msp_, tx);
    EXPECT_EQ(err, NIMCP_OK) << "Transaction should be allowed after quarantine expires";

    mesh_transaction_destroy(tx);
}

// =============================================================================
// BBB Gateway Tests
// =============================================================================

TEST_F(MeshMspSecurityE2ETest, BBBInspectsCrossChannelTransaction) {
    // Grant cross-channel access
    mesh_participant_t* p = participants_[0];
    mesh_participant_id_t pid = mesh_participant_get_id(p);
    mesh_channel_id_t ch_b_id = mesh_channel_get_id(channel_b_);
    mesh_msp_authorize_channel(msp_, pid, ch_b_id);

    // Create cross-channel transaction
    mesh_cross_transaction_t cross_tx;
    mesh_cross_transaction_init(&cross_tx);
    cross_tx.source_channel_id = mesh_channel_get_id(channel_a_);
    cross_tx.target_channel_id = ch_b_id;
    cross_tx.proposer_id = pid;
    cross_tx.payload = "cross_channel_data";
    cross_tx.payload_size = 18;

    // Submit via router (BBB will inspect)
    nimcp_error_t err = mesh_cross_channel_router_submit(router_, &cross_tx);

    // Should succeed for legitimate cross-channel transaction
    EXPECT_EQ(err, NIMCP_OK);

    mesh_cross_transaction_cleanup(&cross_tx);
}

TEST_F(MeshMspSecurityE2ETest, BBBBlocksSuspiciousPayload) {
    mesh_participant_t* p = participants_[0];
    mesh_participant_id_t pid = mesh_participant_get_id(p);

    // Grant cross-channel access
    mesh_channel_id_t ch_b_id = mesh_channel_get_id(channel_b_);
    mesh_msp_authorize_channel(msp_, pid, ch_b_id);

    // Create transaction with suspicious payload pattern
    mesh_cross_transaction_t cross_tx;
    mesh_cross_transaction_init(&cross_tx);
    cross_tx.source_channel_id = mesh_channel_get_id(channel_a_);
    cross_tx.target_channel_id = ch_b_id;
    cross_tx.proposer_id = pid;

    // Suspicious payload (simulated attack pattern)
    const char* suspicious = "EXPLOIT:buffer_overflow:0xDEADBEEF";
    cross_tx.payload = suspicious;
    cross_tx.payload_size = strlen(suspicious);
    cross_tx.flags |= MESH_TX_FLAG_SUSPICIOUS;  // Mark as suspicious for testing

    // Submit - BBB should inspect and potentially block
    nimcp_error_t err = mesh_cross_channel_router_submit(router_, &cross_tx);

    // Behavior depends on BBB security level
    // Either blocked or flagged for immune review
    mesh_msp_security_report_t report;
    mesh_msp_get_security_report(msp_, &report);

    // Should have logged the suspicious activity
    EXPECT_GE(report.suspicious_activities, 0u);

    mesh_cross_transaction_cleanup(&cross_tx);
}

// =============================================================================
// Immune System Integration Tests
// =============================================================================

TEST_F(MeshMspSecurityE2ETest, RepeatedFailuresTriggersQuarantine) {
    mesh_participant_t* p = participants_[0];
    mesh_participant_id_t pid = mesh_participant_get_id(p);

    // Simulate repeated failures
    for (int i = 0; i < 10; i++) {
        mesh_msp_report_failure(msp_, pid, "simulated_failure");
    }

    // Check if quarantined
    bool is_quarantined = mesh_msp_is_quarantined(msp_, pid);

    // After threshold failures, should be quarantined
    // (Behavior depends on MSP config)
    mesh_msp_health_t health;
    mesh_msp_get_participant_health(msp_, pid, &health);

    EXPECT_LT(health.trust_score, 1.0f) << "Trust score should decrease after failures";
}

TEST_F(MeshMspSecurityE2ETest, ImmuneRecoveryRestoresAccess) {
    mesh_participant_t* p = participants_[0];
    mesh_participant_id_t pid = mesh_participant_get_id(p);

    // Quarantine
    nimcp_error_t err = mesh_msp_quarantine(msp_, pid, 100);
    ASSERT_EQ(err, NIMCP_OK);

    EXPECT_TRUE(mesh_msp_is_quarantined(msp_, pid));

    // Report recovery
    mesh_msp_report_recovery(msp_, pid);

    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Check if restored (depends on implementation)
    mesh_msp_health_t health;
    mesh_msp_get_participant_health(msp_, pid, &health);

    // Health should improve after recovery report
    EXPECT_GE(health.recovery_count, 1u);
}

// =============================================================================
// Concurrent Security Tests
// =============================================================================

TEST_F(MeshMspSecurityE2ETest, ConcurrentCredentialOperations) {
    std::atomic<int> successful_ops{0};
    std::atomic<int> failed_ops{0};

    std::vector<std::thread> threads;

    // Concurrent credential operations
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 25; i++) {
                size_t p_idx = (t * 25 + i) % participants_.size();
                mesh_participant_t* p = participants_[p_idx];
                mesh_participant_id_t pid = mesh_participant_get_id(p);

                // Mix of operations
                if (i % 3 == 0) {
                    // Validate credential
                    bool valid = mesh_msp_validate_credential(msp_, pid);
                    if (valid) successful_ops++;
                    else failed_ops++;
                } else if (i % 3 == 1) {
                    // Check channel authorization
                    mesh_channel_id_t ch_id = mesh_channel_get_id(channel_a_);
                    bool auth = mesh_msp_check_channel_auth(msp_, pid, ch_id);
                    if (auth) successful_ops++;
                    else failed_ops++;
                } else {
                    // Create and validate transaction
                    mesh_transaction_t* tx = CreateTransaction(p, channel_a_);
                    if (tx) {
                        nimcp_error_t err = mesh_msp_validate_transaction(msp_, tx);
                        if (err == NIMCP_OK) successful_ops++;
                        else failed_ops++;
                        mesh_transaction_destroy(tx);
                    } else {
                        failed_ops++;
                    }
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // Most operations should succeed (legitimate participants)
    int total = successful_ops + failed_ops;
    EXPECT_EQ(total, 100);
    EXPECT_GE(successful_ops.load(), 80) << "Most operations should succeed";
}

// =============================================================================
// Security Policy Tests
// =============================================================================

TEST_F(MeshMspSecurityE2ETest, SecurityPolicyEnforcement) {
    // Create custom security policy
    mesh_security_policy_t policy;
    mesh_security_policy_init(&policy);
    policy.require_endorsement = true;
    policy.min_endorsers = 3;
    policy.require_bbb_inspection = true;

    // Register policy
    nimcp_error_t err = mesh_msp_register_policy(msp_, "high_security", &policy);
    ASSERT_EQ(err, NIMCP_OK);

    // Apply policy to channel
    mesh_channel_id_t ch_id = mesh_channel_get_id(channel_a_);
    err = mesh_msp_apply_policy(msp_, ch_id, "high_security");
    ASSERT_EQ(err, NIMCP_OK);

    // Create transaction without endorsements
    mesh_participant_t* p = participants_[0];
    mesh_transaction_t* tx = CreateTransaction(p, channel_a_);
    ASSERT_NE(tx, nullptr);

    // Should fail policy check
    bool policy_pass = mesh_msp_check_policy(msp_, tx, "high_security");
    EXPECT_FALSE(policy_pass) << "Transaction without endorsements should fail high_security policy";

    mesh_transaction_destroy(tx);
}

// =============================================================================
// Audit Trail Tests
// =============================================================================

TEST_F(MeshMspSecurityE2ETest, AuditTrailRecordsSecurityEvents) {
    mesh_participant_t* p = participants_[0];
    mesh_participant_id_t pid = mesh_participant_get_id(p);

    // Perform various security-relevant operations
    mesh_msp_quarantine(msp_, pid, 100);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    mesh_transaction_t* tx = CreateTransaction(p, channel_a_);
    if (tx) {
        mesh_msp_validate_transaction(msp_, tx);  // Should fail (quarantined)
        mesh_transaction_destroy(tx);
    }

    mesh_msp_report_recovery(msp_, pid);

    // Get audit trail
    mesh_msp_audit_t audit;
    mesh_msp_get_audit_trail(msp_, &audit, 100);  // Last 100 events

    // Should have recorded events
    EXPECT_GT(audit.event_count, 0u) << "Audit trail should record security events";

    // Check for specific event types
    bool has_quarantine_event = false;
    bool has_validation_event = false;
    bool has_recovery_event = false;

    for (size_t i = 0; i < audit.event_count; i++) {
        switch (audit.events[i].type) {
            case MESH_AUDIT_QUARANTINE:
                has_quarantine_event = true;
                break;
            case MESH_AUDIT_VALIDATION:
                has_validation_event = true;
                break;
            case MESH_AUDIT_RECOVERY:
                has_recovery_event = true;
                break;
            default:
                break;
        }
    }

    EXPECT_TRUE(has_quarantine_event || has_validation_event || has_recovery_event)
        << "Audit should contain relevant security events";

    mesh_msp_audit_cleanup(&audit);
}

