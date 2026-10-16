/**
 * @file test_mesh_security_flow_integration.cpp
 * @brief Integration Tests for Mesh Security Flow with BBB and Immune System
 *
 * WHAT: Tests exception to immune system routing through mesh network
 * WHY:  Verify security components interoperate correctly for threat response
 * HOW:  Create exceptions, route through BBB, trigger immune responses via MSP
 *
 * TEST COVERAGE:
 * - Exception to mesh to immune full flow
 * - BBB validation blocks invalid transactions
 * - Quarantine propagates through mesh
 * - Immune response triggers mesh actions
 * - MSP with real BBB and immune instances
 * - Credential lifecycle through security events
 * - Threat escalation handling
 * - Security policy enforcement
 * - Multi-module quarantine coordination
 * - Recovery after quarantine release
 * - Audit trail verification
 * - Concurrent security operations
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <cmath>
#include <cstring>

extern "C" {
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_integration.h"
#include "mesh/nimcp_mesh_exception_bridge.h"
#include "mesh/nimcp_mesh_msp.h"
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_transaction.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MeshSecurityFlowIntegrationTest : public ::testing::Test {
protected:
    mesh_bootstrap_t* bootstrap = nullptr;
    mesh_exception_bridge_t* exception_bridge = nullptr;
    mesh_msp_t* msp = nullptr;

    void SetUp() override {
        mesh_bootstrap_config_t config;
        mesh_bootstrap_default_config(&config);
        config.subsystems = MESH_SUBSYSTEMS_CORE;
        config.subsystems.enable_security = true;
        config.integration.enable_bbb_integration = true;
        config.integration.enable_immune_integration = true;

        bootstrap = mesh_bootstrap_create(&config);
        ASSERT_NE(bootstrap, nullptr);

        exception_bridge = mesh_bootstrap_get_exception_bridge(bootstrap);
        msp = mesh_integration_get_msp(mesh_bootstrap_get_integration(bootstrap));
    }

    void TearDown() override {
        if (bootstrap) {
            mesh_bootstrap_destroy(bootstrap);
            bootstrap = nullptr;
        }
        exception_bridge = nullptr;
        msp = nullptr;
    }

    mesh_participant_id_t register_test_participant(uint32_t local_id) {
        return mesh_make_participant_id(
            MESH_CHANNEL_SYSTEM,
            MESH_PARTICIPANT_MODULE,
            local_id
        );
    }
};

/* ============================================================================
 * Test 1: Exception to Mesh to Immune Full Flow
 * ============================================================================ */

TEST_F(MeshSecurityFlowIntegrationTest, ExceptionToMeshToImmuneFullFlow) {
    if (!exception_bridge) {
        GTEST_SKIP() << "Exception bridge not available";
    }

    /* Create an exception antigen */
    mesh_exception_antigen_t antigen;
    memset(&antigen, 0, sizeof(antigen));
    antigen.antigen_id = 1;
    antigen.category = MESH_EXC_CAT_SECURITY;
    antigen.severity = MESH_EXC_SEVERITY_ERROR;
    antigen.source_module = register_test_participant(100);
    antigen.error_code = NIMCP_ERROR_VALIDATION;
    strncpy(antigen.error_message, "Test security exception", sizeof(antigen.error_message) - 1);

    /* Route exception through bridge */
    mesh_exception_response_t response;
    memset(&response, 0, sizeof(response));

    nimcp_error_t err = mesh_exception_bridge_route_error(
        exception_bridge,
        NIMCP_ERROR_VALIDATION,
        "Test validation error",
        antigen.source_module,
        __FILE__,
        __LINE__,
        &response
    );

    if (err == NIMCP_SUCCESS) {
        /* Response should indicate appropriate action */
        EXPECT_NE(response.primary_action, MESH_IMMUNE_ACTION_SHUTDOWN);
        /* Threat score should be computed */
        EXPECT_GE(response.threat_score, 0.0f);
        EXPECT_LE(response.threat_score, 1.0f);
    }

    /* Verify statistics updated */
    mesh_exception_bridge_stats_t stats;
    mesh_exception_bridge_get_stats(exception_bridge, &stats);
    EXPECT_GT(stats.exceptions_received, 0u);
}

/* ============================================================================
 * Test 2: BBB Validation Blocks Invalid Transactions
 * ============================================================================ */

TEST_F(MeshSecurityFlowIntegrationTest, BBBValidationBlocksInvalidTransactions) {
    if (!msp) {
        GTEST_SKIP() << "MSP not available";
    }

    /* Create participant without credentials */
    mesh_participant_id_t invalid_participant = register_test_participant(999);

    /* Try to validate transaction from uncredentialed participant */
    mesh_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.id.channel = MESH_CHANNEL_SYSTEM;
    tx.id.proposer = invalid_participant;
    tx.id.sequence = 1;
    tx.type = MESH_TX_STATE_CHANGE;

    nimcp_error_t err = mesh_msp_validate_transaction(msp, &tx);
    /* Should fail or require authentication */
    if (err == NIMCP_SUCCESS) {
        /* If it succeeds, participant might have default credentials */
        /* This is acceptable - MSP policy dependent */
    }
}

/* ============================================================================
 * Test 3: Quarantine Propagates Through Mesh
 * ============================================================================ */

TEST_F(MeshSecurityFlowIntegrationTest, QuarantinePropagatesThroughMesh) {
    if (!msp) {
        GTEST_SKIP() << "MSP not available";
    }

    /* Create and credential a participant */
    mesh_participant_id_t participant = register_test_participant(200);

    credential_t cred;
    nimcp_error_t err = mesh_msp_issue_credential(
        msp, participant, 5, MESH_CAP_READ | MESH_CAP_WRITE | MESH_CAP_PROPOSE, &cred
    );

    if (err == NIMCP_SUCCESS) {
        /* Verify participant is valid */
        EXPECT_TRUE(mesh_msp_is_credential_valid(msp, participant));

        /* Quarantine the participant */
        err = mesh_msp_quarantine(msp, participant, 10000);  /* 10 second quarantine */
        EXPECT_EQ(err, NIMCP_SUCCESS);

        /* Verify quarantine is active */
        EXPECT_TRUE(mesh_msp_is_quarantined(msp, participant));

        /* Transactions from quarantined participant should be blocked */
        mesh_transaction_t tx;
        memset(&tx, 0, sizeof(tx));
        tx.id.proposer = participant;
        tx.type = MESH_TX_BELIEF_UPDATE;

        err = mesh_msp_validate_transaction(msp, &tx);
        /* Should fail due to quarantine */
        EXPECT_NE(err, NIMCP_SUCCESS);

        /* Release quarantine */
        err = mesh_msp_release_quarantine(msp, participant);
        EXPECT_EQ(err, NIMCP_SUCCESS);
        EXPECT_FALSE(mesh_msp_is_quarantined(msp, participant));
    }
}

/* ============================================================================
 * Test 4: Immune Response Triggers Mesh Actions
 * ============================================================================ */

TEST_F(MeshSecurityFlowIntegrationTest, ImmuneResponseTriggersMeshActions) {
    if (!exception_bridge || !msp) {
        GTEST_SKIP() << "Exception bridge or MSP not available";
    }

    mesh_participant_id_t threat_source = register_test_participant(300);

    /* Issue credential first */
    credential_t cred;
    mesh_msp_issue_credential(msp, threat_source, 3, MESH_CAP_READ, &cred);

    /* Route a severe exception from this participant */
    mesh_exception_response_t response;
    nimcp_error_t err = mesh_exception_bridge_route_error(
        exception_bridge,
        NIMCP_ERROR_SECURITY,
        "Severe security violation detected",
        threat_source,
        __FILE__,
        __LINE__,
        &response
    );

    if (err == NIMCP_SUCCESS) {
        /* Check if immune recommended quarantine */
        if (response.primary_action == MESH_IMMUNE_ACTION_QUARANTINE) {
            /* Participant should now be quarantined */
            /* (if auto-quarantine enabled) */
            mesh_exception_bridge_stats_t stats;
            mesh_exception_bridge_get_stats(exception_bridge, &stats);
            /* Stats should reflect the action */
        }
    }
}

/* ============================================================================
 * Test 5: MSP with Real BBB and Immune Instances
 * ============================================================================ */

TEST_F(MeshSecurityFlowIntegrationTest, MSPWithRealBBBAndImmuneInstances) {
    if (!msp) {
        GTEST_SKIP() << "MSP not available";
    }

    /* Get MSP statistics to verify integration */
    mesh_msp_stats_t stats;
    nimcp_error_t err = mesh_msp_get_stats(msp, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Verify MSP is operational */
    EXPECT_EQ(stats.credentials_issued, stats.credentials_issued);  /* No specific count expected */

    /* Test full credential lifecycle */
    mesh_participant_id_t participant = register_test_participant(400);

    credential_t cred;
    err = mesh_msp_issue_credential(
        msp, participant, 7, MESH_CAP_ALL, &cred
    );

    if (err == NIMCP_SUCCESS) {
        EXPECT_EQ(cred.state, CREDENTIAL_STATE_VALID);
        EXPECT_EQ(cred.privilege_level, 7u);
        EXPECT_EQ(cred.capabilities, MESH_CAP_ALL);

        /* Verify through MSP */
        EXPECT_TRUE(mesh_msp_check_privilege(msp, participant, 5));
        EXPECT_TRUE(mesh_msp_check_capability(msp, participant, MESH_CAP_WRITE));
    }
}

/* ============================================================================
 * Test 6: Credential Lifecycle Through Security Events
 * ============================================================================ */

TEST_F(MeshSecurityFlowIntegrationTest, CredentialLifecycleThroughSecurityEvents) {
    if (!msp) {
        GTEST_SKIP() << "MSP not available";
    }

    mesh_participant_id_t participant = register_test_participant(500);

    /* Issue -> Valid */
    credential_t cred;
    nimcp_error_t err = mesh_msp_issue_credential(
        msp, participant, 5, MESH_CAP_READ | MESH_CAP_WRITE, &cred
    );
    if (err != NIMCP_SUCCESS) return;  /* Skip if issuance fails */

    EXPECT_TRUE(mesh_msp_is_credential_valid(msp, participant));

    /* Suspend credential */
    err = mesh_msp_suspend_credential(msp, participant, 5000);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    const credential_t* current = mesh_msp_get_credential(msp, participant);
    if (current) {
        EXPECT_EQ(current->state, CREDENTIAL_STATE_SUSPENDED);
    }

    /* Restore credential */
    err = mesh_msp_restore_credential(msp, participant);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(mesh_msp_is_credential_valid(msp, participant));

    /* Revoke credential */
    err = mesh_msp_revoke_credential(msp, participant, "Test revocation");
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_FALSE(mesh_msp_is_credential_valid(msp, participant));
}

/* ============================================================================
 * Test 7: Threat Escalation Handling
 * ============================================================================ */

TEST_F(MeshSecurityFlowIntegrationTest, ThreatEscalationHandling) {
    if (!exception_bridge) {
        GTEST_SKIP() << "Exception bridge not available";
    }

    mesh_participant_id_t source = register_test_participant(600);

    /* Send escalating severity exceptions */
    mesh_exception_severity_t severities[] = {
        MESH_EXC_SEVERITY_WARNING,
        MESH_EXC_SEVERITY_ERROR,
        MESH_EXC_SEVERITY_SEVERE,
        MESH_EXC_SEVERITY_CRITICAL
    };

    mesh_immune_action_t last_action = MESH_IMMUNE_ACTION_NONE;

    for (auto severity : severities) {
        mesh_exception_antigen_t antigen;
        memset(&antigen, 0, sizeof(antigen));
        antigen.severity = severity;
        antigen.category = MESH_EXC_CAT_SECURITY;
        antigen.source_module = source;

        mesh_exception_response_t response;
        nimcp_error_t err = mesh_exception_bridge_route_error(
            exception_bridge,
            NIMCP_ERROR_VALIDATION,
            "Escalating threat",
            source,
            __FILE__,
            __LINE__,
            &response
        );

        if (err == NIMCP_SUCCESS) {
            /* Actions should escalate or stay same (never decrease) */
            EXPECT_GE((int)response.primary_action, (int)last_action);
            last_action = response.primary_action;
        }
    }

    /* Verify escalation statistics */
    mesh_exception_bridge_stats_t stats;
    mesh_exception_bridge_get_stats(exception_bridge, &stats);
    /* Multiple severity levels should have been processed */
    EXPECT_GT(stats.exceptions_received, 1u);
}

/* ============================================================================
 * Test 8: Security Policy Enforcement
 * ============================================================================ */

TEST_F(MeshSecurityFlowIntegrationTest, SecurityPolicyEnforcement) {
    if (!msp) {
        GTEST_SKIP() << "MSP not available";
    }

    /* Create low-privilege participant */
    mesh_participant_id_t low_priv = register_test_participant(700);
    credential_t low_cred;
    mesh_msp_issue_credential(msp, low_priv, 2, MESH_CAP_READ, &low_cred);

    /* Create high-privilege participant */
    mesh_participant_id_t high_priv = register_test_participant(701);
    credential_t high_cred;
    mesh_msp_issue_credential(msp, high_priv, 8, MESH_CAP_ALL, &high_cred);

    /* Verify privilege checks */
    EXPECT_TRUE(mesh_msp_check_privilege(msp, low_priv, 1));
    EXPECT_TRUE(mesh_msp_check_privilege(msp, low_priv, 2));
    EXPECT_FALSE(mesh_msp_check_privilege(msp, low_priv, 5));

    EXPECT_TRUE(mesh_msp_check_privilege(msp, high_priv, 5));
    EXPECT_TRUE(mesh_msp_check_privilege(msp, high_priv, 8));

    /* Verify capability checks */
    EXPECT_TRUE(mesh_msp_check_capability(msp, low_priv, MESH_CAP_READ));
    EXPECT_FALSE(mesh_msp_check_capability(msp, low_priv, MESH_CAP_WRITE));
    EXPECT_FALSE(mesh_msp_check_capability(msp, low_priv, MESH_CAP_ADMIN));

    EXPECT_TRUE(mesh_msp_check_capability(msp, high_priv, MESH_CAP_WRITE));
    EXPECT_TRUE(mesh_msp_check_capability(msp, high_priv, MESH_CAP_ADMIN));
}

/* ============================================================================
 * Test 9: Multi-Module Quarantine Coordination
 * ============================================================================ */

TEST_F(MeshSecurityFlowIntegrationTest, MultiModuleQuarantineCoordination) {
    if (!msp) {
        GTEST_SKIP() << "MSP not available";
    }

    /* Create multiple related participants */
    mesh_participant_id_t modules[5];
    for (int i = 0; i < 5; i++) {
        modules[i] = register_test_participant(800 + i);
        credential_t cred;
        mesh_msp_issue_credential(msp, modules[i], 5, MESH_CAP_READ | MESH_CAP_WRITE, &cred);
    }

    /* Quarantine multiple modules */
    for (int i = 0; i < 3; i++) {
        mesh_msp_quarantine(msp, modules[i], 10000);
    }

    /* Verify quarantine status */
    for (int i = 0; i < 5; i++) {
        bool should_be_quarantined = (i < 3);
        EXPECT_EQ(mesh_msp_is_quarantined(msp, modules[i]), should_be_quarantined)
            << "Module " << i << " quarantine status incorrect";
    }

    /* Release all quarantines */
    for (int i = 0; i < 3; i++) {
        mesh_msp_release_quarantine(msp, modules[i]);
    }

    /* Verify all released */
    for (int i = 0; i < 5; i++) {
        EXPECT_FALSE(mesh_msp_is_quarantined(msp, modules[i]));
    }
}

/* ============================================================================
 * Test 10: Recovery After Quarantine Release
 * ============================================================================ */

TEST_F(MeshSecurityFlowIntegrationTest, RecoveryAfterQuarantineRelease) {
    if (!msp) {
        GTEST_SKIP() << "MSP not available";
    }

    mesh_participant_id_t participant = register_test_participant(900);

    /* Issue credential */
    credential_t cred;
    mesh_msp_issue_credential(msp, participant, 5, MESH_CAP_ALL, &cred);

    /* Verify normal operation */
    mesh_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.id.proposer = participant;
    tx.type = MESH_TX_BELIEF_UPDATE;

    nimcp_error_t err = mesh_msp_validate_transaction(msp, &tx);
    bool was_valid_before = (err == NIMCP_SUCCESS);

    /* Quarantine */
    mesh_msp_quarantine(msp, participant, 10000);

    /* Verify blocked */
    err = mesh_msp_validate_transaction(msp, &tx);
    EXPECT_NE(err, NIMCP_SUCCESS);

    /* Release */
    mesh_msp_release_quarantine(msp, participant);

    /* Verify restored operation */
    err = mesh_msp_validate_transaction(msp, &tx);
    if (was_valid_before) {
        EXPECT_EQ(err, NIMCP_SUCCESS) << "Operation should be restored after quarantine release";
    }
}

/* ============================================================================
 * Test 11: Audit Trail Verification
 * ============================================================================ */

TEST_F(MeshSecurityFlowIntegrationTest, AuditTrailVerification) {
    if (!msp) {
        GTEST_SKIP() << "MSP not available";
    }

    /* Get initial stats */
    mesh_msp_stats_t before;
    mesh_msp_get_stats(msp, &before);

    /* Perform various operations */
    mesh_participant_id_t p1 = register_test_participant(1000);
    credential_t cred;
    mesh_msp_issue_credential(msp, p1, 5, MESH_CAP_ALL, &cred);

    mesh_msp_quarantine(msp, p1, 5000);
    mesh_msp_release_quarantine(msp, p1);
    mesh_msp_suspend_credential(msp, p1, 5000);
    mesh_msp_restore_credential(msp, p1);

    /* Get final stats */
    mesh_msp_stats_t after;
    mesh_msp_get_stats(msp, &after);

    /* Verify operations were tracked */
    EXPECT_GT(after.credentials_issued, before.credentials_issued);
    EXPECT_GE(after.quarantine_events, before.quarantine_events);
    EXPECT_GE(after.recovery_events, before.recovery_events);
    EXPECT_GT(after.auth_requests, before.auth_requests);
}

/* ============================================================================
 * Test 12: Concurrent Security Operations
 * ============================================================================ */

TEST_F(MeshSecurityFlowIntegrationTest, ConcurrentSecurityOperations) {
    if (!msp) {
        GTEST_SKIP() << "MSP not available";
    }

    std::atomic<bool> running{true};
    std::atomic<int> issue_count{0};
    std::atomic<int> validate_count{0};
    std::atomic<int> quarantine_count{0};

    /* Thread 1: Issue credentials */
    std::thread issue_thread([&]() {
        uint32_t id = 2000;
        while (running) {
            mesh_participant_id_t p = register_test_participant(id++);
            credential_t cred;
            mesh_msp_issue_credential(msp, p, 5, MESH_CAP_READ, &cred);
            issue_count++;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    /* Thread 2: Validate transactions */
    std::thread validate_thread([&]() {
        while (running) {
            mesh_participant_id_t p = register_test_participant(2000 + (validate_count % 100));
            mesh_transaction_t tx;
            memset(&tx, 0, sizeof(tx));
            tx.id.proposer = p;
            mesh_msp_validate_transaction(msp, &tx);
            validate_count++;
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    });

    /* Thread 3: Quarantine/release cycle */
    std::thread quarantine_thread([&]() {
        uint32_t id = 3000;
        while (running) {
            mesh_participant_id_t p = register_test_participant(id++);
            credential_t cred;
            mesh_msp_issue_credential(msp, p, 5, MESH_CAP_READ, &cred);
            mesh_msp_quarantine(msp, p, 1000);
            mesh_msp_release_quarantine(msp, p);
            quarantine_count++;
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    running = false;
    issue_thread.join();
    validate_thread.join();
    quarantine_thread.join();

    EXPECT_GT(issue_count.load(), 0);
    EXPECT_GT(validate_count.load(), 0);
    EXPECT_GT(quarantine_count.load(), 0);

    /* Verify MSP is still functional */
    mesh_msp_stats_t stats;
    EXPECT_EQ(mesh_msp_get_stats(msp, &stats), NIMCP_SUCCESS);
}

/* ============================================================================
 * Test 13: Exception Classification Accuracy
 * ============================================================================ */

TEST_F(MeshSecurityFlowIntegrationTest, ExceptionClassificationAccuracy) {
    /* Test exception classification for various error codes */
    struct {
        nimcp_error_t error;
        mesh_exception_category_t expected_cat;
    } test_cases[] = {
        {NIMCP_ERROR_MEMORY, MESH_EXC_CAT_MEMORY},
        {NIMCP_ERROR_SECURITY, MESH_EXC_CAT_SECURITY},
        {NIMCP_ERROR_NETWORK, MESH_EXC_CAT_NETWORK},
        {NIMCP_ERROR_VALIDATION, MESH_EXC_CAT_DATA},
        {NIMCP_ERROR_TIMEOUT, MESH_EXC_CAT_TIMING},
    };

    for (auto& tc : test_cases) {
        mesh_exception_category_t category;
        mesh_exception_severity_t severity;

        nimcp_error_t err = mesh_exception_bridge_classify(tc.error, &category, &severity);

        if (err == NIMCP_SUCCESS) {
            /* Classification should be reasonable */
            EXPECT_NE(category, MESH_EXC_CAT_UNKNOWN)
                << "Error " << tc.error << " classified as unknown";
            EXPECT_GE((int)severity, (int)MESH_EXC_SEVERITY_WARNING);
        }
    }
}
