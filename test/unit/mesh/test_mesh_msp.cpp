/**
 * @file test_mesh_msp.cpp
 * @brief Unit tests for mesh Membership Service Provider module
 *
 * Tests credential management, authentication, authorization,
 * channel membership, quarantine, and policy evaluation.
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "mesh/nimcp_mesh_msp.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_transaction.h"
}

class MeshMspTest : public ::testing::Test {
protected:
    mesh_participant_registry_t* registry;
    mesh_msp_t* msp;

    void SetUp() override {
        registry = mesh_registry_create(nullptr);
        ASSERT_NE(registry, nullptr);
        msp = nullptr;
    }

    void TearDown() override {
        if (msp) {
            mesh_msp_destroy(msp);
            msp = nullptr;
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
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(MeshMspTest, DefaultConfig) {
    mesh_msp_config_t config;
    nimcp_error_t err = mesh_msp_default_config(&config);

    ASSERT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(config.credential_expiration_ms, 0);
    EXPECT_TRUE(config.enable_quarantine);
}

TEST_F(MeshMspTest, DefaultConfigNullParam) {
    nimcp_error_t err = mesh_msp_default_config(nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_INVALID_PARAM);
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(MeshMspTest, CreateWithDefaults) {
    mesh_msp_config_t config;
    mesh_msp_default_config(&config);
    config.msp_name = "test_msp";

    msp = mesh_msp_create(&config, registry);
    ASSERT_NE(msp, nullptr);

    EXPECT_STREQ(mesh_msp_get_name(msp), "test_msp");
}

TEST_F(MeshMspTest, CreateWithNullConfig) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);
}

TEST_F(MeshMspTest, DestroyNull) {
    mesh_msp_destroy(nullptr);
    // Should not crash
}

TEST_F(MeshMspTest, GetNameNull) {
    EXPECT_EQ(mesh_msp_get_name(nullptr), nullptr);
}

/* ============================================================================
 * Credential Management Tests
 * ============================================================================ */

TEST_F(MeshMspTest, IssueCredential) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");

    credential_t cred;
    nimcp_error_t err = mesh_msp_issue_credential(msp, p, 5, MESH_CAP_ALL, &cred);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(cred.participant_id, p);
    EXPECT_EQ(cred.privilege_level, 5);
    EXPECT_EQ(cred.state, CREDENTIAL_STATE_VALID);
}

TEST_F(MeshMspTest, IssueCredentialNoOutput) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");

    nimcp_error_t err = mesh_msp_issue_credential(msp, p, 3, MESH_CAP_READ, nullptr);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_TRUE(mesh_msp_is_credential_valid(msp, p));
}

TEST_F(MeshMspTest, IssueDuplicateCredential) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");

    mesh_msp_issue_credential(msp, p, 3, MESH_CAP_READ, nullptr);
    nimcp_error_t err = mesh_msp_issue_credential(msp, p, 5, MESH_CAP_ALL, nullptr);

    EXPECT_EQ(err, NIMCP_SUCCESS);  // Returns existing
}

TEST_F(MeshMspTest, RevokeCredential) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");
    mesh_msp_issue_credential(msp, p, 3, MESH_CAP_READ, nullptr);

    nimcp_error_t err = mesh_msp_revoke_credential(msp, p, "test_revoke");
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_FALSE(mesh_msp_is_credential_valid(msp, p));
}

TEST_F(MeshMspTest, RevokeNonexistent) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    nimcp_error_t err = mesh_msp_revoke_credential(msp, 0xDEADBEEF, "test");
    EXPECT_EQ(err, NIMCP_ERROR_NOT_FOUND);
}

TEST_F(MeshMspTest, SuspendCredential) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");
    mesh_msp_issue_credential(msp, p, 3, MESH_CAP_READ, nullptr);

    nimcp_error_t err = mesh_msp_suspend_credential(msp, p, 10000);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    const credential_t* cred = mesh_msp_get_credential(msp, p);
    EXPECT_EQ(cred->state, CREDENTIAL_STATE_SUSPENDED);
}

TEST_F(MeshMspTest, RestoreCredential) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");
    mesh_msp_issue_credential(msp, p, 3, MESH_CAP_READ, nullptr);
    mesh_msp_suspend_credential(msp, p, 10000);

    nimcp_error_t err = mesh_msp_restore_credential(msp, p);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_TRUE(mesh_msp_is_credential_valid(msp, p));
}

TEST_F(MeshMspTest, GetCredential) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");
    mesh_msp_issue_credential(msp, p, 7, MESH_CAP_ADMIN, nullptr);

    const credential_t* cred = mesh_msp_get_credential(msp, p);
    ASSERT_NE(cred, nullptr);
    EXPECT_EQ(cred->privilege_level, 7);
}

TEST_F(MeshMspTest, GetCredentialNonexistent) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    const credential_t* cred = mesh_msp_get_credential(msp, 0xDEADBEEF);
    EXPECT_EQ(cred, nullptr);
}

TEST_F(MeshMspTest, RefreshCredential) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");
    mesh_msp_issue_credential(msp, p, 3, MESH_CAP_READ, nullptr);

    nimcp_error_t err = mesh_msp_refresh_credential(msp, p);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

/* ============================================================================
 * Channel Membership Tests
 * ============================================================================ */

TEST_F(MeshMspTest, GrantChannelMembership) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");
    mesh_msp_issue_credential(msp, p, 3, MESH_CAP_READ, nullptr);

    nimcp_error_t err = mesh_msp_grant_channel_membership(msp, p, MESH_CHANNEL_LEFT_HEMISPHERE);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_TRUE(mesh_msp_has_channel_membership(msp, p, MESH_CHANNEL_LEFT_HEMISPHERE));
}

TEST_F(MeshMspTest, RevokeChannelMembership) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");
    mesh_msp_issue_credential(msp, p, 3, MESH_CAP_READ, nullptr);
    mesh_msp_grant_channel_membership(msp, p, MESH_CHANNEL_LEFT_HEMISPHERE);

    nimcp_error_t err = mesh_msp_revoke_channel_membership(msp, p, MESH_CHANNEL_LEFT_HEMISPHERE);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_FALSE(mesh_msp_has_channel_membership(msp, p, MESH_CHANNEL_LEFT_HEMISPHERE));
}

TEST_F(MeshMspTest, MultipleChannelMemberships) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");
    mesh_msp_issue_credential(msp, p, 3, MESH_CAP_ALL, nullptr);

    mesh_msp_grant_channel_membership(msp, p, MESH_CHANNEL_LEFT_HEMISPHERE);
    mesh_msp_grant_channel_membership(msp, p, MESH_CHANNEL_RIGHT_HEMISPHERE);
    mesh_msp_grant_channel_membership(msp, p, MESH_CHANNEL_SUBCORTICAL);

    mesh_channel_id_t channels[10];
    size_t count;
    nimcp_error_t err = mesh_msp_get_channel_memberships(msp, p, channels, 10, &count);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(count, 3);
}

TEST_F(MeshMspTest, HasChannelMembershipFalse) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");
    mesh_msp_issue_credential(msp, p, 3, MESH_CAP_READ, nullptr);

    EXPECT_FALSE(mesh_msp_has_channel_membership(msp, p, MESH_CHANNEL_GPU_COMPUTE));
}

/* ============================================================================
 * Authentication Tests
 * ============================================================================ */

TEST_F(MeshMspTest, AuthenticateSuccess) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");
    mesh_msp_issue_credential(msp, p, 3, MESH_CAP_READ, nullptr);

    nimcp_error_t err = mesh_msp_authenticate(msp, p, nullptr, 0);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshMspTest, AuthenticateNoCredential) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");

    nimcp_error_t err = mesh_msp_authenticate(msp, p, nullptr, 0);
    EXPECT_EQ(err, NIMCP_ERROR_UNAUTHORIZED);
}

TEST_F(MeshMspTest, AuthenticateQuarantined) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");
    mesh_msp_issue_credential(msp, p, 3, MESH_CAP_READ, nullptr);
    mesh_msp_quarantine(msp, p, 100000);

    nimcp_error_t err = mesh_msp_authenticate(msp, p, nullptr, 0);
    EXPECT_EQ(err, NIMCP_ERROR_ACCESS_DENIED);
}

TEST_F(MeshMspTest, ValidateTransaction) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");
    mesh_msp_issue_credential(msp, p, 3, MESH_CAP_PROPOSE, nullptr);

    mesh_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.proposer_id = p;
    tx.source_channel = MESH_CHANNEL_LEFT_HEMISPHERE;
    tx.target_channel = MESH_CHANNEL_LEFT_HEMISPHERE;

    nimcp_error_t err = mesh_msp_validate_transaction(msp, &tx);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshMspTest, CheckCapability) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");
    mesh_msp_issue_credential(msp, p, 3, MESH_CAP_READ | MESH_CAP_WRITE, nullptr);

    EXPECT_TRUE(mesh_msp_check_capability(msp, p, MESH_CAP_READ));
    EXPECT_TRUE(mesh_msp_check_capability(msp, p, MESH_CAP_WRITE));
    EXPECT_FALSE(mesh_msp_check_capability(msp, p, MESH_CAP_ADMIN));
}

TEST_F(MeshMspTest, CheckPrivilege) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");
    mesh_msp_issue_credential(msp, p, 5, MESH_CAP_READ, nullptr);

    EXPECT_TRUE(mesh_msp_check_privilege(msp, p, 3));
    EXPECT_TRUE(mesh_msp_check_privilege(msp, p, 5));
    EXPECT_FALSE(mesh_msp_check_privilege(msp, p, 7));
}

/* ============================================================================
 * Policy Tests
 * ============================================================================ */

TEST_F(MeshMspTest, AddPolicy) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    msp_access_policy_t policy = {
        .policy_name = "test_policy",
        .type = MSP_POLICY_ALLOW_ALL
    };

    nimcp_error_t err = mesh_msp_add_policy(msp, &policy);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshMspTest, AddDuplicatePolicy) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    msp_access_policy_t policy = {
        .policy_name = "test_policy",
        .type = MSP_POLICY_ALLOW_ALL
    };

    mesh_msp_add_policy(msp, &policy);
    nimcp_error_t err = mesh_msp_add_policy(msp, &policy);
    EXPECT_EQ(err, NIMCP_ERROR_ALREADY_EXISTS);
}

TEST_F(MeshMspTest, RemovePolicy) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    msp_access_policy_t policy = {
        .policy_name = "test_policy",
        .type = MSP_POLICY_ALLOW_ALL
    };

    mesh_msp_add_policy(msp, &policy);
    nimcp_error_t err = mesh_msp_remove_policy(msp, "test_policy");
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshMspTest, EvaluatePolicyAllowAll) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    msp_access_policy_t policy = {
        .policy_name = "allow_all",
        .type = MSP_POLICY_ALLOW_ALL
    };
    mesh_msp_add_policy(msp, &policy);

    mesh_participant_id_t p = register_test_participant("module1");
    mesh_msp_issue_credential(msp, p, 1, MESH_CAP_READ, nullptr);

    EXPECT_TRUE(mesh_msp_evaluate_policy(msp, "allow_all", p, nullptr));
}

TEST_F(MeshMspTest, EvaluatePolicyDenyAll) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    msp_access_policy_t policy = {
        .policy_name = "deny_all",
        .type = MSP_POLICY_DENY_ALL
    };
    mesh_msp_add_policy(msp, &policy);

    mesh_participant_id_t p = register_test_participant("module1");
    mesh_msp_issue_credential(msp, p, 10, MESH_CAP_ALL, nullptr);

    EXPECT_FALSE(mesh_msp_evaluate_policy(msp, "deny_all", p, nullptr));
}

TEST_F(MeshMspTest, EvaluatePolicyPrivilegeLevel) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    msp_access_policy_t policy = {
        .policy_name = "min_level_5",
        .type = MSP_POLICY_PRIVILEGE_LEVEL,
        .min_privilege_level = 5
    };
    mesh_msp_add_policy(msp, &policy);

    mesh_participant_id_t p1 = register_test_participant("module1");
    mesh_msp_issue_credential(msp, p1, 3, MESH_CAP_READ, nullptr);

    mesh_participant_id_t p2 = register_test_participant("module2");
    mesh_msp_issue_credential(msp, p2, 7, MESH_CAP_READ, nullptr);

    EXPECT_FALSE(mesh_msp_evaluate_policy(msp, "min_level_5", p1, nullptr));
    EXPECT_TRUE(mesh_msp_evaluate_policy(msp, "min_level_5", p2, nullptr));
}

/* ============================================================================
 * Quarantine Tests
 * ============================================================================ */

TEST_F(MeshMspTest, Quarantine) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");
    mesh_msp_issue_credential(msp, p, 3, MESH_CAP_READ, nullptr);

    nimcp_error_t err = mesh_msp_quarantine(msp, p, 10000);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_TRUE(mesh_msp_is_quarantined(msp, p));
}

TEST_F(MeshMspTest, ReleaseQuarantine) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");
    mesh_msp_issue_credential(msp, p, 3, MESH_CAP_READ, nullptr);
    mesh_msp_quarantine(msp, p, 10000);

    nimcp_error_t err = mesh_msp_release_quarantine(msp, p);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_FALSE(mesh_msp_is_quarantined(msp, p));
}

TEST_F(MeshMspTest, HandleImmuneEventThreat) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");
    mesh_msp_issue_credential(msp, p, 3, MESH_CAP_READ, nullptr);

    nimcp_error_t err = mesh_msp_handle_immune_event(msp, p, 0);  // 0 = threat
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_TRUE(mesh_msp_is_quarantined(msp, p));
}

TEST_F(MeshMspTest, HandleImmuneEventChronicFailure) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");
    mesh_msp_issue_credential(msp, p, 3, MESH_CAP_READ, nullptr);

    nimcp_error_t err = mesh_msp_handle_immune_event(msp, p, 1);  // 1 = chronic failure
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_FALSE(mesh_msp_is_credential_valid(msp, p));
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(MeshMspTest, Update) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    nimcp_error_t err = mesh_msp_update(msp, 10);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(MeshMspTest, GetStats) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    mesh_msp_stats_t stats;
    nimcp_error_t err = mesh_msp_get_stats(msp, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshMspTest, ResetStats) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    mesh_participant_id_t p = register_test_participant("module1");
    mesh_msp_issue_credential(msp, p, 3, MESH_CAP_READ, nullptr);

    mesh_msp_reset_stats(msp);

    mesh_msp_stats_t stats;
    mesh_msp_get_stats(msp, &stats);
    EXPECT_EQ(stats.credentials_issued, 0);
}

/* ============================================================================
 * Utility Tests
 * ============================================================================ */

TEST_F(MeshMspTest, PrintStatus) {
    msp = mesh_msp_create(nullptr, registry);
    ASSERT_NE(msp, nullptr);

    // Should not crash
    mesh_msp_print_status(msp);
}

TEST_F(MeshMspTest, PrintStatusNull) {
    // Should not crash
    mesh_msp_print_status(nullptr);
}

TEST_F(MeshMspTest, PrintCredential) {
    credential_t cred;
    memset(&cred, 0, sizeof(cred));
    cred.participant_id = 123;
    cred.state = CREDENTIAL_STATE_VALID;
    cred.privilege_level = 5;

    // Should not crash
    mesh_msp_print_credential(&cred);
}
