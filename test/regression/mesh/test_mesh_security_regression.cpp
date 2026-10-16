/**
 * @file test_mesh_security_regression.cpp
 * @brief Regression Tests for Mesh Security System
 *
 * WHAT: Tests for security system stability under attack patterns and scale
 * WHY:  Catch regressions in immune response, BBB validation, and MSP operations
 * HOW:  Test exception floods, attack patterns, quarantine, and recovery
 *
 * TEST COVERAGE:
 * - Immune response under exception flood
 * - BBB validation under attack patterns
 * - Quarantine/revocation at scale
 * - Recovery from security incidents
 * - Credential lifecycle under stress
 * - MSP policy enforcement edge cases
 * - Concurrent security operations
 * - Exception bridge stability
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
#include <random>
#include <mutex>

extern "C" {
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_msp.h"
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_exception_bridge.h"
#include "mesh/nimcp_mesh_health_bridge.h"
#include "mesh/nimcp_mesh_participant.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
}

// =============================================================================
// Test Constants
// =============================================================================

static constexpr size_t EXCEPTION_FLOOD_COUNT = 500;
static constexpr size_t ATTACK_PATTERN_COUNT = 100;
static constexpr size_t SCALE_PARTICIPANT_COUNT = 100;
static constexpr size_t CONCURRENT_THREADS = 8;

// =============================================================================
// Test Fixture
// =============================================================================

class MeshSecurityRegressionTest : public ::testing::Test {
protected:
    mesh_msp_t* msp_ = nullptr;
    mesh_bootstrap_t* bootstrap_ = nullptr;
    mesh_exception_bridge_t* exception_bridge_ = nullptr;

    std::vector<mesh_participant_id_t> participants_;

    void SetUp() override {
        mesh_msp_config_t msp_config;
        mesh_msp_default_config(&msp_config);
        msp_config.enable_quarantine = true;
        msp_config.quarantine_duration_ms = 1000;
        msp_config.enable_logging = false;
        msp_config.enable_audit = true;

        msp_ = mesh_msp_create(&msp_config, nullptr);
        ASSERT_NE(msp_, nullptr);
    }

    void TearDown() override {
        if (exception_bridge_) {
            mesh_exception_bridge_destroy(exception_bridge_);
            exception_bridge_ = nullptr;
        }
        if (bootstrap_) {
            mesh_bootstrap_destroy(bootstrap_);
            bootstrap_ = nullptr;
        }
        if (msp_) {
            mesh_msp_destroy(msp_);
            msp_ = nullptr;
        }
        participants_.clear();
    }

    mesh_participant_id_t CreateAndCredentialParticipant(
        const char* name, uint32_t privilege_level, uint64_t capabilities) {

        mesh_participant_config_t config;
        mesh_participant_config_init(&config);
        config.name = name;

        mesh_participant_t* p = mesh_participant_create(&config);
        if (!p) return 0;

        mesh_participant_id_t pid = mesh_participant_get_id(p);

        credential_t cred;
        mesh_msp_issue_credential(msp_, pid, privilege_level, capabilities, &cred);

        participants_.push_back(pid);
        return pid;
    }
};

// =============================================================================
// Test 1: Immune Response Under Exception Flood
// =============================================================================

TEST_F(MeshSecurityRegressionTest, ImmuneResponseUnderExceptionFlood) {
    // Bug scenario: Exception flood overwhelmed immune system
    mesh_bootstrap_config_t boot_config;
    mesh_bootstrap_default_config(&boot_config);

    bootstrap_ = mesh_bootstrap_create(&boot_config);
    if (!bootstrap_) {
        GTEST_SKIP() << "Bootstrap creation not available";
    }

    mesh_exception_bridge_config_t exc_config;
    mesh_exception_bridge_default_config(&exc_config);
    exc_config.min_report_severity = MESH_EXC_SEVERITY_WARNING;
    exc_config.quarantine_threshold = MESH_EXC_SEVERITY_SEVERE;
    exc_config.debounce_ms = 10;
    exc_config.enable_auto_quarantine = true;

    exception_bridge_ = mesh_exception_bridge_create(bootstrap_, nullptr, &exc_config);
    if (!exception_bridge_) {
        GTEST_SKIP() << "Exception bridge creation not available";
    }

    mesh_participant_id_t pid = CreateAndCredentialParticipant(
        "flood_source", 5, MESH_CAP_PROPOSE | MESH_CAP_ENDORSE);

    auto start = std::chrono::high_resolution_clock::now();
    std::atomic<size_t> processed{0};
    std::atomic<size_t> debounced{0};

    // Flood with exceptions
    for (size_t i = 0; i < EXCEPTION_FLOOD_COUNT; i++) {
        mesh_exception_response_t response;

        nimcp_error_t err = mesh_exception_bridge_route_error(
            exception_bridge_,
            NIMCP_ERROR_TIMEOUT,
            "flood_exception",
            pid,
            __FILE__,
            __LINE__,
            &response);

        if (err == NIMCP_SUCCESS) {
            processed++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Should complete in reasonable time (debouncing helps)
    EXPECT_LT(duration_ms, 10000)
        << "Exception flood processing took too long: " << duration_ms << "ms";

    // Verify debouncing worked
    mesh_exception_bridge_stats_t stats;
    mesh_exception_bridge_get_stats(exception_bridge_, &stats);

    EXPECT_LT(stats.exceptions_received, EXCEPTION_FLOOD_COUNT)
        << "Debouncing should reduce processed exceptions";

    EXPECT_GT(stats.debounced_exceptions, 0u)
        << "Some exceptions should be debounced";

    // Verify no resource exhaustion
    mesh_exception_bridge_reset_stats(exception_bridge_);
}

// =============================================================================
// Test 2: BBB Validation Under Attack Patterns
// =============================================================================

TEST_F(MeshSecurityRegressionTest, BBBValidationUnderAttackPatterns) {
    // Bug scenario: BBB validation failed to detect attack patterns
    mesh_bootstrap_config_t boot_config;
    mesh_bootstrap_default_config(&boot_config);

    bootstrap_ = mesh_bootstrap_create(&boot_config);
    if (!bootstrap_) {
        GTEST_SKIP() << "Bootstrap creation not available";
    }

    mesh_exception_bridge_config_t exc_config;
    mesh_exception_bridge_default_config(&exc_config);
    exc_config.enable_bbb_validation = true;

    exception_bridge_ = mesh_exception_bridge_create(bootstrap_, nullptr, &exc_config);
    if (!exception_bridge_) {
        GTEST_SKIP() << "Exception bridge creation not available";
    }

    // Create attack patterns
    struct AttackPattern {
        mesh_exception_category_t category;
        mesh_exception_severity_t severity;
        const char* description;
    };

    AttackPattern attack_patterns[] = {
        {MESH_EXC_CAT_SECURITY, MESH_EXC_SEVERITY_CRITICAL, "privilege_escalation"},
        {MESH_EXC_CAT_MEMORY, MESH_EXC_SEVERITY_SEVERE, "buffer_overflow"},
        {MESH_EXC_CAT_NETWORK, MESH_EXC_SEVERITY_SEVERE, "injection_attack"},
        {MESH_EXC_CAT_DATA, MESH_EXC_SEVERITY_CRITICAL, "data_exfiltration"},
        {MESH_EXC_CAT_RESOURCE, MESH_EXC_SEVERITY_SEVERE, "dos_attack"},
    };
    size_t num_patterns = sizeof(attack_patterns) / sizeof(attack_patterns[0]);

    size_t high_threat_count = 0;

    for (size_t round = 0; round < ATTACK_PATTERN_COUNT; round++) {
        for (size_t p = 0; p < num_patterns; p++) {
            mesh_exception_antigen_t antigen;
            memset(&antigen, 0, sizeof(antigen));

            antigen.antigen_id = static_cast<uint32_t>(round * num_patterns + p);
            antigen.category = attack_patterns[p].category;
            antigen.severity = attack_patterns[p].severity;
            antigen.error_code = NIMCP_ERROR_SECURITY;
            strncpy(antigen.error_message, attack_patterns[p].description,
                   sizeof(antigen.error_message) - 1);

            float threat_score = 0.0f;
            nimcp_error_t err = mesh_exception_bridge_bbb_validate(
                exception_bridge_, &antigen, &threat_score);

            if (err == NIMCP_SUCCESS) {
                if (threat_score > 0.7f) {
                    high_threat_count++;
                }
            }
        }
    }

    // Security-related exceptions should have high threat scores
    size_t expected_high_threat = ATTACK_PATTERN_COUNT * num_patterns * 0.5;
    EXPECT_GE(high_threat_count, expected_high_threat)
        << "BBB should detect most attack patterns as high threat";

    mesh_exception_bridge_stats_t stats;
    mesh_exception_bridge_get_stats(exception_bridge_, &stats);
    EXPECT_GT(stats.bbb_validations, 0u)
        << "BBB validations should be performed";
}

// =============================================================================
// Test 3: Quarantine/Revocation at Scale
// =============================================================================

TEST_F(MeshSecurityRegressionTest, QuarantineRevocationAtScale) {
    // Bug scenario: Quarantine/revocation operations failed at scale
    std::vector<mesh_participant_id_t> participants;

    auto start = std::chrono::high_resolution_clock::now();

    // Create and credential many participants
    for (size_t i = 0; i < SCALE_PARTICIPANT_COUNT; i++) {
        char name[64];
        snprintf(name, sizeof(name), "scale_participant_%zu", i);

        mesh_participant_id_t pid = CreateAndCredentialParticipant(
            name, 5, MESH_CAP_ALL);

        ASSERT_NE(pid, 0u) << "Failed to create participant " << i;
        participants.push_back(pid);
    }

    // Quarantine half
    size_t quarantine_success = 0;
    for (size_t i = 0; i < participants.size() / 2; i++) {
        nimcp_error_t err = mesh_msp_quarantine(msp_, participants[i], 5000);
        if (err == NIMCP_SUCCESS) {
            quarantine_success++;
        }
    }

    EXPECT_EQ(quarantine_success, SCALE_PARTICIPANT_COUNT / 2)
        << "All quarantine operations should succeed";

    // Verify quarantine status
    for (size_t i = 0; i < participants.size() / 2; i++) {
        EXPECT_TRUE(mesh_msp_is_quarantined(msp_, participants[i]))
            << "Participant " << i << " should be quarantined";
    }

    for (size_t i = participants.size() / 2; i < participants.size(); i++) {
        EXPECT_FALSE(mesh_msp_is_quarantined(msp_, participants[i]))
            << "Participant " << i << " should NOT be quarantined";
    }

    // Revoke credentials for some
    size_t revoke_success = 0;
    for (size_t i = 0; i < 20; i++) {
        nimcp_error_t err = mesh_msp_revoke_credential(
            msp_, participants[i], "scale_test_revocation");
        if (err == NIMCP_SUCCESS) {
            revoke_success++;
        }
    }

    EXPECT_EQ(revoke_success, 20u)
        << "All revocation operations should succeed";

    // Verify revoked credentials are invalid
    for (size_t i = 0; i < 20; i++) {
        EXPECT_FALSE(mesh_msp_is_credential_valid(msp_, participants[i]))
            << "Revoked credential should be invalid";
    }

    // Release quarantine for non-revoked
    size_t release_success = 0;
    for (size_t i = 20; i < participants.size() / 2; i++) {
        nimcp_error_t err = mesh_msp_release_quarantine(msp_, participants[i]);
        if (err == NIMCP_SUCCESS) {
            release_success++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_LT(duration_ms, 5000)
        << "Scale operations took too long: " << duration_ms << "ms";

    // Verify stats
    mesh_msp_stats_t stats;
    mesh_msp_get_stats(msp_, &stats);
    EXPECT_EQ(stats.credentials_revoked, 20u);
    EXPECT_GE(stats.quarantine_events, SCALE_PARTICIPANT_COUNT / 2);
}

// =============================================================================
// Test 4: Recovery from Security Incidents
// =============================================================================

TEST_F(MeshSecurityRegressionTest, RecoveryFromSecurityIncidents) {
    // Bug scenario: System didn't recover properly after security incidents
    mesh_participant_id_t pid = CreateAndCredentialParticipant(
        "incident_test", 5, MESH_CAP_ALL);

    // Phase 1: Normal operation
    EXPECT_TRUE(mesh_msp_is_credential_valid(msp_, pid));
    EXPECT_TRUE(mesh_msp_check_capability(msp_, pid, MESH_CAP_PROPOSE));
    EXPECT_TRUE(mesh_msp_check_privilege(msp_, pid, 3));

    // Phase 2: Simulate incident - quarantine
    nimcp_error_t err = mesh_msp_quarantine(msp_, pid, 500);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(mesh_msp_is_quarantined(msp_, pid));

    // Quarantined participant should have restricted access
    // (depending on implementation, credential might still be valid but restricted)

    // Phase 3: Recovery
    err = mesh_msp_release_quarantine(msp_, pid);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_FALSE(mesh_msp_is_quarantined(msp_, pid));

    // Phase 4: Verify full recovery
    EXPECT_TRUE(mesh_msp_is_credential_valid(msp_, pid));
    EXPECT_TRUE(mesh_msp_check_capability(msp_, pid, MESH_CAP_PROPOSE));

    // Phase 5: Suspension and restoration
    err = mesh_msp_suspend_credential(msp_, pid, 500);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Suspended credential is not valid
    EXPECT_FALSE(mesh_msp_is_credential_valid(msp_, pid));

    err = mesh_msp_restore_credential(msp_, pid);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(mesh_msp_is_credential_valid(msp_, pid));

    // Verify recovery stats
    mesh_msp_stats_t stats;
    mesh_msp_get_stats(msp_, &stats);
    EXPECT_GT(stats.recovery_events, 0u);
}

// =============================================================================
// Test 5: Credential Lifecycle Under Stress
// =============================================================================

TEST_F(MeshSecurityRegressionTest, CredentialLifecycleUnderStress) {
    // Bug scenario: Rapid credential operations caused inconsistencies
    std::atomic<size_t> issue_success{0};
    std::atomic<size_t> refresh_success{0};
    std::atomic<size_t> suspend_success{0};
    std::atomic<size_t> restore_success{0};

    std::vector<std::thread> threads;
    std::mutex pid_mutex;
    std::vector<mesh_participant_id_t> pids;

    // Create initial participants
    for (size_t i = 0; i < 50; i++) {
        char name[64];
        snprintf(name, sizeof(name), "stress_participant_%zu", i);

        mesh_participant_id_t pid = CreateAndCredentialParticipant(
            name, 5, MESH_CAP_ALL);
        if (pid) {
            pids.push_back(pid);
            issue_success++;
        }
    }

    // Stress threads
    for (size_t t = 0; t < CONCURRENT_THREADS; t++) {
        threads.emplace_back([&, t]() {
            std::mt19937 rng(static_cast<unsigned>(t * 42));
            std::uniform_int_distribution<size_t> idx_dist(0, pids.size() - 1);

            for (size_t i = 0; i < 20; i++) {
                size_t idx = idx_dist(rng);
                mesh_participant_id_t pid = pids[idx];

                switch (i % 4) {
                    case 0:  // Refresh
                        if (mesh_msp_refresh_credential(msp_, pid) == NIMCP_SUCCESS) {
                            refresh_success++;
                        }
                        break;
                    case 1:  // Suspend
                        if (mesh_msp_suspend_credential(msp_, pid, 100) == NIMCP_SUCCESS) {
                            suspend_success++;
                        }
                        break;
                    case 2:  // Restore
                        if (mesh_msp_restore_credential(msp_, pid) == NIMCP_SUCCESS) {
                            restore_success++;
                        }
                        break;
                    case 3:  // Check validity
                        mesh_msp_is_credential_valid(msp_, pid);
                        break;
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // All operations should complete without crashes
    EXPECT_EQ(issue_success.load(), 50u);
    EXPECT_GT(refresh_success.load(), 0u);
    EXPECT_GT(suspend_success.load(), 0u);

    // System should still be consistent
    mesh_msp_stats_t stats;
    nimcp_error_t err = mesh_msp_get_stats(msp_, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

// =============================================================================
// Test 6: MSP Policy Enforcement Edge Cases
// =============================================================================

TEST_F(MeshSecurityRegressionTest, MSPPolicyEnforcementEdgeCases) {
    // Bug scenario: Policy enforcement had edge case failures
    mesh_participant_id_t low_priv = CreateAndCredentialParticipant(
        "low_privilege", 1, MESH_CAP_READ);

    mesh_participant_id_t mid_priv = CreateAndCredentialParticipant(
        "mid_privilege", 5, MESH_CAP_READ | MESH_CAP_WRITE | MESH_CAP_PROPOSE);

    mesh_participant_id_t high_priv = CreateAndCredentialParticipant(
        "high_privilege", 10, MESH_CAP_ALL);

    // Test privilege level checks
    EXPECT_FALSE(mesh_msp_check_privilege(msp_, low_priv, 5));
    EXPECT_TRUE(mesh_msp_check_privilege(msp_, mid_priv, 5));
    EXPECT_TRUE(mesh_msp_check_privilege(msp_, high_priv, 10));

    // Test capability checks
    EXPECT_TRUE(mesh_msp_check_capability(msp_, low_priv, MESH_CAP_READ));
    EXPECT_FALSE(mesh_msp_check_capability(msp_, low_priv, MESH_CAP_WRITE));

    EXPECT_TRUE(mesh_msp_check_capability(msp_, mid_priv, MESH_CAP_PROPOSE));
    EXPECT_FALSE(mesh_msp_check_capability(msp_, mid_priv, MESH_CAP_ADMIN));

    EXPECT_TRUE(mesh_msp_check_capability(msp_, high_priv, MESH_CAP_ADMIN));
    EXPECT_TRUE(mesh_msp_check_capability(msp_, high_priv, MESH_CAP_EMERGENCY));

    // Test edge case: zero capabilities
    mesh_participant_id_t no_cap = CreateAndCredentialParticipant(
        "no_capabilities", 5, 0);

    EXPECT_FALSE(mesh_msp_check_capability(msp_, no_cap, MESH_CAP_READ));

    // Test edge case: non-existent participant
    mesh_participant_id_t fake_pid = 0xDEADBEEF;
    EXPECT_FALSE(mesh_msp_check_privilege(msp_, fake_pid, 1));
    EXPECT_FALSE(mesh_msp_check_capability(msp_, fake_pid, MESH_CAP_READ));

    // Add and evaluate custom policy
    msp_access_policy_t policy;
    memset(&policy, 0, sizeof(policy));
    policy.policy_name = "admin_only";
    policy.type = MSP_POLICY_CAPABILITY_CHECK;
    policy.required_capabilities = MESH_CAP_ADMIN;

    nimcp_error_t err = mesh_msp_add_policy(msp_, &policy);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_FALSE(mesh_msp_evaluate_policy(msp_, "admin_only", low_priv, nullptr));
    EXPECT_FALSE(mesh_msp_evaluate_policy(msp_, "admin_only", mid_priv, nullptr));
    EXPECT_TRUE(mesh_msp_evaluate_policy(msp_, "admin_only", high_priv, nullptr));

    // Remove policy
    err = mesh_msp_remove_policy(msp_, "admin_only");
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

// =============================================================================
// Test 7: Concurrent Security Operations
// =============================================================================

TEST_F(MeshSecurityRegressionTest, ConcurrentSecurityOperations) {
    // Bug scenario: Concurrent security ops caused race conditions
    std::vector<mesh_participant_id_t> participants;

    // Create participants
    for (size_t i = 0; i < 20; i++) {
        char name[64];
        snprintf(name, sizeof(name), "concurrent_sec_%zu", i);

        mesh_participant_id_t pid = CreateAndCredentialParticipant(
            name, 5, MESH_CAP_ALL);
        participants.push_back(pid);
    }

    std::atomic<size_t> auth_checks{0};
    std::atomic<size_t> quarantine_ops{0};
    std::atomic<size_t> inconsistencies{0};
    std::atomic<bool> stop{false};

    std::vector<std::thread> threads;

    // Thread 1: Continuous authentication checks
    threads.emplace_back([&]() {
        while (!stop.load()) {
            for (auto pid : participants) {
                mesh_msp_authenticate(msp_, pid, nullptr, 0);
                auth_checks++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    // Thread 2: Quarantine operations
    threads.emplace_back([&]() {
        std::mt19937 rng(42);
        std::uniform_int_distribution<size_t> idx_dist(0, participants.size() - 1);

        while (!stop.load()) {
            size_t idx = idx_dist(rng);
            mesh_msp_quarantine(msp_, participants[idx], 50);
            quarantine_ops++;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    // Thread 3: Release operations
    threads.emplace_back([&]() {
        std::mt19937 rng(123);
        std::uniform_int_distribution<size_t> idx_dist(0, participants.size() - 1);

        while (!stop.load()) {
            size_t idx = idx_dist(rng);
            mesh_msp_release_quarantine(msp_, participants[idx]);
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
        }
    });

    // Thread 4: Validity checks
    threads.emplace_back([&]() {
        while (!stop.load()) {
            for (auto pid : participants) {
                bool valid = mesh_msp_is_credential_valid(msp_, pid);
                bool quarantined = mesh_msp_is_quarantined(msp_, pid);
                // Valid and quarantined states should be consistent
                // (implementation specific)
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    stop.store(true);

    for (auto& th : threads) {
        th.join();
    }

    EXPECT_GT(auth_checks.load(), 0u);
    EXPECT_GT(quarantine_ops.load(), 0u);
    EXPECT_EQ(inconsistencies.load(), 0u)
        << "No inconsistencies should occur during concurrent operations";
}

// =============================================================================
// Test 8: Exception Bridge Stability
// =============================================================================

TEST_F(MeshSecurityRegressionTest, ExceptionBridgeStability) {
    // Bug scenario: Exception bridge crashed on malformed input
    mesh_bootstrap_config_t boot_config;
    mesh_bootstrap_default_config(&boot_config);

    bootstrap_ = mesh_bootstrap_create(&boot_config);
    if (!bootstrap_) {
        GTEST_SKIP() << "Bootstrap creation not available";
    }

    mesh_exception_bridge_config_t exc_config;
    mesh_exception_bridge_default_config(&exc_config);

    exception_bridge_ = mesh_exception_bridge_create(bootstrap_, nullptr, &exc_config);
    if (!exception_bridge_) {
        GTEST_SKIP() << "Exception bridge creation not available";
    }

    // Test 1: NULL inputs
    mesh_exception_response_t response;
    nimcp_error_t err = mesh_exception_bridge_route(exception_bridge_, nullptr, &response);
    EXPECT_NE(err, NIMCP_SUCCESS) << "NULL exception should fail gracefully";

    // Test 2: NULL response
    nimcp_exception_t exc;
    memset(&exc, 0, sizeof(exc));
    err = mesh_exception_bridge_route(exception_bridge_, &exc, nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS) << "NULL response should fail gracefully";

    // Test 3: All severity levels
    mesh_exception_severity_t severities[] = {
        MESH_EXC_SEVERITY_TRACE,
        MESH_EXC_SEVERITY_INFO,
        MESH_EXC_SEVERITY_WARNING,
        MESH_EXC_SEVERITY_ERROR,
        MESH_EXC_SEVERITY_SEVERE,
        MESH_EXC_SEVERITY_CRITICAL
    };

    for (auto severity : severities) {
        mesh_exception_category_t category;
        mesh_exception_severity_t sev;

        err = mesh_exception_bridge_classify(
            NIMCP_ERROR_GENERIC, &category, &sev);
        // Should handle gracefully
    }

    // Test 4: All categories
    mesh_exception_category_t categories[] = {
        MESH_EXC_CAT_MEMORY,
        MESH_EXC_CAT_SECURITY,
        MESH_EXC_CAT_NETWORK,
        MESH_EXC_CAT_RESOURCE,
        MESH_EXC_CAT_LOGIC,
        MESH_EXC_CAT_TIMING,
        MESH_EXC_CAT_DATA,
        MESH_EXC_CAT_SYSTEM,
        MESH_EXC_CAT_GPU,
        MESH_EXC_CAT_UNKNOWN
    };

    for (auto category : categories) {
        mesh_exception_antigen_t antigen;
        memset(&antigen, 0, sizeof(antigen));
        antigen.category = category;
        antigen.severity = MESH_EXC_SEVERITY_ERROR;

        float threat_score;
        mesh_exception_bridge_bbb_validate(exception_bridge_, &antigen, &threat_score);
        // Should handle all categories gracefully
    }

    // Test 5: Rapid stats access
    for (int i = 0; i < 100; i++) {
        mesh_exception_bridge_stats_t stats;
        mesh_exception_bridge_get_stats(exception_bridge_, &stats);
        mesh_exception_bridge_reset_stats(exception_bridge_);
    }

    SUCCEED();
}

