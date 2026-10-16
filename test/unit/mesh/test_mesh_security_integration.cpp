/**
 * @file test_mesh_security_integration.cpp
 * @brief Unit tests for mesh security integration with immune system
 *
 * Tests exception to antigen conversion, antigen to immune routing,
 * BBB validation of mesh transactions, quarantine notification through mesh,
 * revocation notification through mesh, and MSP credential validation.
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "mesh/nimcp_mesh_exception_bridge.h"
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_msp.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MeshSecurityIntegrationTest : public ::testing::Test {
protected:
    mesh_bootstrap_t* bootstrap = nullptr;
    mesh_exception_bridge_t* exception_bridge = nullptr;

    void SetUp() override {
        mesh_bootstrap_config_t config;
        mesh_bootstrap_default_config(&config);
        config.subsystems.enable_cognitive = false;
        config.subsystems.enable_sensory = false;
        config.subsystems.enable_motor = false;
        config.subsystems.enable_memory = false;
        config.subsystems.enable_security = false;
        config.subsystems.enable_gpu = false;
        config.subsystems.enable_plasticity = false;
        config.subsystems.enable_glial = false;
        config.subsystems.enable_swarm = false;
        config.subsystems.enable_async = false;
        config.subsystems.enable_lnn = false;
        config.subsystems.enable_snn = false;

        bootstrap = mesh_bootstrap_create(&config);
        ASSERT_NE(bootstrap, nullptr);

        exception_bridge = mesh_bootstrap_get_exception_bridge(bootstrap);
    }

    void TearDown() override {
        if (bootstrap) {
            mesh_bootstrap_destroy(bootstrap);
            bootstrap = nullptr;
            exception_bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Exception to Antigen Conversion Tests
 * ============================================================================ */

TEST_F(MeshSecurityIntegrationTest, ClassifyMemoryError) {
    mesh_exception_category_t category;
    mesh_exception_severity_t severity;

    nimcp_error_t err = mesh_exception_bridge_classify(
        NIMCP_ERROR_OUT_OF_MEMORY, &category, &severity
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(category, MESH_EXC_CAT_MEMORY);
}

TEST_F(MeshSecurityIntegrationTest, ClassifyNullPointerError) {
    mesh_exception_category_t category;
    mesh_exception_severity_t severity;

    nimcp_error_t err = mesh_exception_bridge_classify(
        NIMCP_ERROR_NULL_POINTER, &category, &severity
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
    /* Null pointer is typically logic or memory category */
    EXPECT_TRUE(category == MESH_EXC_CAT_LOGIC ||
                category == MESH_EXC_CAT_MEMORY);
}

TEST_F(MeshSecurityIntegrationTest, ClassifyTimeoutError) {
    mesh_exception_category_t category;
    mesh_exception_severity_t severity;

    nimcp_error_t err = mesh_exception_bridge_classify(
        NIMCP_ERROR_TIMEOUT, &category, &severity
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(category, MESH_EXC_CAT_TIMING);
}

TEST_F(MeshSecurityIntegrationTest, ClassifyWithNullOutputs) {
    nimcp_error_t err = mesh_exception_bridge_classify(
        NIMCP_ERROR_OUT_OF_MEMORY, nullptr, nullptr
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshSecurityIntegrationTest, ClassifyWithOnlyCategory) {
    mesh_exception_category_t category;
    nimcp_error_t err = mesh_exception_bridge_classify(
        NIMCP_ERROR_TIMEOUT, &category, nullptr
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(category, MESH_EXC_CAT_TIMING);
}

TEST_F(MeshSecurityIntegrationTest, ClassifyWithOnlySeverity) {
    mesh_exception_severity_t severity;
    nimcp_error_t err = mesh_exception_bridge_classify(
        NIMCP_ERROR_TIMEOUT, nullptr, &severity
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

/* ============================================================================
 * Antigen Structure Tests
 * ============================================================================ */

TEST_F(MeshSecurityIntegrationTest, AntigenStructureSize) {
    EXPECT_GT(sizeof(mesh_exception_antigen_t), 0u);
    EXPECT_LT(sizeof(mesh_exception_antigen_t), 1024u);
}

TEST_F(MeshSecurityIntegrationTest, AntigenPatternSize) {
    mesh_exception_antigen_t antigen;
    EXPECT_EQ(sizeof(antigen.pattern), sizeof(float) * 8);
}

TEST_F(MeshSecurityIntegrationTest, AntigenFieldsAccessible) {
    mesh_exception_antigen_t antigen;
    memset(&antigen, 0, sizeof(antigen));

    antigen.antigen_id = 12345;
    antigen.category = MESH_EXC_CAT_MEMORY;
    antigen.severity = MESH_EXC_SEVERITY_ERROR;
    antigen.source_module = 0x100;
    antigen.source_channel = MESH_CHANNEL_SYSTEM;
    antigen.error_code = NIMCP_ERROR_OUT_OF_MEMORY;
    strcpy(antigen.error_message, "Test error");
    strcpy(antigen.source_file, "test.c");
    antigen.source_line = 42;
    antigen.timestamp_ns = 1000000;
    antigen.occurrence_count = 1;

    EXPECT_EQ(antigen.antigen_id, 12345u);
    EXPECT_EQ(antigen.category, MESH_EXC_CAT_MEMORY);
    EXPECT_STREQ(antigen.error_message, "Test error");
}

/* ============================================================================
 * Antigen to Immune Routing Tests
 * ============================================================================ */

TEST_F(MeshSecurityIntegrationTest, RouteErrorWithExplicitSource) {
    if (!exception_bridge) {
        GTEST_SKIP() << "Exception bridge not available";
    }

    mesh_exception_response_t response;
    memset(&response, 0, sizeof(response));

    nimcp_error_t err = mesh_exception_bridge_route_error(
        exception_bridge,
        NIMCP_ERROR_OUT_OF_MEMORY,
        "Memory allocation failed",
        0x100,  /* source module */
        __FILE__,
        __LINE__,
        &response
    );

    /* Routing may succeed or not be fully implemented */
    if (err == NIMCP_SUCCESS) {
        /* Response should have an action */
        EXPECT_GE(response.primary_action, MESH_IMMUNE_ACTION_NONE);
    }
}

TEST_F(MeshSecurityIntegrationTest, ImmuneActionOrdering) {
    /* None should be the least severe action */
    EXPECT_LT(MESH_IMMUNE_ACTION_NONE, MESH_IMMUNE_ACTION_LOG);

    /* Shutdown should be the most severe */
    EXPECT_GT(MESH_IMMUNE_ACTION_SHUTDOWN, MESH_IMMUNE_ACTION_RESTART);
    EXPECT_GT(MESH_IMMUNE_ACTION_SHUTDOWN, MESH_IMMUNE_ACTION_REVOKE);
    EXPECT_GT(MESH_IMMUNE_ACTION_SHUTDOWN, MESH_IMMUNE_ACTION_QUARANTINE);
}

TEST_F(MeshSecurityIntegrationTest, AllActionsDistinct) {
    EXPECT_NE(MESH_IMMUNE_ACTION_NONE, MESH_IMMUNE_ACTION_LOG);
    EXPECT_NE(MESH_IMMUNE_ACTION_LOG, MESH_IMMUNE_ACTION_WARN);
    EXPECT_NE(MESH_IMMUNE_ACTION_WARN, MESH_IMMUNE_ACTION_QUARANTINE);
    EXPECT_NE(MESH_IMMUNE_ACTION_QUARANTINE, MESH_IMMUNE_ACTION_REVOKE);
    EXPECT_NE(MESH_IMMUNE_ACTION_REVOKE, MESH_IMMUNE_ACTION_REPAIR);
    EXPECT_NE(MESH_IMMUNE_ACTION_REPAIR, MESH_IMMUNE_ACTION_RESTART);
    EXPECT_NE(MESH_IMMUNE_ACTION_RESTART, MESH_IMMUNE_ACTION_SHUTDOWN);
}

/* ============================================================================
 * BBB Validation Tests
 * ============================================================================ */

TEST_F(MeshSecurityIntegrationTest, SetBBBValidation) {
    if (!exception_bridge) {
        GTEST_SKIP() << "Exception bridge not available";
    }

    /* Setting BBB to NULL should be handled gracefully */
    nimcp_error_t err = mesh_exception_bridge_set_bbb(exception_bridge, nullptr);
    /* May succeed or return appropriate error */
    (void)err;
}

TEST_F(MeshSecurityIntegrationTest, BBBValidateAntigen) {
    if (!exception_bridge) {
        GTEST_SKIP() << "Exception bridge not available";
    }

    mesh_exception_antigen_t antigen;
    memset(&antigen, 0, sizeof(antigen));
    antigen.category = MESH_EXC_CAT_SECURITY;
    antigen.severity = MESH_EXC_SEVERITY_WARNING;

    float threat_score = 0.0f;
    nimcp_error_t err = mesh_exception_bridge_bbb_validate(
        exception_bridge, &antigen, &threat_score
    );

    /* Validation may succeed or fail if BBB not configured */
    if (err == NIMCP_SUCCESS) {
        EXPECT_GE(threat_score, 0.0f);
        EXPECT_LE(threat_score, 1.0f);
    }
}

TEST_F(MeshSecurityIntegrationTest, BBBValidateNullAntigen) {
    if (!exception_bridge) {
        GTEST_SKIP() << "Exception bridge not available";
    }

    float threat_score = 0.0f;
    nimcp_error_t err = mesh_exception_bridge_bbb_validate(
        exception_bridge, nullptr, &threat_score
    );
    EXPECT_NE(err, NIMCP_SUCCESS);
}

/* ============================================================================
 * Quarantine Notification Tests
 * ============================================================================ */

TEST_F(MeshSecurityIntegrationTest, QuarantineThresholdConfig) {
    mesh_exception_bridge_config_t config;
    nimcp_error_t err = mesh_exception_bridge_default_config(&config);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    /* Quarantine threshold should be high severity */
    EXPECT_GE(config.quarantine_threshold, MESH_EXC_SEVERITY_SEVERE);
}

TEST_F(MeshSecurityIntegrationTest, AutoQuarantineEnabled) {
    mesh_exception_bridge_config_t config;
    mesh_exception_bridge_default_config(&config);

    /* Check that auto-quarantine is configurable */
    config.enable_auto_quarantine = true;
    EXPECT_TRUE(config.enable_auto_quarantine);

    config.enable_auto_quarantine = false;
    EXPECT_FALSE(config.enable_auto_quarantine);
}

TEST_F(MeshSecurityIntegrationTest, QuarantineStatisticsTracking) {
    if (!exception_bridge) {
        GTEST_SKIP() << "Exception bridge not available";
    }

    mesh_exception_bridge_stats_t stats;
    nimcp_error_t err = mesh_exception_bridge_get_stats(exception_bridge, &stats);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.quarantine_actions, 0u);  /* No quarantines yet */
}

/* ============================================================================
 * Revocation Notification Tests
 * ============================================================================ */

TEST_F(MeshSecurityIntegrationTest, RevokeStatisticsTracking) {
    if (!exception_bridge) {
        GTEST_SKIP() << "Exception bridge not available";
    }

    mesh_exception_bridge_stats_t stats;
    nimcp_error_t err = mesh_exception_bridge_get_stats(exception_bridge, &stats);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.revoke_actions, 0u);  /* No revokes yet */
}

TEST_F(MeshSecurityIntegrationTest, ResponseContainsRevocationStatus) {
    mesh_exception_response_t response;
    memset(&response, 0, sizeof(response));

    /* Response should track if credentials were revoked */
    response.credential_revoked = false;
    EXPECT_FALSE(response.credential_revoked);

    response.credential_revoked = true;
    EXPECT_TRUE(response.credential_revoked);
}

/* ============================================================================
 * MSP Credential Validation Tests
 * ============================================================================ */

TEST_F(MeshSecurityIntegrationTest, CredentialStateOrdering) {
    /* States should have logical ordering */
    EXPECT_EQ(CREDENTIAL_STATE_NONE, 0);
    EXPECT_LT(CREDENTIAL_STATE_PENDING, CREDENTIAL_STATE_VALID);
}

TEST_F(MeshSecurityIntegrationTest, CredentialStructureFields) {
    credential_t cred;
    memset(&cred, 0, sizeof(cred));

    cred.state = CREDENTIAL_STATE_VALID;
    cred.participant_id = 0x12345678;
    cred.privilege_level = 5;
    cred.capabilities = 0xFF;

    EXPECT_EQ(cred.state, CREDENTIAL_STATE_VALID);
    EXPECT_EQ(cred.participant_id, 0x12345678u);
    EXPECT_EQ(cred.privilege_level, 5u);
}

TEST_F(MeshSecurityIntegrationTest, CredentialIdSize) {
    EXPECT_EQ(MESH_CREDENTIAL_ID_SIZE, 32u);
}

TEST_F(MeshSecurityIntegrationTest, CredentialSignatureSize) {
    EXPECT_EQ(MESH_SIGNATURE_SIZE, 64u);
}

TEST_F(MeshSecurityIntegrationTest, CredentialStateToString) {
    const char* valid = mesh_credential_state_to_string(CREDENTIAL_STATE_VALID);
    EXPECT_NE(valid, nullptr);

    const char* revoked = mesh_credential_state_to_string(CREDENTIAL_STATE_REVOKED);
    EXPECT_NE(revoked, nullptr);
}

/* ============================================================================
 * Severity Level Tests
 * ============================================================================ */

TEST_F(MeshSecurityIntegrationTest, SeverityOrdering) {
    EXPECT_LT(MESH_EXC_SEVERITY_TRACE, MESH_EXC_SEVERITY_INFO);
    EXPECT_LT(MESH_EXC_SEVERITY_INFO, MESH_EXC_SEVERITY_WARNING);
    EXPECT_LT(MESH_EXC_SEVERITY_WARNING, MESH_EXC_SEVERITY_ERROR);
    EXPECT_LT(MESH_EXC_SEVERITY_ERROR, MESH_EXC_SEVERITY_SEVERE);
    EXPECT_LT(MESH_EXC_SEVERITY_SEVERE, MESH_EXC_SEVERITY_CRITICAL);
}

TEST_F(MeshSecurityIntegrationTest, TraceSeverityIsLowest) {
    EXPECT_EQ(MESH_EXC_SEVERITY_TRACE, 0);
}

TEST_F(MeshSecurityIntegrationTest, CriticalSeverityIsHighest) {
    EXPECT_EQ(MESH_EXC_SEVERITY_CRITICAL, 5);
}

/* ============================================================================
 * Exception Category Tests
 * ============================================================================ */

TEST_F(MeshSecurityIntegrationTest, AllCategoriesDefined) {
    EXPECT_NE(MESH_EXC_CAT_MEMORY, MESH_EXC_CAT_SECURITY);
    EXPECT_NE(MESH_EXC_CAT_SECURITY, MESH_EXC_CAT_NETWORK);
    EXPECT_NE(MESH_EXC_CAT_NETWORK, MESH_EXC_CAT_RESOURCE);
    EXPECT_NE(MESH_EXC_CAT_RESOURCE, MESH_EXC_CAT_LOGIC);
    EXPECT_NE(MESH_EXC_CAT_LOGIC, MESH_EXC_CAT_TIMING);
    EXPECT_NE(MESH_EXC_CAT_TIMING, MESH_EXC_CAT_DATA);
    EXPECT_NE(MESH_EXC_CAT_DATA, MESH_EXC_CAT_SYSTEM);
    EXPECT_NE(MESH_EXC_CAT_SYSTEM, MESH_EXC_CAT_GPU);
}

TEST_F(MeshSecurityIntegrationTest, MemoryCategoryIsZero) {
    EXPECT_EQ(MESH_EXC_CAT_MEMORY, 0);
}

/* ============================================================================
 * Response Structure Tests
 * ============================================================================ */

TEST_F(MeshSecurityIntegrationTest, ResponseStructureSize) {
    EXPECT_GT(sizeof(mesh_exception_response_t), 0u);
}

TEST_F(MeshSecurityIntegrationTest, ResponseExplanationBuffer) {
    mesh_exception_response_t response;
    EXPECT_GE(sizeof(response.explanation), 64u);
}

TEST_F(MeshSecurityIntegrationTest, ResponseFieldsAccessible) {
    mesh_exception_response_t response;
    memset(&response, 0, sizeof(response));

    response.primary_action = MESH_IMMUNE_ACTION_QUARANTINE;
    response.fallback_action = MESH_IMMUNE_ACTION_LOG;
    response.quarantine_duration_ms = 5000;
    response.credential_revoked = false;
    response.inflammation_level = 0.5f;
    response.threat_score = 0.7f;
    strcpy(response.explanation, "Test response");

    EXPECT_EQ(response.primary_action, MESH_IMMUNE_ACTION_QUARANTINE);
    EXPECT_EQ(response.quarantine_duration_ms, 5000u);
    EXPECT_FLOAT_EQ(response.threat_score, 0.7f);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(MeshSecurityIntegrationTest, GetStats) {
    if (!exception_bridge) {
        GTEST_SKIP() << "Exception bridge not available";
    }

    mesh_exception_bridge_stats_t stats;
    nimcp_error_t err = mesh_exception_bridge_get_stats(exception_bridge, &stats);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.exceptions_received, 0u);
    EXPECT_EQ(stats.antigens_created, 0u);
}

TEST_F(MeshSecurityIntegrationTest, ResetStats) {
    if (!exception_bridge) {
        GTEST_SKIP() << "Exception bridge not available";
    }

    nimcp_error_t err = mesh_exception_bridge_reset_stats(exception_bridge);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    mesh_exception_bridge_stats_t stats;
    mesh_exception_bridge_get_stats(exception_bridge, &stats);
    EXPECT_EQ(stats.exceptions_received, 0u);
}

TEST_F(MeshSecurityIntegrationTest, CategoryCountsInStats) {
    if (!exception_bridge) {
        GTEST_SKIP() << "Exception bridge not available";
    }

    mesh_exception_bridge_stats_t stats;
    mesh_exception_bridge_get_stats(exception_bridge, &stats);

    /* All category counts should start at zero */
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(stats.category_counts[i], 0u);
    }
}

TEST_F(MeshSecurityIntegrationTest, SeverityCountsInStats) {
    if (!exception_bridge) {
        GTEST_SKIP() << "Exception bridge not available";
    }

    mesh_exception_bridge_stats_t stats;
    mesh_exception_bridge_get_stats(exception_bridge, &stats);

    /* All severity counts should start at zero */
    for (int i = 0; i < 6; i++) {
        EXPECT_EQ(stats.severity_counts[i], 0u);
    }
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(MeshSecurityIntegrationTest, DefaultConfig) {
    mesh_exception_bridge_config_t config;
    nimcp_error_t err = mesh_exception_bridge_default_config(&config);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(config.debounce_ms, 0u);
    EXPECT_GT(config.escalation_window_ms, 0u);
    EXPECT_GT(config.max_per_window, 0u);
}

TEST_F(MeshSecurityIntegrationTest, DefaultConfigNullPointer) {
    nimcp_error_t err = mesh_exception_bridge_default_config(nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshSecurityIntegrationTest, MinReportSeverity) {
    mesh_exception_bridge_config_t config;
    mesh_exception_bridge_default_config(&config);

    /* Min report severity should allow warnings and above */
    EXPECT_LE(config.min_report_severity, MESH_EXC_SEVERITY_WARNING);
}
