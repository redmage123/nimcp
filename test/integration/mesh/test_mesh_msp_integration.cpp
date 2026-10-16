/**
 * @file test_mesh_msp_integration.cpp
 * @brief Integration Tests for Mesh Membership Service Provider
 *
 * WHAT: Tests MSP with BBB and immune system integration
 * WHY:  Verify identity, access control, and security enforcement work correctly
 * HOW:  Test authentication, authorization, quarantine, and revocation flows
 *
 * TEST COVERAGE:
 * - Participant authentication with credentials
 * - Channel authorization and membership
 * - Immune system integration (quarantine, revocation)
 * - BBB gateway inspection for cross-channel
 * - Credential lifecycle management
 * - Access policy evaluation
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
#include "mesh/nimcp_mesh_msp.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_transaction.h"
#include "utils/memory/nimcp_memory.h"
}

// =============================================================================
// Test Fixture
// =============================================================================

// Callback for immune events
struct ImmuneCallbackContext {
    std::atomic<int> threat_count{0};
    std::atomic<int> quarantine_count{0};
    std::atomic<int> revocation_count{0};
    std::mutex mutex;
};

static void immune_event_callback(mesh_msp_immune_event_t event_type,
                                  mesh_participant_id_t participant_id,
                                  const char* reason,
                                  void* user_ctx) {
    if (user_ctx) {
        auto* ctx = static_cast<ImmuneCallbackContext*>(user_ctx);
        switch (event_type) {
            case MESH_MSP_IMMUNE_THREAT:
                ctx->threat_count++;
                break;
            case MESH_MSP_IMMUNE_QUARANTINE:
                ctx->quarantine_count++;
                break;
            case MESH_MSP_IMMUNE_REVOCATION:
                ctx->revocation_count++;
                break;
            default:
                break;
        }
    }
}

class MeshMSPIntegrationTest : public ::testing::Test {
protected:
    static constexpr size_t NUM_PARTICIPANTS = 6;
    static constexpr size_t NUM_CHANNELS = 2;

    mesh_msp_t* msp_ = nullptr;
    mesh_channel_t* channels_[NUM_CHANNELS] = {nullptr};
    std::vector<mesh_participant_t*> participants_;
    ImmuneCallbackContext immune_ctx_;

    void SetUp() override {
        // Create MSP
        mesh_msp_config_t msp_config;
        mesh_msp_config_init(&msp_config);
        msp_config.enable_bbb = true;
        msp_config.enable_immune = true;
        msp_config.quarantine_duration_ms = 500;
        msp_config.max_violations_before_revoke = 3;
        msp_config.immune_callback = immune_event_callback;
        msp_config.immune_callback_ctx = &immune_ctx_;

        msp_ = mesh_msp_create(&msp_config);
        ASSERT_NE(msp_, nullptr);

        // Create channels
        for (size_t i = 0; i < NUM_CHANNELS; i++) {
            mesh_channel_config_t ch_config;
            mesh_channel_config_init(&ch_config);

            char name[32];
            snprintf(name, sizeof(name), "channel_%zu", i);
            ch_config.name = name;
            ch_config.max_participants = 16;
            ch_config.msp = msp_;

            channels_[i] = mesh_channel_create(&ch_config);
            ASSERT_NE(channels_[i], nullptr);

            // Register channel with MSP
            mesh_channel_id_t ch_id = mesh_channel_get_id(channels_[i]);
            mesh_msp_register_channel(msp_, ch_id);
        }

        // Create participants
        for (size_t i = 0; i < NUM_PARTICIPANTS; i++) {
            mesh_participant_config_t p_config;
            mesh_participant_config_init(&p_config);

            char name[32];
            snprintf(name, sizeof(name), "participant_%zu", i);
            p_config.name = name;
            p_config.type = MESH_PARTICIPANT_TYPE_PEER;

            mesh_participant_t* p = mesh_participant_create(&p_config);
            ASSERT_NE(p, nullptr);
            participants_.push_back(p);
        }
    }

    void TearDown() override {
        for (auto* p : participants_) {
            if (p) mesh_participant_destroy(p);
        }
        participants_.clear();

        for (size_t i = 0; i < NUM_CHANNELS; i++) {
            if (channels_[i]) {
                mesh_channel_destroy(channels_[i]);
                channels_[i] = nullptr;
            }
        }

        if (msp_) {
            mesh_msp_destroy(msp_);
            msp_ = nullptr;
        }
    }

    // Helper: Issue credential to participant
    nimcp_error_t IssueCredential(size_t participant_idx) {
        if (participant_idx >= participants_.size()) return NIMCP_ERROR_INVALID_PARAM;

        mesh_participant_id_t pid = mesh_participant_get_id(participants_[participant_idx]);

        mesh_credential_t credential;
        mesh_credential_init(&credential);
        credential.participant_id = pid;
        credential.valid_from_ms = 0;
        credential.valid_until_ms = UINT64_MAX;

        return mesh_msp_issue_credential(msp_, &credential);
    }

    // Helper: Grant channel access
    nimcp_error_t GrantChannelAccess(size_t participant_idx, size_t channel_idx) {
        if (participant_idx >= participants_.size() || channel_idx >= NUM_CHANNELS) {
            return NIMCP_ERROR_INVALID_PARAM;
        }

        mesh_participant_id_t pid = mesh_participant_get_id(participants_[participant_idx]);
        mesh_channel_id_t ch_id = mesh_channel_get_id(channels_[channel_idx]);

        return mesh_msp_authorize_channel(msp_, pid, ch_id);
    }
};

// =============================================================================
// Authentication Tests
// =============================================================================

TEST_F(MeshMSPIntegrationTest, ParticipantAuthentication) {
    // Issue credential
    nimcp_error_t err = IssueCredential(0);
    ASSERT_EQ(err, NIMCP_OK);

    // Authenticate
    mesh_participant_id_t pid = mesh_participant_get_id(participants_[0]);
    bool authenticated = false;
    err = mesh_msp_authenticate(msp_, pid, &authenticated);
    ASSERT_EQ(err, NIMCP_OK);
    EXPECT_TRUE(authenticated);
}

TEST_F(MeshMSPIntegrationTest, AuthenticationWithoutCredential) {
    // Try to authenticate without credential
    mesh_participant_id_t pid = mesh_participant_get_id(participants_[0]);
    bool authenticated = true;  // Start true to verify it's set to false
    nimcp_error_t err = mesh_msp_authenticate(msp_, pid, &authenticated);

    // Should fail or return not authenticated
    EXPECT_FALSE(authenticated || err != NIMCP_OK);
}

// =============================================================================
// Authorization Tests
// =============================================================================

TEST_F(MeshMSPIntegrationTest, ChannelAuthorizationGrant) {
    // Issue credential first
    IssueCredential(0);

    // Grant access to channel 0
    nimcp_error_t err = GrantChannelAccess(0, 0);
    ASSERT_EQ(err, NIMCP_OK);

    // Verify authorization
    mesh_participant_id_t pid = mesh_participant_get_id(participants_[0]);
    mesh_channel_id_t ch_id = mesh_channel_get_id(channels_[0]);

    bool authorized = false;
    err = mesh_msp_check_channel_access(msp_, pid, ch_id, &authorized);
    ASSERT_EQ(err, NIMCP_OK);
    EXPECT_TRUE(authorized);
}

TEST_F(MeshMSPIntegrationTest, ChannelAuthorizationDenied) {
    // Issue credential but don't grant channel access
    IssueCredential(0);

    // Check access (should be denied)
    mesh_participant_id_t pid = mesh_participant_get_id(participants_[0]);
    mesh_channel_id_t ch_id = mesh_channel_get_id(channels_[0]);

    bool authorized = true;
    nimcp_error_t err = mesh_msp_check_channel_access(msp_, pid, ch_id, &authorized);
    ASSERT_EQ(err, NIMCP_OK);
    EXPECT_FALSE(authorized);
}

TEST_F(MeshMSPIntegrationTest, MultiChannelAuthorization) {
    // Issue credential and grant access to multiple channels
    IssueCredential(0);
    GrantChannelAccess(0, 0);
    GrantChannelAccess(0, 1);

    mesh_participant_id_t pid = mesh_participant_get_id(participants_[0]);

    // Should have access to both channels
    for (size_t i = 0; i < NUM_CHANNELS; i++) {
        mesh_channel_id_t ch_id = mesh_channel_get_id(channels_[i]);
        bool authorized = false;
        mesh_msp_check_channel_access(msp_, pid, ch_id, &authorized);
        EXPECT_TRUE(authorized) << "Should have access to channel " << i;
    }
}

// =============================================================================
// Quarantine Tests
// =============================================================================

TEST_F(MeshMSPIntegrationTest, ParticipantQuarantine) {
    IssueCredential(0);
    GrantChannelAccess(0, 0);

    mesh_participant_id_t pid = mesh_participant_get_id(participants_[0]);

    // Quarantine the participant
    nimcp_error_t err = mesh_msp_quarantine(msp_, pid, 100, "test_quarantine");
    ASSERT_EQ(err, NIMCP_OK);

    // Check quarantine status
    bool is_quarantined = false;
    err = mesh_msp_is_quarantined(msp_, pid, &is_quarantined);
    ASSERT_EQ(err, NIMCP_OK);
    EXPECT_TRUE(is_quarantined);

    // Should not be able to access channel while quarantined
    mesh_channel_id_t ch_id = mesh_channel_get_id(channels_[0]);
    bool authorized = true;
    err = mesh_msp_check_channel_access(msp_, pid, ch_id, &authorized);
    EXPECT_FALSE(authorized) << "Quarantined participant should not have access";

    // Verify callback was invoked
    EXPECT_GT(immune_ctx_.quarantine_count.load(), 0);
}

TEST_F(MeshMSPIntegrationTest, QuarantineExpiration) {
    IssueCredential(0);
    GrantChannelAccess(0, 0);

    mesh_participant_id_t pid = mesh_participant_get_id(participants_[0]);

    // Quarantine with short duration (50ms)
    mesh_msp_quarantine(msp_, pid, 50, "short_quarantine");

    // Verify quarantined
    bool is_quarantined = false;
    mesh_msp_is_quarantined(msp_, pid, &is_quarantined);
    EXPECT_TRUE(is_quarantined);

    // Wait for expiration
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Process expiration
    mesh_msp_process_expirations(msp_);

    // Should no longer be quarantined
    mesh_msp_is_quarantined(msp_, pid, &is_quarantined);
    EXPECT_FALSE(is_quarantined) << "Quarantine should have expired";

    // Should have access restored
    mesh_channel_id_t ch_id = mesh_channel_get_id(channels_[0]);
    bool authorized = false;
    mesh_msp_check_channel_access(msp_, pid, ch_id, &authorized);
    EXPECT_TRUE(authorized) << "Access should be restored after quarantine";
}

// =============================================================================
// Revocation Tests
// =============================================================================

TEST_F(MeshMSPIntegrationTest, CredentialRevocation) {
    IssueCredential(0);
    GrantChannelAccess(0, 0);

    mesh_participant_id_t pid = mesh_participant_get_id(participants_[0]);

    // Revoke credential
    nimcp_error_t err = mesh_msp_revoke(msp_, pid, "security_violation");
    ASSERT_EQ(err, NIMCP_OK);

    // Authentication should fail
    bool authenticated = true;
    mesh_msp_authenticate(msp_, pid, &authenticated);
    EXPECT_FALSE(authenticated);

    // Channel access should be denied
    mesh_channel_id_t ch_id = mesh_channel_get_id(channels_[0]);
    bool authorized = true;
    mesh_msp_check_channel_access(msp_, pid, ch_id, &authorized);
    EXPECT_FALSE(authorized);

    // Verify callback
    EXPECT_GT(immune_ctx_.revocation_count.load(), 0);
}

TEST_F(MeshMSPIntegrationTest, AutoRevocationAfterViolations) {
    IssueCredential(0);

    mesh_participant_id_t pid = mesh_participant_get_id(participants_[0]);

    // Report violations (max is 3 before revoke)
    for (int i = 0; i < 3; i++) {
        mesh_msp_report_violation(msp_, pid, "repeated_violation");
    }

    // Should be revoked after max violations
    bool authenticated = true;
    mesh_msp_authenticate(msp_, pid, &authenticated);
    EXPECT_FALSE(authenticated) << "Should be revoked after max violations";
}

// =============================================================================
// Transaction Validation Tests
// =============================================================================

TEST_F(MeshMSPIntegrationTest, TransactionValidationAuthorized) {
    IssueCredential(0);
    GrantChannelAccess(0, 0);

    // Create a transaction
    mesh_transaction_config_t tx_config;
    mesh_transaction_config_init(&tx_config);
    tx_config.type = MESH_TX_TYPE_BELIEF_UPDATE;
    tx_config.channel_id = mesh_channel_get_id(channels_[0]);
    tx_config.proposer_id = mesh_participant_get_id(participants_[0]);
    tx_config.payload = "test";
    tx_config.payload_size = 4;

    mesh_transaction_t* tx = mesh_transaction_create(&tx_config);
    ASSERT_NE(tx, nullptr);

    // Validate transaction
    nimcp_error_t err = mesh_msp_validate_transaction(msp_, tx);
    EXPECT_EQ(err, NIMCP_OK);

    mesh_transaction_destroy(tx);
}

TEST_F(MeshMSPIntegrationTest, TransactionValidationUnauthorized) {
    // Create participant without credential
    mesh_transaction_config_t tx_config;
    mesh_transaction_config_init(&tx_config);
    tx_config.type = MESH_TX_TYPE_BELIEF_UPDATE;
    tx_config.channel_id = mesh_channel_get_id(channels_[0]);
    tx_config.proposer_id = mesh_participant_get_id(participants_[0]);
    tx_config.payload = "test";
    tx_config.payload_size = 4;

    mesh_transaction_t* tx = mesh_transaction_create(&tx_config);
    ASSERT_NE(tx, nullptr);

    // Validation should fail
    nimcp_error_t err = mesh_msp_validate_transaction(msp_, tx);
    EXPECT_NE(err, NIMCP_OK);

    mesh_transaction_destroy(tx);
}

TEST_F(MeshMSPIntegrationTest, TransactionValidationQuarantined) {
    IssueCredential(0);
    GrantChannelAccess(0, 0);

    mesh_participant_id_t pid = mesh_participant_get_id(participants_[0]);

    // Quarantine participant
    mesh_msp_quarantine(msp_, pid, 1000, "test");

    // Create transaction
    mesh_transaction_config_t tx_config;
    mesh_transaction_config_init(&tx_config);
    tx_config.type = MESH_TX_TYPE_BELIEF_UPDATE;
    tx_config.channel_id = mesh_channel_get_id(channels_[0]);
    tx_config.proposer_id = pid;
    tx_config.payload = "test";
    tx_config.payload_size = 4;

    mesh_transaction_t* tx = mesh_transaction_create(&tx_config);
    ASSERT_NE(tx, nullptr);

    // Validation should fail due to quarantine
    nimcp_error_t err = mesh_msp_validate_transaction(msp_, tx);
    EXPECT_NE(err, NIMCP_OK);

    mesh_transaction_destroy(tx);
}

// =============================================================================
// Cross-Channel BBB Tests
// =============================================================================

TEST_F(MeshMSPIntegrationTest, CrossChannelBBBInspection) {
    // Issue credentials and grant access to both channels
    IssueCredential(0);
    GrantChannelAccess(0, 0);
    GrantChannelAccess(0, 1);

    mesh_participant_id_t pid = mesh_participant_get_id(participants_[0]);

    // Create cross-channel transaction
    mesh_transaction_config_t tx_config;
    mesh_transaction_config_init(&tx_config);
    tx_config.type = MESH_TX_TYPE_CROSS_CHANNEL;
    tx_config.channel_id = mesh_channel_get_id(channels_[0]);
    tx_config.target_channel_id = mesh_channel_get_id(channels_[1]);
    tx_config.proposer_id = pid;
    tx_config.payload = "cross_channel_data";
    tx_config.payload_size = 18;
    tx_config.is_cross_channel = true;

    mesh_transaction_t* tx = mesh_transaction_create(&tx_config);
    ASSERT_NE(tx, nullptr);

    // BBB inspection should pass
    nimcp_error_t err = mesh_msp_validate_transaction(msp_, tx);
    EXPECT_EQ(err, NIMCP_OK);

    mesh_transaction_destroy(tx);
}

TEST_F(MeshMSPIntegrationTest, CrossChannelBBBDeniedNoTargetAccess) {
    // Grant access to source channel only
    IssueCredential(0);
    GrantChannelAccess(0, 0);
    // NOT granting access to channel 1

    mesh_participant_id_t pid = mesh_participant_get_id(participants_[0]);

    // Create cross-channel transaction
    mesh_transaction_config_t tx_config;
    mesh_transaction_config_init(&tx_config);
    tx_config.type = MESH_TX_TYPE_CROSS_CHANNEL;
    tx_config.channel_id = mesh_channel_get_id(channels_[0]);
    tx_config.target_channel_id = mesh_channel_get_id(channels_[1]);
    tx_config.proposer_id = pid;
    tx_config.payload = "cross_channel_data";
    tx_config.payload_size = 18;
    tx_config.is_cross_channel = true;

    mesh_transaction_t* tx = mesh_transaction_create(&tx_config);
    ASSERT_NE(tx, nullptr);

    // Should fail - no access to target channel
    nimcp_error_t err = mesh_msp_validate_transaction(msp_, tx);
    EXPECT_NE(err, NIMCP_OK);

    mesh_transaction_destroy(tx);
}

// =============================================================================
// MSP Metrics Tests
// =============================================================================

TEST_F(MeshMSPIntegrationTest, MSPMetrics) {
    // Issue some credentials
    for (size_t i = 0; i < 3; i++) {
        IssueCredential(i);
        GrantChannelAccess(i, 0);
    }

    // Quarantine one
    mesh_msp_quarantine(msp_, mesh_participant_get_id(participants_[0]), 1000, "test");

    // Get metrics
    mesh_msp_metrics_t metrics;
    nimcp_error_t err = mesh_msp_get_metrics(msp_, &metrics);
    ASSERT_EQ(err, NIMCP_OK);

    EXPECT_EQ(metrics.total_credentials, 3u);
    EXPECT_EQ(metrics.active_credentials, 2u);  // One quarantined
    EXPECT_EQ(metrics.quarantined_count, 1u);
    EXPECT_EQ(metrics.registered_channels, NUM_CHANNELS);
}
