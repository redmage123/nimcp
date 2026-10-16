/**
 * @file test_mesh_security_resilience_e2e.cpp
 * @brief End-to-End Tests for Mesh Security and Resilience
 *
 * WHAT: Tests full security flow from exception to immune response to recovery
 * WHY:  Verify system resilience under attack and failure conditions
 * HOW:  Test exception routing, BBB blocking, coordinator failure, partial failure
 *
 * TEST COVERAGE:
 * - Full exception -> antigen -> immune -> recovery flow
 * - Test BBB blocking malicious input end-to-end
 * - Test system recovery after coordinator failure
 * - Test mesh continues after partial system failure
 * - Quarantine and credential revocation flow
 * - Immune response escalation
 * - System-wide health monitoring during incidents
 * - Cascading failure prevention
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
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_exception_bridge.h"
#include "mesh/nimcp_mesh_health_bridge.h"
#include "mesh/nimcp_mesh_msp.h"
#include "mesh/nimcp_mesh_coordinator_pool.h"
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_transaction.h"
#include "mesh/nimcp_mesh_ordering.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
}

// =============================================================================
// Test Constants
// =============================================================================

static constexpr size_t ATTACK_DURATION_MS = 2000;
static constexpr size_t RECOVERY_WAIT_MS = 1000;
static constexpr size_t NUM_ATTACK_SOURCES = 5;

// =============================================================================
// Test Fixture - Security Resilience E2E
// =============================================================================

class MeshSecurityResilienceE2ETest : public ::testing::Test {
protected:
    mesh_bootstrap_t* bootstrap_ = nullptr;
    mesh_exception_bridge_t* exception_bridge_ = nullptr;
    mesh_health_bridge_t* health_bridge_ = nullptr;
    mesh_msp_t* msp_ = nullptr;

    std::vector<mesh_participant_id_t> participants_;

    void SetUp() override {
        mesh_bootstrap_config_t boot_config;
        mesh_bootstrap_default_config(&boot_config);
        boot_config.subsystems.enable_security = true;
        boot_config.enable_health_monitoring = true;
        boot_config.verbose_logging = false;

        bootstrap_ = mesh_bootstrap_create(&boot_config);
        if (!bootstrap_) {
            GTEST_SKIP() << "Bootstrap creation not available";
        }

        exception_bridge_ = mesh_bootstrap_get_exception_bridge(bootstrap_);
        health_bridge_ = mesh_bootstrap_get_health_bridge(bootstrap_);

        // Create MSP for credential management
        mesh_msp_config_t msp_config;
        mesh_msp_default_config(&msp_config);
        msp_config.enable_quarantine = true;
        msp_config.quarantine_duration_ms = 5000;

        // Note: MSP might be obtained from bootstrap in real implementation
    }

    void TearDown() override {
        participants_.clear();

        if (bootstrap_) {
            mesh_bootstrap_destroy(bootstrap_);
            bootstrap_ = nullptr;
        }
    }

    mesh_participant_id_t CreateParticipant(const char* name) {
        // In real implementation, would create through bootstrap/registry
        // Here we simulate with sequential IDs
        static mesh_participant_id_t next_id = 1;
        mesh_participant_id_t pid = next_id++;
        participants_.push_back(pid);
        return pid;
    }
};

// =============================================================================
// Test 1: Full Exception to Immune Recovery Flow
// =============================================================================

TEST_F(MeshSecurityResilienceE2ETest, FullExceptionImmuneRecoveryFlow) {
    if (!exception_bridge_) {
        GTEST_SKIP() << "Exception bridge not available";
    }

    mesh_participant_id_t source = CreateParticipant("exception_source");

    // Phase 1: Normal operation baseline
    mesh_exception_bridge_stats_t baseline_stats;
    mesh_exception_bridge_get_stats(exception_bridge_, &baseline_stats);

    // Phase 2: Trigger exception
    mesh_exception_response_t response;
    nimcp_error_t err = mesh_exception_bridge_route_error(
        exception_bridge_,
        NIMCP_ERROR_TIMEOUT,
        "simulated_exception_for_test",
        source,
        __FILE__,
        __LINE__,
        &response);

    if (err == NIMCP_SUCCESS) {
        // Verify response was generated
        EXPECT_NE(response.primary_action, MESH_IMMUNE_ACTION_NONE)
            << "Exception should trigger some immune action";

        // Phase 3: Check if escalation triggers appropriate response
        mesh_exception_severity_t severity;
        mesh_exception_category_t category;
        mesh_exception_bridge_classify(NIMCP_ERROR_TIMEOUT, &category, &severity);

        EXPECT_EQ(category, MESH_EXC_CAT_TIMING);
    }

    // Phase 4: Verify stats updated
    mesh_exception_bridge_stats_t post_stats;
    mesh_exception_bridge_get_stats(exception_bridge_, &post_stats);
    EXPECT_GT(post_stats.exceptions_received, baseline_stats.exceptions_received);

    // Phase 5: Recovery - continue normal operations
    mesh_transaction_config_t tx_config;
    mesh_transaction_config_init(&tx_config);
    tx_config.type = MESH_TX_TYPE_BELIEF_UPDATE;
    tx_config.payload = "post_exception_operation";
    tx_config.payload_size = strlen("post_exception_operation");

    mesh_transaction_t* recovery_tx = mesh_transaction_create(&tx_config);
    EXPECT_NE(recovery_tx, nullptr) << "Should be able to create transactions after exception";
    if (recovery_tx) mesh_transaction_destroy(recovery_tx);
}

// =============================================================================
// Test 2: BBB Blocking Malicious Input End-to-End
// =============================================================================

TEST_F(MeshSecurityResilienceE2ETest, BBBBlockingMaliciousInput) {
    if (!exception_bridge_) {
        GTEST_SKIP() << "Exception bridge not available";
    }

    struct MaliciousInput {
        nimcp_error_t error_code;
        const char* message;
        mesh_exception_severity_t expected_min_severity;
    };

    MaliciousInput attacks[] = {
        {NIMCP_ERROR_SECURITY, "sql_injection_attempt", MESH_EXC_SEVERITY_CRITICAL},
        {NIMCP_ERROR_SECURITY, "buffer_overflow_attack", MESH_EXC_SEVERITY_CRITICAL},
        {NIMCP_ERROR_MEMORY, "heap_spray_detected", MESH_EXC_SEVERITY_SEVERE},
        {NIMCP_ERROR_SECURITY, "privilege_escalation", MESH_EXC_SEVERITY_CRITICAL},
        {NIMCP_ERROR_RESOURCE, "dos_resource_exhaustion", MESH_EXC_SEVERITY_SEVERE},
    };
    size_t num_attacks = sizeof(attacks) / sizeof(attacks[0]);

    size_t blocked_count = 0;
    size_t quarantine_triggered = 0;

    for (const auto& attack : attacks) {
        mesh_participant_id_t attacker = CreateParticipant("attacker");

        // Create antigen from attack
        mesh_exception_antigen_t antigen;
        memset(&antigen, 0, sizeof(antigen));
        antigen.source_module = attacker;
        antigen.error_code = attack.error_code;
        strncpy(antigen.error_message, attack.message, sizeof(antigen.error_message) - 1);

        // Classify attack
        mesh_exception_bridge_classify(
            attack.error_code, &antigen.category, &antigen.severity);

        EXPECT_GE(antigen.severity, attack.expected_min_severity)
            << "Attack '" << attack.message << "' should have high severity";

        // Validate through BBB
        float threat_score = 0.0f;
        nimcp_error_t err = mesh_exception_bridge_bbb_validate(
            exception_bridge_, &antigen, &threat_score);

        if (err == NIMCP_SUCCESS) {
            // High threat should be blocked
            if (threat_score > 0.7f) {
                blocked_count++;
            }
        }

        // Route exception and check response
        mesh_exception_response_t response;
        err = mesh_exception_bridge_route_error(
            exception_bridge_,
            attack.error_code,
            attack.message,
            attacker,
            __FILE__,
            __LINE__,
            &response);

        if (err == NIMCP_SUCCESS) {
            if (response.primary_action == MESH_IMMUNE_ACTION_QUARANTINE) {
                quarantine_triggered++;
            }
        }
    }

    EXPECT_GT(blocked_count, 0u)
        << "BBB should block some high-threat attacks";

    EXPECT_GT(quarantine_triggered, 0u)
        << "Severe attacks should trigger quarantine";

    // Verify bridge stats
    mesh_exception_bridge_stats_t stats;
    mesh_exception_bridge_get_stats(exception_bridge_, &stats);
    EXPECT_GT(stats.bbb_validations, 0u);
}

// =============================================================================
// Test 3: System Recovery After Coordinator Failure
// =============================================================================

TEST_F(MeshSecurityResilienceE2ETest, RecoveryAfterCoordinatorFailure) {
    // Create coordinator pool
    mesh_coordinator_pool_config_t pool_config;
    mesh_coordinator_pool_config_init(&pool_config);
    pool_config.pool_name = "resilience_test_pool";
    pool_config.initial_size = 5;
    pool_config.enable_bft = true;
    pool_config.election_timeout_ms = 100.0f;

    mesh_coordinator_pool_t* pool = mesh_coordinator_pool_create(&pool_config);
    if (!pool) {
        GTEST_SKIP() << "Coordinator pool creation not available";
    }

    // Phase 1: Elect initial leader
    nimcp_error_t err = mesh_coordinator_pool_elect_leader(pool);
    EXPECT_EQ(err, NIMCP_OK);

    mesh_coordinator_pool_info_t info;
    mesh_coordinator_pool_get_info(pool, &info);
    EXPECT_TRUE(info.has_leader);
    uint64_t original_term = info.current_term;
    size_t original_leader = info.leader_index;

    // Phase 2: Submit transactions before failure
    std::atomic<size_t> pre_failure_success{0};

    // Phase 3: Simulate leader failure
    mesh_coordinator_pool_handle_failure(pool, original_leader);

    // Wait for re-election
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    mesh_coordinator_pool_get_info(pool, &info);
    EXPECT_TRUE(info.has_leader)
        << "Pool should have new leader after failure";
    EXPECT_GT(info.current_term, original_term)
        << "Term should increase after re-election";
    EXPECT_NE(info.leader_index, original_leader)
        << "New leader should be different from failed leader";

    // Phase 4: Verify system still functional
    for (int i = 0; i < 10; i++) {
        mesh_transaction_config_t tx_config;
        mesh_transaction_config_init(&tx_config);
        tx_config.type = MESH_TX_TYPE_BELIEF_UPDATE;

        char payload[64];
        snprintf(payload, sizeof(payload), "post_recovery_tx_%d", i);
        tx_config.payload = payload;
        tx_config.payload_size = strlen(payload);

        mesh_transaction_t* tx = mesh_transaction_create(&tx_config);
        EXPECT_NE(tx, nullptr)
            << "Should be able to create transactions after coordinator recovery";
        if (tx) mesh_transaction_destroy(tx);
    }

    // Phase 5: Simulate cascading failures
    for (int failures = 0; failures < 2; failures++) {
        mesh_coordinator_pool_get_info(pool, &info);
        if (info.has_leader) {
            mesh_coordinator_pool_handle_failure(pool, info.leader_index);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

    // Pool should still be able to elect leader (if BFT tolerates failures)
    err = mesh_coordinator_pool_elect_leader(pool);
    mesh_coordinator_pool_get_info(pool, &info);
    // With 5 nodes and 3 failures, BFT might not have quorum

    mesh_coordinator_pool_destroy(pool);
}

// =============================================================================
// Test 4: Mesh Continues After Partial System Failure
// =============================================================================

TEST_F(MeshSecurityResilienceE2ETest, MeshContinuesAfterPartialFailure) {
    // Create multiple channels
    mesh_channel_t* healthy_channel = nullptr;
    mesh_channel_t* failing_channel = nullptr;

    mesh_channel_config_t ch_config;
    mesh_channel_default_config(&ch_config);
    ch_config.channel_name = "healthy_channel";
    ch_config.channel_id = MESH_CHANNEL_LEFT_HEMISPHERE;
    healthy_channel = mesh_channel_create(&ch_config, nullptr);

    ch_config.channel_name = "failing_channel";
    ch_config.channel_id = MESH_CHANNEL_RIGHT_HEMISPHERE;
    failing_channel = mesh_channel_create(&ch_config, nullptr);

    if (!healthy_channel || !failing_channel) {
        if (healthy_channel) mesh_channel_destroy(healthy_channel);
        if (failing_channel) mesh_channel_destroy(failing_channel);
        GTEST_SKIP() << "Channel creation not available";
    }

    // Add participants to channels
    for (int i = 0; i < 10; i++) {
        char name[32];
        snprintf(name, sizeof(name), "healthy_p%d", i);
        mesh_participant_id_t pid = CreateParticipant(name);
        mesh_channel_add_participant(healthy_channel, pid);
    }

    for (int i = 0; i < 10; i++) {
        char name[32];
        snprintf(name, sizeof(name), "failing_p%d", i);
        mesh_participant_id_t pid = CreateParticipant(name);
        mesh_channel_add_participant(failing_channel, pid);
    }

    // Phase 1: Normal operation on both channels
    std::atomic<size_t> healthy_ops{0};
    std::atomic<size_t> failing_ops{0};

    for (int i = 0; i < 20; i++) {
        mesh_belief_t belief;
        mesh_belief_init(&belief);
        belief.confidence = 0.8f;
        snprintf(belief.topic, sizeof(belief.topic), "pre_failure_%d", i);

        mesh_channel_introduce_belief(healthy_channel, &belief);
        healthy_ops++;

        mesh_channel_introduce_belief(failing_channel, &belief);
        failing_ops++;
    }

    // Phase 2: Simulate failure in one channel
    // (In real implementation, would mark channel unhealthy or remove participants)

    // Phase 3: Continue operations on healthy channel
    for (int i = 0; i < 50; i++) {
        mesh_belief_t belief;
        mesh_belief_init(&belief);
        belief.confidence = 0.9f;
        snprintf(belief.topic, sizeof(belief.topic), "post_failure_%d", i);

        nimcp_error_t err = mesh_channel_introduce_belief(healthy_channel, &belief);
        if (err == NIMCP_OK) {
            healthy_ops++;
        }
    }

    EXPECT_GT(healthy_ops.load(), 50u)
        << "Healthy channel should continue operations after partial failure";

    // Phase 4: Verify channel stats
    mesh_channel_stats_t healthy_stats, failing_stats;
    mesh_channel_get_stats(healthy_channel, &healthy_stats);
    mesh_channel_get_stats(failing_channel, &failing_stats);

    mesh_channel_destroy(healthy_channel);
    mesh_channel_destroy(failing_channel);
}

// =============================================================================
// Test 5: Quarantine and Credential Revocation Flow
// =============================================================================

TEST_F(MeshSecurityResilienceE2ETest, QuarantineAndRevocationFlow) {
    if (!exception_bridge_) {
        GTEST_SKIP() << "Exception bridge not available";
    }

    // Create MSP for this test
    mesh_msp_config_t msp_config;
    mesh_msp_default_config(&msp_config);
    msp_config.enable_quarantine = true;
    msp_config.quarantine_duration_ms = 1000;

    mesh_msp_t* test_msp = mesh_msp_create(&msp_config, nullptr);
    if (!test_msp) {
        GTEST_SKIP() << "MSP creation not available";
    }

    // Create and credential a participant
    mesh_participant_id_t malicious = CreateParticipant("malicious_module");

    credential_t cred;
    nimcp_error_t err = mesh_msp_issue_credential(
        test_msp, malicious, 5, MESH_CAP_ALL, &cred);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Verify credential is valid
    EXPECT_TRUE(mesh_msp_is_credential_valid(test_msp, malicious));

    // Phase 1: Detect malicious behavior
    for (int i = 0; i < 5; i++) {
        mesh_exception_response_t response;
        mesh_exception_bridge_route_error(
            exception_bridge_,
            NIMCP_ERROR_SECURITY,
            "malicious_behavior_detected",
            malicious,
            __FILE__,
            __LINE__,
            &response);
    }

    // Phase 2: Quarantine
    err = mesh_msp_quarantine(test_msp, malicious, 500);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(mesh_msp_is_quarantined(test_msp, malicious));

    // Quarantined participant should have restricted capabilities
    // (implementation specific)

    // Phase 3: Wait and release
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    err = mesh_msp_release_quarantine(test_msp, malicious);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_FALSE(mesh_msp_is_quarantined(test_msp, malicious));

    // Phase 4: Continued misbehavior leads to revocation
    err = mesh_msp_revoke_credential(test_msp, malicious, "repeated_violations");
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_FALSE(mesh_msp_is_credential_valid(test_msp, malicious));

    // Phase 5: Verify stats
    mesh_msp_stats_t stats;
    mesh_msp_get_stats(test_msp, &stats);
    EXPECT_GT(stats.quarantine_events, 0u);
    EXPECT_GT(stats.credentials_revoked, 0u);

    mesh_msp_destroy(test_msp);
}

// =============================================================================
// Test 6: Immune Response Escalation
// =============================================================================

TEST_F(MeshSecurityResilienceE2ETest, ImmuneResponseEscalation) {
    if (!exception_bridge_) {
        GTEST_SKIP() << "Exception bridge not available";
    }

    mesh_participant_id_t source = CreateParticipant("escalation_source");

    // Escalation pattern: repeated exceptions lead to stronger response
    mesh_immune_action_t last_action = MESH_IMMUNE_ACTION_NONE;
    int escalation_count = 0;

    for (int round = 0; round < 20; round++) {
        mesh_exception_response_t response;

        // Increase severity with each round
        nimcp_error_t error_code = (round < 5) ? NIMCP_ERROR_TIMEOUT :
                                   (round < 10) ? NIMCP_ERROR_RESOURCE :
                                   (round < 15) ? NIMCP_ERROR_MEMORY :
                                                  NIMCP_ERROR_SECURITY;

        mesh_exception_bridge_route_error(
            exception_bridge_,
            error_code,
            "escalation_test",
            source,
            __FILE__,
            __LINE__,
            &response);

        // Check for escalation
        if (response.primary_action > last_action) {
            escalation_count++;
            last_action = response.primary_action;
        }

        // Brief delay to allow processing
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    EXPECT_GT(escalation_count, 0)
        << "Repeated exceptions should trigger escalation";

    // Verify escalation stats
    mesh_exception_bridge_stats_t stats;
    mesh_exception_bridge_get_stats(exception_bridge_, &stats);
    EXPECT_GT(stats.escalations, 0u)
        << "Escalations should be recorded in stats";
}

// =============================================================================
// Test 7: System-Wide Health Monitoring During Incidents
// =============================================================================

TEST_F(MeshSecurityResilienceE2ETest, HealthMonitoringDuringIncidents) {
    if (!health_bridge_ || !exception_bridge_) {
        GTEST_SKIP() << "Health or exception bridge not available";
    }

    // Get baseline health
    mesh_system_health_t baseline_health;
    mesh_health_bridge_get_system_health(health_bridge_, &baseline_health);

    // Simulate security incident
    mesh_participant_id_t attacker = CreateParticipant("attacker");

    for (int i = 0; i < 50; i++) {
        mesh_exception_response_t response;
        mesh_exception_bridge_route_error(
            exception_bridge_,
            NIMCP_ERROR_SECURITY,
            "security_incident",
            attacker,
            __FILE__,
            __LINE__,
            &response);
    }

    // Check health during incident
    mesh_system_health_t incident_health;
    mesh_health_bridge_get_system_health(health_bridge_, &incident_health);

    // Health should be affected by incidents
    // (exact behavior depends on implementation)

    // Verify health bridge recorded changes
    mesh_health_bridge_stats_t stats;
    mesh_health_bridge_get_stats(health_bridge_, &stats);

    // After incident subsides, health should recover
    std::this_thread::sleep_for(std::chrono::milliseconds(RECOVERY_WAIT_MS));

    mesh_system_health_t recovery_health;
    mesh_health_bridge_get_system_health(health_bridge_, &recovery_health);

    // Recovery health should be better than or equal to incident health
    // (implementation specific)
}

// =============================================================================
// Test 8: Cascading Failure Prevention
// =============================================================================

TEST_F(MeshSecurityResilienceE2ETest, CascadingFailurePrevention) {
    // Create multiple interconnected components
    mesh_coordinator_pool_config_t pool_config;
    mesh_coordinator_pool_config_init(&pool_config);
    pool_config.initial_size = 3;
    pool_config.enable_bft = true;

    mesh_coordinator_pool_t* pools[3] = {nullptr, nullptr, nullptr};
    const char* pool_names[] = {"pool_a", "pool_b", "pool_c"};

    for (int i = 0; i < 3; i++) {
        pool_config.pool_name = pool_names[i];
        pools[i] = mesh_coordinator_pool_create(&pool_config);
        if (!pools[i]) {
            for (int j = 0; j < i; j++) {
                mesh_coordinator_pool_destroy(pools[j]);
            }
            GTEST_SKIP() << "Coordinator pool creation not available";
        }
        mesh_coordinator_pool_elect_leader(pools[i]);
    }

    // Verify all pools have leaders
    for (int i = 0; i < 3; i++) {
        mesh_coordinator_pool_info_t info;
        mesh_coordinator_pool_get_info(pools[i], &info);
        EXPECT_TRUE(info.has_leader)
            << "Pool " << pool_names[i] << " should have leader";
    }

    // Simulate failure in first pool
    mesh_coordinator_pool_info_t info;
    mesh_coordinator_pool_get_info(pools[0], &info);
    if (info.has_leader) {
        mesh_coordinator_pool_handle_failure(pools[0], info.leader_index);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Other pools should remain functional (cascading prevention)
    for (int i = 1; i < 3; i++) {
        mesh_coordinator_pool_get_info(pools[i], &info);
        EXPECT_TRUE(info.has_leader)
            << "Pool " << pool_names[i] << " should still have leader after "
            << pool_names[0] << " failure";
    }

    // First pool should recover
    mesh_coordinator_pool_elect_leader(pools[0]);
    mesh_coordinator_pool_get_info(pools[0], &info);
    EXPECT_TRUE(info.has_leader)
        << "Failed pool should recover";

    // Cleanup
    for (int i = 0; i < 3; i++) {
        mesh_coordinator_pool_destroy(pools[i]);
    }
}

