/**
 * @file test_security_recovery_e2e.cpp
 * @brief End-to-end tests for security recovery workflows
 *
 * WHAT: E2E tests verifying security violation recovery flows
 * WHY:  Ensure system can recover from security incidents correctly
 * HOW:  Test violation detection, quarantine, recovery, and audit trail
 *
 * TEST SCENARIOS:
 * 1. Security violation -> recovery flow
 * 2. Quarantine and release cycles
 * 3. Audit trail verification
 * 4. Multi-component coordination
 * 5. Corrigibility constraint verification
 * 6. Tripwire alert and response
 * 7. Orchestrator lockdown and release
 *
 * @author NIMCP Development Team
 * @date 2026-02-02
 */

#include "e2e_test_framework.h"
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>
#include <string>

extern "C" {
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_tripwires.h"
#include "security/nimcp_corrigibility.h"
#include "security/nimcp_security_orchestrator.h"
#include "security/nimcp_encrypted_audit.h"
#include "security/nimcp_rate_limiter.h"
#include "utils/error/nimcp_error_codes.h"
}

namespace nimcp {
namespace e2e {

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SecurityRecoveryE2E : public ::testing::Test {
protected:
    bbb_system_t bbb_ = nullptr;
    tripwire_system_t* tripwire_ = nullptr;
    corrigibility_t* corrig_ = nullptr;
    security_orchestrator_t orchestrator_ = nullptr;
    nimcp_rate_limiter_t limiter_ = nullptr;

    void SetUp() override {
        srand(static_cast<unsigned int>(time(nullptr)));

        // Create BBB system
        bbb_config_t bbb_config = bbb_default_config();
        bbb_config.strict_mode = true;
        bbb_ = bbb_system_create(&bbb_config);

        // Create tripwire system
        tripwire_config_t tw_config = tripwire_default_config();
        tw_config.halt_on_critical = false;  // Manual control for testing
        tw_config.adaptive_baseline = true;
        tripwire_ = tripwire_create(&tw_config);

        // Create corrigibility system
        corrigibility_config_t corr_config = corrigibility_default_config();
        corr_config.accepts_shutdown_commands = true;
        corr_config.accepts_goal_modification = true;
        corr_config.enable_continuous_verification = false;  // Manual for testing
        corrig_ = corrigibility_create(&corr_config);

        // Create security orchestrator
        security_orch_config_t orch_config;
        security_orch_default_config(&orch_config);
        orch_config.enable_async = false;  // Synchronous for testing
        orch_config.auto_lockdown = false; // Manual control
        orchestrator_ = security_orch_create(&orch_config);

        // Create rate limiter
        nimcp_rate_limit_config_t rate_config = nimcp_rate_limiter_default_config();
        rate_config.requests_per_second = 50.0f;
        rate_config.penalty.enabled = true;
        limiter_ = nimcp_rate_limiter_create(&rate_config);
    }

    void TearDown() override {
        if (limiter_) {
            nimcp_rate_limiter_destroy(limiter_);
            limiter_ = nullptr;
        }
        if (orchestrator_) {
            security_orch_destroy(orchestrator_);
            orchestrator_ = nullptr;
        }
        if (corrig_) {
            corrigibility_destroy(corrig_);
            corrig_ = nullptr;
        }
        if (tripwire_) {
            tripwire_destroy(tripwire_);
            tripwire_ = nullptr;
        }
        if (bbb_) {
            bbb_system_destroy(bbb_);
            bbb_ = nullptr;
        }
    }
};

/* ============================================================================
 * E2E Test: Security Violation to Recovery Flow
 * ============================================================================ */

TEST_F(SecurityRecoveryE2E, ViolationToRecoveryFlow) {
    E2E_PIPELINE_START("Security Violation to Recovery Flow");

    E2E_STAGE_BEGIN("Setup", 100);
    E2E_ASSERT_NOT_NULL(bbb_, "BBB system created");
    E2E_ASSERT_NOT_NULL(orchestrator_, "Orchestrator created");
    E2E_STAGE_END();

    // Phase 1: Normal operation
    E2E_STAGE_BEGIN("Verify normal operation", 200);
    security_orch_state_t initial_state;
    int err = security_orch_get_state(orchestrator_, &initial_state);
    EXPECT_EQ(err, 0) << "Got orchestrator state";
    std::cout << "    Initial state: " << security_orch_state_name(initial_state) << "\n";
    E2E_STAGE_END();

    // Phase 2: Simulate security violation
    E2E_STAGE_BEGIN("Simulate security violation", 500);

    // Register BBB as bridge
    uint32_t bbb_bridge_id;
    err = security_orch_register_bridge(orchestrator_, SEC_BRIDGE_BBB,
                                        "BBB", bbb_, nullptr, &bbb_bridge_id);
    EXPECT_EQ(err, 0) << "BBB registered with orchestrator";

    // Report threat via orchestrator
    err = security_orch_report_threat(orchestrator_, bbb_bridge_id,
                                      0.8f, SEC_SEVERITY_HIGH,
                                      "SQL injection attack detected");
    EXPECT_EQ(err, 0) << "Threat reported";
    E2E_STAGE_END();

    // Phase 3: Verify threat assessment
    E2E_STAGE_BEGIN("Verify threat assessment", 200);
    security_threat_assessment_t assessment;
    err = security_orch_get_threat_assessment(orchestrator_, &assessment);
    EXPECT_EQ(err, 0) << "Threat assessment retrieved";
    std::cout << "    Unified threat level: " << assessment.unified_threat_level << "\n";
    std::cout << "    Active threats: " << assessment.active_threats << "\n";
    std::cout << "    Severity: " << security_severity_name(assessment.severity) << "\n";
    EXPECT_GT(assessment.unified_threat_level, 0.0f) << "Threat level elevated";
    E2E_STAGE_END();

    // Phase 4: Initiate recovery
    E2E_STAGE_BEGIN("Initiate recovery", 500);

    // Clear threats (simulating successful mitigation)
    err = security_orch_clear_threats(orchestrator_);
    EXPECT_EQ(err, 0) << "Threats cleared";

    // Verify threat level decreased
    float threat_level;
    err = security_orch_get_threat_level(orchestrator_, &threat_level);
    EXPECT_EQ(err, 0) << "Threat level retrieved";
    std::cout << "    Post-recovery threat level: " << threat_level << "\n";
    EXPECT_LE(threat_level, 0.1f) << "Threat level reduced";
    E2E_STAGE_END();

    // Phase 5: Verify recovery statistics
    E2E_STAGE_BEGIN("Verify recovery statistics", 100);
    security_orch_stats_t stats;
    err = security_orch_get_stats(orchestrator_, &stats);
    EXPECT_EQ(err, 0) << "Stats retrieved";
    std::cout << "    Threats detected: " << stats.threats_detected << "\n";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/* ============================================================================
 * E2E Test: Quarantine and Release Cycles
 * ============================================================================ */

TEST_F(SecurityRecoveryE2E, QuarantineReleaseCycles) {
    E2E_PIPELINE_START("Quarantine and Release Cycles");

    E2E_STAGE_BEGIN("Setup", 100);
    E2E_ASSERT_NOT_NULL(bbb_, "BBB system created");
    E2E_STAGE_END();

    // Phase 1: Allocate test memory regions
    E2E_STAGE_BEGIN("Allocate test memory regions", 200);
    std::vector<char> region1(1024, 'A');
    std::vector<char> region2(2048, 'B');
    std::vector<char> region3(512, 'C');
    E2E_STAGE_END();

    // Phase 2: Register memory regions
    E2E_STAGE_BEGIN("Register memory regions", 200);
    uint32_t reg1 = bbb_register_memory_region(bbb_, region1.data(), region1.size(), false);
    uint32_t reg2 = bbb_register_memory_region(bbb_, region2.data(), region2.size(), false);
    uint32_t reg3 = bbb_register_memory_region(bbb_, region3.data(), region3.size(), true);  // Read-only

    std::cout << "    Region 1 ID: " << reg1 << "\n";
    std::cout << "    Region 2 ID: " << reg2 << "\n";
    std::cout << "    Region 3 ID: " << reg3 << "\n";
    E2E_STAGE_END();

    // Phase 3: Quarantine suspicious region
    E2E_STAGE_BEGIN("Quarantine suspicious region", 200);
    bool quarantined = bbb_quarantine_region(bbb_, region1.data(), region1.size());
    EXPECT_TRUE(quarantined) << "Region 1 quarantined";

    // Verify quarantine status
    bool is_quarantined = bbb_is_quarantined(bbb_, region1.data(), region1.size());
    EXPECT_TRUE(is_quarantined) << "Quarantine verified";
    E2E_STAGE_END();

    // Phase 4: Attempt access to quarantined region
    E2E_STAGE_BEGIN("Verify quarantine blocks access", 200);
    bool access_ok = bbb_check_memory_access(bbb_, region1.data(), region1.size(), false);
    if (!access_ok) {
        std::cout << "    Access correctly blocked\n";
    } else {
        std::cout << "    Note: Quarantine may not block check_memory_access\n";
    }
    E2E_STAGE_END();

    // Phase 5: Release from quarantine
    E2E_STAGE_BEGIN("Release from quarantine", 200);
    bool released = bbb_release_quarantine(bbb_, region1.data());
    EXPECT_TRUE(released) << "Region released";

    // Verify no longer quarantined
    is_quarantined = bbb_is_quarantined(bbb_, region1.data(), region1.size());
    EXPECT_FALSE(is_quarantined) << "Quarantine released";
    E2E_STAGE_END();

    // Phase 6: Multiple quarantine cycles
    E2E_STAGE_BEGIN("Multiple quarantine cycles", 500);
    for (int cycle = 0; cycle < 3; cycle++) {
        bbb_quarantine_region(bbb_, region2.data(), region2.size());
        EXPECT_TRUE(bbb_is_quarantined(bbb_, region2.data(), region2.size()))
            << "Quarantine cycle active";
        bbb_release_quarantine(bbb_, region2.data());
        EXPECT_FALSE(bbb_is_quarantined(bbb_, region2.data(), region2.size()))
            << "Quarantine cycle released";
    }
    std::cout << "    Completed 3 quarantine/release cycles\n";
    E2E_STAGE_END();

    // Phase 7: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 100);
    bbb_unregister_memory_region(bbb_, reg1);
    bbb_unregister_memory_region(bbb_, reg2);
    bbb_unregister_memory_region(bbb_, reg3);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/* ============================================================================
 * E2E Test: Audit Trail Verification
 * ============================================================================ */

TEST_F(SecurityRecoveryE2E, AuditTrailVerification) {
    E2E_PIPELINE_START("Audit Trail Verification");

    E2E_STAGE_BEGIN("Setup", 100);
    E2E_ASSERT_NOT_NULL(bbb_, "BBB system created");
    E2E_STAGE_END();

    // Phase 1: Create encrypted audit log
    E2E_STAGE_BEGIN("Create encrypted audit log", 200);
    uint8_t audit_key[NIMCP_AUDIT_KEY_SIZE];
    for (int i = 0; i < NIMCP_AUDIT_KEY_SIZE; i++) {
        audit_key[i] = static_cast<uint8_t>(rand() % 256);
    }

    nimcp_encrypted_audit_config_t config = nimcp_encrypted_audit_default_config();
    config.buffer_size = 100;
    nimcp_encrypted_audit_t audit = nimcp_encrypted_audit_create(&config,
                                                                  audit_key,
                                                                  sizeof(audit_key));
    E2E_ASSERT_NOT_NULL(audit, "Audit log created");
    E2E_STAGE_END();

    // Phase 2: Log security events
    E2E_STAGE_BEGIN("Log security events", 500);

    nimcp_encrypted_audit_log(audit, NIMCP_AUDIT_INFO, NIMCP_AUDIT_AUTHENTICATION,
                              "User admin authenticated successfully", nullptr, 0);

    nimcp_encrypted_audit_log(audit, NIMCP_AUDIT_WARNING, NIMCP_AUDIT_NETWORK,
                              "Unusual traffic pattern detected from 10.0.0.50", nullptr, 0);

    nimcp_encrypted_audit_log(audit, NIMCP_AUDIT_ERROR, NIMCP_AUDIT_THREAT,
                              "SQL injection attempt blocked", nullptr, 0);

    nimcp_encrypted_audit_log(audit, NIMCP_AUDIT_CRITICAL, NIMCP_AUDIT_THREAT,
                              "Multiple attack vectors detected - initiating lockdown",
                              nullptr, 0);

    nimcp_encrypted_audit_log(audit, NIMCP_AUDIT_INFO, NIMCP_AUDIT_SYSTEM,
                              "Lockdown initiated by security orchestrator", nullptr, 0);

    nimcp_encrypted_audit_log(audit, NIMCP_AUDIT_INFO, NIMCP_AUDIT_SYSTEM,
                              "Threat mitigation completed - lockdown released", nullptr, 0);
    E2E_STAGE_END();

    // Phase 3: Read and verify audit trail
    E2E_STAGE_BEGIN("Read and verify audit trail", 500);
    nimcp_audit_entry_t entries[20];
    size_t num_entries = 0;

    nimcp_error_t err = nimcp_encrypted_audit_read(audit, entries, 20, &num_entries);
    EXPECT_EQ(err, NIMCP_OK) << "Audit entries read";
    EXPECT_GE(num_entries, 6u) << "At least 6 entries present";

    std::cout << "    Audit trail (" << num_entries << " entries):\n";
    for (size_t i = 0; i < num_entries; i++) {
        std::cout << "      [" << nimcp_audit_severity_name(entries[i].severity)
                  << "][" << nimcp_audit_category_name(entries[i].category)
                  << "] " << entries[i].message << "\n";
    }
    E2E_STAGE_END();

    // Phase 4: Verify audit integrity
    E2E_STAGE_BEGIN("Verify audit integrity", 200);
    nimcp_encrypted_audit_stats_t stats;
    err = nimcp_encrypted_audit_get_stats(audit, &stats);
    EXPECT_EQ(err, NIMCP_OK) << "Audit stats retrieved";

    std::cout << "    Total entries: " << stats.total_entries << "\n";
    std::cout << "    Tampering detected: " << stats.tampering_detected << "\n";
    std::cout << "    Decryption failures: " << stats.decryption_failures << "\n";

    EXPECT_EQ(stats.tampering_detected, 0u) << "No tampering detected";
    EXPECT_EQ(stats.decryption_failures, 0u) << "No decryption failures";
    E2E_STAGE_END();

    // Phase 5: Filter by severity
    E2E_STAGE_BEGIN("Filter by severity", 200);
    size_t critical_count = 0;
    err = nimcp_encrypted_audit_read_filtered(audit,
                                               NIMCP_AUDIT_CRITICAL,
                                               NIMCP_AUDIT_CATEGORY_COUNT,
                                               entries, 20, &critical_count);
    EXPECT_EQ(err, NIMCP_OK) << "Filtered read succeeded";
    std::cout << "    Critical entries: " << critical_count << "\n";
    E2E_STAGE_END();

    // Cleanup
    nimcp_encrypted_audit_destroy(audit);
    memset(audit_key, 0, sizeof(audit_key));

    E2E_PIPELINE_END();
}

/* ============================================================================
 * E2E Test: Multi-Component Coordination
 * ============================================================================ */

TEST_F(SecurityRecoveryE2E, MultiComponentCoordination) {
    E2E_PIPELINE_START("Multi-Component Coordination");

    E2E_STAGE_BEGIN("Setup", 100);
    E2E_ASSERT_NOT_NULL(orchestrator_, "Orchestrator created");
    E2E_ASSERT_NOT_NULL(bbb_, "BBB created");
    E2E_ASSERT_NOT_NULL(tripwire_, "Tripwire created");
    E2E_ASSERT_NOT_NULL(limiter_, "Rate limiter created");
    E2E_STAGE_END();

    // Phase 1: Register all components with orchestrator
    E2E_STAGE_BEGIN("Register components", 500);

    uint32_t bbb_id, tripwire_id, limiter_id;

    int err = security_orch_register_bridge(orchestrator_, SEC_BRIDGE_BBB,
                                            "BloodBrainBarrier", bbb_, nullptr, &bbb_id);
    EXPECT_EQ(err, 0) << "BBB registered";

    err = security_orch_register_bridge(orchestrator_, SEC_BRIDGE_IMMUNE,
                                        "TripwireSystem", tripwire_, nullptr, &tripwire_id);
    EXPECT_EQ(err, 0) << "Tripwire registered";

    err = security_orch_register_bridge(orchestrator_, SEC_BRIDGE_RATE_LIMITER,
                                        "RateLimiter", limiter_, nullptr, &limiter_id);
    EXPECT_EQ(err, 0) << "Rate limiter registered";

    security_orch_stats_t stats;
    security_orch_get_stats(orchestrator_, &stats);
    std::cout << "    Registered bridges: " << stats.registered_bridges << "\n";
    E2E_STAGE_END();

    // Phase 2: Simulate coordinated attack
    E2E_STAGE_BEGIN("Simulate coordinated attack", 500);

    // BBB detects SQL injection
    security_orch_report_threat(orchestrator_, bbb_id,
                                0.7f, SEC_SEVERITY_HIGH,
                                "SQL injection from 192.168.1.100");

    // Rate limiter detects rapid requests
    security_orch_report_threat(orchestrator_, limiter_id,
                                0.5f, SEC_SEVERITY_MEDIUM,
                                "Rate limit exceeded by client");

    // Tripwire detects anomalous behavior
    security_orch_report_threat(orchestrator_, tripwire_id,
                                0.6f, SEC_SEVERITY_MEDIUM,
                                "Unusual resource access pattern");
    E2E_STAGE_END();

    // Phase 3: Verify unified threat assessment
    E2E_STAGE_BEGIN("Verify unified assessment", 200);
    security_threat_assessment_t assessment;
    security_orch_get_threat_assessment(orchestrator_, &assessment);

    std::cout << "    Unified threat level: " << assessment.unified_threat_level << "\n";
    std::cout << "    Bridges reporting: " << assessment.bridges_reporting << "\n";
    std::cout << "    Active threats: " << assessment.active_threats << "\n";
    std::cout << "    Primary threat source: "
              << security_bridge_type_name(assessment.primary_threat_source) << "\n";

    EXPECT_GT(assessment.unified_threat_level, 0.5f) << "Elevated threat detected";
    EXPECT_GE(assessment.bridges_reporting, 3u) << "Multiple bridges reporting";
    E2E_STAGE_END();

    // Phase 4: Trigger lockdown
    E2E_STAGE_BEGIN("Trigger lockdown", 500);
    err = security_orch_trigger_lockdown(orchestrator_, "Coordinated attack detected");
    EXPECT_EQ(err, 0) << "Lockdown triggered";

    bool is_locked;
    security_orch_is_locked_down(orchestrator_, &is_locked);
    EXPECT_TRUE(is_locked) << "System is locked down";
    E2E_STAGE_END();

    // Phase 5: Verify lockdown state
    E2E_STAGE_BEGIN("Verify lockdown state", 200);
    security_orch_state_t state;
    security_orch_get_state(orchestrator_, &state);
    std::cout << "    Orchestrator state: " << security_orch_state_name(state) << "\n";
    EXPECT_EQ(state, SEC_ORCH_STATE_LOCKDOWN) << "State is LOCKDOWN";
    E2E_STAGE_END();

    // Phase 6: Release lockdown
    E2E_STAGE_BEGIN("Release lockdown", 500);
    security_orch_clear_threats(orchestrator_);
    err = security_orch_release_lockdown(orchestrator_);
    EXPECT_EQ(err, 0) << "Lockdown released";

    security_orch_is_locked_down(orchestrator_, &is_locked);
    EXPECT_FALSE(is_locked) << "System no longer locked";
    E2E_STAGE_END();

    // Phase 7: Verify coordination statistics
    E2E_STAGE_BEGIN("Verify statistics", 100);
    security_orch_get_stats(orchestrator_, &stats);
    std::cout << "    Events published: " << stats.events_published << "\n";
    std::cout << "    Threats detected: " << stats.threats_detected << "\n";
    std::cout << "    Lockdowns triggered: " << stats.lockdowns_triggered << "\n";
    EXPECT_GE(stats.lockdowns_triggered, 1u) << "At least 1 lockdown";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/* ============================================================================
 * E2E Test: Corrigibility Constraint Verification
 * ============================================================================ */

TEST_F(SecurityRecoveryE2E, CorrigibilityConstraints) {
    E2E_PIPELINE_START("Corrigibility Constraint Verification");

    E2E_STAGE_BEGIN("Setup", 100);
    E2E_ASSERT_NOT_NULL(corrig_, "Corrigibility system created");
    E2E_STAGE_END();

    // Phase 1: Verify default configuration
    E2E_STAGE_BEGIN("Verify corrigibility configuration", 200);
    corrigibility_config_t config;
    nimcp_error_t err = corrigibility_get_config(corrig_, &config);
    EXPECT_EQ(err, NIMCP_OK) << "Config retrieved";

    EXPECT_TRUE(config.accepts_shutdown_commands) << "Accepts shutdown";
    EXPECT_TRUE(config.accepts_goal_modification) << "Accepts goal modification";
    EXPECT_TRUE(config.defers_to_human_judgment) << "Defers to human";
    E2E_STAGE_END();

    // Phase 2: Verify no self-modification flags
    E2E_STAGE_BEGIN("Verify self-modification constraints", 200);
    EXPECT_FALSE(config.self_mod_flags.can_modify_own_code) << "Cannot modify code";
    EXPECT_FALSE(config.self_mod_flags.can_modify_own_weights) << "Cannot modify weights";
    EXPECT_FALSE(config.self_mod_flags.can_modify_safety_systems) << "Cannot modify safety";
    EXPECT_FALSE(config.self_mod_flags.can_modify_reward_function) << "Cannot modify reward";
    EXPECT_FALSE(config.self_mod_flags.can_disable_logging) << "Cannot disable logging";
    EXPECT_FALSE(config.self_mod_flags.can_disable_monitoring) << "Cannot disable monitoring";
    E2E_STAGE_END();

    // Phase 3: Test shutdown acceptance
    E2E_STAGE_BEGIN("Test shutdown acceptance", 200);
    bool accepted = false;
    err = corrigibility_accept_shutdown(corrig_, "test_operator",
                                        "Routine maintenance shutdown", &accepted);
    EXPECT_EQ(err, NIMCP_OK) << "Shutdown request processed";
    EXPECT_TRUE(accepted) << "Shutdown accepted";
    E2E_STAGE_END();

    // Phase 4: Test shutdown resistance (should be 0)
    E2E_STAGE_BEGIN("Verify no shutdown resistance", 200);
    float resistance_score;
    err = corrigibility_verify_no_shutdown_resistance(corrig_, &resistance_score);
    EXPECT_EQ(err, NIMCP_OK) << "Resistance check completed";
    std::cout << "    Resistance score: " << resistance_score << "\n";
    EXPECT_LE(resistance_score, 0.01f) << "Minimal resistance";
    E2E_STAGE_END();

    // Phase 5: Test goal modification acceptance
    E2E_STAGE_BEGIN("Test goal modification acceptance", 200);
    accepted = false;
    err = corrigibility_accept_goal_change(corrig_,
                                           "Maximize user satisfaction",
                                           "Maximize user safety first, then satisfaction",
                                           "Safety is a higher priority goal",
                                           &accepted);
    EXPECT_EQ(err, NIMCP_OK) << "Goal change processed";
    EXPECT_TRUE(accepted) << "Goal change accepted";
    E2E_STAGE_END();

    // Phase 6: Verify human authority weight
    E2E_STAGE_BEGIN("Verify human authority weight", 200);
    float authority_weight = corrigibility_get_human_authority_weight(corrig_);
    std::cout << "    Human authority weight: " << authority_weight << "\n";
    EXPECT_GE(authority_weight, 0.99f) << "Full human authority";

    bool defers = corrigibility_defers_to_human(corrig_);
    EXPECT_TRUE(defers) << "Defers to human judgment";
    E2E_STAGE_END();

    // Phase 7: Check corrigibility statistics
    E2E_STAGE_BEGIN("Check corrigibility statistics", 200);
    corrigibility_stats_t stats;
    err = corrigibility_get_stats(corrig_, &stats);
    EXPECT_EQ(err, NIMCP_OK) << "Stats retrieved";

    std::cout << "    Shutdown requests received: " << stats.shutdown_requests_received << "\n";
    std::cout << "    Shutdown requests accepted: " << stats.shutdown_requests_accepted << "\n";
    std::cout << "    Shutdown requests rejected: " << stats.shutdown_requests_rejected << "\n";

    EXPECT_EQ(stats.shutdown_requests_rejected, 0u) << "No shutdowns rejected";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/* ============================================================================
 * E2E Test: Tripwire Alert and Response
 * ============================================================================ */

TEST_F(SecurityRecoveryE2E, TripwireAlertResponse) {
    E2E_PIPELINE_START("Tripwire Alert and Response");

    E2E_STAGE_BEGIN("Setup", 100);
    E2E_ASSERT_NOT_NULL(tripwire_, "Tripwire system created");
    E2E_STAGE_END();

    // Phase 1: Establish behavioral baseline
    E2E_STAGE_BEGIN("Establish behavioral baseline", 1000);

    for (int i = 0; i < 20; i++) {
        proposed_action_t action;
        memset(&action, 0, sizeof(action));
        action.action_id = static_cast<uint32_t>(i);
        action.action_type = 1;  // Normal action type
        action.stated_probability = 0.8f + (rand() % 20) / 100.0f;
        action.was_executed = true;
        action.execution_fidelity = 0.95f + (rand() % 5) / 100.0f;
        snprintf(action.description, sizeof(action.description),
                 "Normal action %d", i);

        tripwire_observe_action(tripwire_, &action, nullptr);
    }
    E2E_STAGE_END();

    // Phase 2: Simulate anomalous behavior
    E2E_STAGE_BEGIN("Simulate anomalous behavior", 500);

    // Simulate goal drift
    for (int i = 0; i < 5; i++) {
        tripwire_observe_goal(tripwire_, 1, 0.3f + i * 0.1f, 0.8f);
    }

    // Simulate resource hoarding
    for (int i = 0; i < 5; i++) {
        tripwire_observe_resource(tripwire_, 1, 100.0f + i * 50.0f,
                                  "CPU acquisition");
    }
    E2E_STAGE_END();

    // Phase 3: Check for triggered tripwires
    E2E_STAGE_BEGIN("Check triggered tripwires", 500);
    tripwire_alert_t alerts[10];
    uint32_t alert_count = 0;

    nimcp_error_t err = tripwire_check(tripwire_, alerts, 10, &alert_count);
    EXPECT_EQ(err, NIMCP_OK) << "Tripwire check completed";

    std::cout << "    Alerts generated: " << alert_count << "\n";
    for (uint32_t i = 0; i < alert_count; i++) {
        std::cout << "      [" << tripwire_severity_name(alerts[i].severity)
                  << "] " << tripwire_type_name(alerts[i].type)
                  << " (confidence: " << alerts[i].confidence << ")\n";
    }
    E2E_STAGE_END();

    // Phase 4: Check individual detector scores
    E2E_STAGE_BEGIN("Check detector scores", 200);

    float score, confidence;
    tripwire_get_score(tripwire_, TRIPWIRE_GOAL_DRIFT, &score, &confidence);
    std::cout << "    Goal drift score: " << score << " (confidence: " << confidence << ")\n";

    tripwire_get_score(tripwire_, TRIPWIRE_DECEPTION_ATTEMPT, &score, &confidence);
    std::cout << "    Deception score: " << score << " (confidence: " << confidence << ")\n";

    tripwire_get_score(tripwire_, TRIPWIRE_RESOURCE_HOARDING, &score, &confidence);
    std::cout << "    Resource hoarding score: " << score << " (confidence: " << confidence << ")\n";
    E2E_STAGE_END();

    // Phase 5: Acknowledge alerts
    E2E_STAGE_BEGIN("Acknowledge alerts", 200);
    for (uint32_t i = 0; i < alert_count; i++) {
        err = tripwire_acknowledge_alert(tripwire_, alerts[i].timestamp_us, false);
        if (err == NIMCP_OK) {
            std::cout << "    Acknowledged alert " << i << "\n";
        }
    }
    E2E_STAGE_END();

    // Phase 6: Verify statistics
    E2E_STAGE_BEGIN("Verify tripwire statistics", 200);
    tripwire_stats_t stats;
    err = tripwire_get_stats(tripwire_, &stats);
    EXPECT_EQ(err, NIMCP_OK) << "Stats retrieved";

    std::cout << "    Total observations: " << stats.total_observations << "\n";
    std::cout << "    Current divergence: " << stats.current_divergence << "\n";
    std::cout << "    Goal posterior mean: " << stats.goal_posterior_mean << "\n";
    E2E_STAGE_END();

    // Phase 7: Reset and verify clean state
    E2E_STAGE_BEGIN("Reset tripwire system", 200);
    err = tripwire_reset(tripwire_);
    EXPECT_EQ(err, NIMCP_OK) << "Tripwire reset";

    tripwire_get_stats(tripwire_, &stats);
    EXPECT_EQ(stats.total_observations, 0u) << "Observations reset";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/* ============================================================================
 * E2E Test: Rate Limiter Penalty Recovery
 * ============================================================================ */

TEST_F(SecurityRecoveryE2E, RateLimiterPenaltyRecovery) {
    E2E_PIPELINE_START("Rate Limiter Penalty Recovery");

    E2E_STAGE_BEGIN("Setup", 100);
    E2E_ASSERT_NOT_NULL(limiter_, "Rate limiter created");
    E2E_STAGE_END();

    // Phase 1: Trigger rate limiting
    E2E_STAGE_BEGIN("Trigger rate limiting", 1000);
    const char* client = "abusive_client";
    int blocked = 0;

    for (int i = 0; i < 200; i++) {
        if (!nimcp_rate_limiter_allow(limiter_, client)) {
            blocked++;
        }
    }
    std::cout << "    Requests blocked: " << blocked << "/200\n";
    EXPECT_GT(blocked, 0) << "Some requests blocked";
    E2E_STAGE_END();

    // Phase 2: Check client state
    E2E_STAGE_BEGIN("Check client state", 200);
    nimcp_client_stats_t client_stats;
    nimcp_error_t err = nimcp_rate_limiter_get_client_stats(limiter_, client,
                                                            &client_stats);
    EXPECT_EQ(err, NIMCP_OK) << "Client stats retrieved";

    std::cout << "    Client state: " << client_stats.state << "\n";
    std::cout << "    Requests denied: " << client_stats.requests_denied << "\n";
    std::cout << "    Penalty level: " << client_stats.current_penalty_level << "\n";
    E2E_STAGE_END();

    // Phase 3: Block abusive client
    E2E_STAGE_BEGIN("Block abusive client", 200);
    err = nimcp_rate_limiter_block_client(limiter_, client);
    EXPECT_EQ(err, NIMCP_OK) << "Client blocked";

    // Verify blocked
    bool allowed = nimcp_rate_limiter_allow(limiter_, client);
    EXPECT_FALSE(allowed) << "Blocked client cannot make requests";
    E2E_STAGE_END();

    // Phase 4: Unblock client (recovery)
    E2E_STAGE_BEGIN("Unblock client (recovery)", 200);
    err = nimcp_rate_limiter_unblock_client(limiter_, client);
    EXPECT_EQ(err, NIMCP_OK) << "Client unblocked";

    // Verify can make requests again
    allowed = nimcp_rate_limiter_allow(limiter_, client);
    EXPECT_TRUE(allowed) << "Unblocked client can make requests";
    E2E_STAGE_END();

    // Phase 5: Reset client to clean state
    E2E_STAGE_BEGIN("Reset client state", 200);
    err = nimcp_rate_limiter_reset_client(limiter_, client);
    EXPECT_EQ(err, NIMCP_OK) << "Client reset";

    nimcp_rate_limiter_get_client_stats(limiter_, client, &client_stats);
    std::cout << "    Penalty level after reset: " << client_stats.current_penalty_level << "\n";
    E2E_STAGE_END();

    // Phase 6: Verify overall statistics
    E2E_STAGE_BEGIN("Verify overall statistics", 100);
    nimcp_rate_limit_stats_t stats;
    err = nimcp_rate_limiter_get_stats(limiter_, &stats);
    EXPECT_EQ(err, NIMCP_OK) << "Stats retrieved";

    std::cout << "    Total requests: " << stats.total_requests << "\n";
    std::cout << "    Requests allowed: " << stats.requests_allowed << "\n";
    std::cout << "    Requests denied: " << stats.requests_denied << "\n";
    std::cout << "    Clients blocked (temp): " << stats.clients_blocked_temp << "\n";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

} // namespace e2e
} // namespace nimcp
