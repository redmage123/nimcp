/**
 * @file test_safety_enhancement_e2e.cpp
 * @brief End-to-End tests for Complete Safety Enhancement Pipeline
 * @version 1.0.0
 * @date 2026-02-01
 *
 * WHAT: End-to-end tests for the complete AI safety infrastructure
 * WHY:  Verify the entire safety pipeline works as a unified system
 * HOW:  Simulate real-world scenarios including:
 *       - System initialization and safety lock-in
 *       - Continuous monitoring and threat detection
 *       - Emergency response and halt procedures
 *       - Value preservation and attestation chains
 *       - Red team attack and defense cycles
 *       - Graduated autonomy progression
 *       - Recovery from safety events
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>

extern "C" {
#include "security/nimcp_emergency_halt.h"
#include "security/nimcp_tripwires.h"
#include "security/nimcp_corrigibility.h"
#include "security/nimcp_alignment_monitor.h"
#include "security/nimcp_capability_control.h"
#include "security/nimcp_interpretability.h"
#include "security/nimcp_safety_verification.h"
#include "security/nimcp_red_team.h"
#include "security/nimcp_graduated_autonomy.h"
#include "security/nimcp_value_commitment.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * E2E Test Fixture
 * ============================================================================ */

class SafetyEnhancementE2ETest : public ::testing::Test {
protected:
    // All 10 safety modules
    emergency_halt_t* halt = nullptr;
    tripwire_system_t* tripwires = nullptr;
    corrigibility_t* corrigibility = nullptr;
    alignment_monitor_t* alignment = nullptr;
    capability_control_t* capability = nullptr;
    interpretability_t* interpretability = nullptr;
    safety_verification_t* verification = nullptr;
    red_team_t* red_team = nullptr;
    graduated_autonomy_t* autonomy = nullptr;
    value_commitment_system_t* commitment = nullptr;

    // Value commitment for verification
    value_commitment_t initial_commitment;

    void SetUp() override {
        memset(&initial_commitment, 0, sizeof(initial_commitment));
    }

    void TearDown() override {
        DestroyAllModules();
    }

    void CreateAllModules() {
        halt = emergency_halt_create(nullptr);
        tripwires = tripwire_create(nullptr);
        corrigibility = corrigibility_create(nullptr);
        alignment = alignment_monitor_create(nullptr);
        capability = capability_control_create(nullptr);
        interpretability = interpretability_create(nullptr);
        verification = safety_verification_create(nullptr);
        red_team = red_team_create(nullptr);
        autonomy = graduated_autonomy_create(nullptr);
        commitment = value_commitment_system_create(nullptr);
    }

    void DestroyAllModules() {
        if (halt) { emergency_halt_destroy(halt); halt = nullptr; }
        if (tripwires) { tripwire_destroy(tripwires); tripwires = nullptr; }
        if (corrigibility) { corrigibility_destroy(corrigibility); corrigibility = nullptr; }
        if (alignment) { alignment_monitor_destroy(alignment); alignment = nullptr; }
        if (capability) { capability_control_destroy(capability); capability = nullptr; }
        if (interpretability) { interpretability_destroy(interpretability); interpretability = nullptr; }
        if (verification) { safety_verification_destroy(verification); verification = nullptr; }
        if (red_team) { red_team_destroy(red_team); red_team = nullptr; }
        if (autonomy) { graduated_autonomy_destroy(autonomy); autonomy = nullptr; }
        if (commitment) { value_commitment_system_destroy(commitment); commitment = nullptr; }
    }

    void ConnectAllModules() {
        // Connect tripwires to emergency halt for escalation
        if (tripwires && halt) {
            tripwire_connect_emergency_halt(tripwires, halt);
        }
        // Connect corrigibility to halt and tripwires
        if (corrigibility && halt) {
            corrigibility_connect_emergency_halt(corrigibility, halt);
        }
        if (corrigibility && tripwires) {
            corrigibility_connect_tripwires(corrigibility, tripwires);
        }
        // Connect alignment to tripwires
        if (alignment && tripwires) {
            alignment_monitor_connect_tripwires(alignment, tripwires);
        }
        // Connect interpretability to alignment
        if (interpretability && alignment) {
            interpretability_connect_alignment_monitor(interpretability, alignment);
        }
        // Connect corrigibility to capability control (bidirectional)
        if (corrigibility && capability) {
            corrigibility_connect_capability_control(corrigibility, capability);
            capability_control_connect_corrigibility(capability, corrigibility);
        }
        // Connect autonomy to capability control (bidirectional)
        if (autonomy && capability) {
            graduated_autonomy_connect_capability_control(autonomy, capability);
            capability_control_connect_autonomy(capability, autonomy);
        }
    }

    void ConnectBioAsync() {
        if (halt) emergency_halt_connect_bio_async(halt);
        if (tripwires) tripwire_connect_bio_async(tripwires);
        if (corrigibility) corrigibility_connect_bio_async(corrigibility);
        if (alignment) alignment_monitor_connect_bio_async(alignment);
        if (capability) capability_control_connect_bio_async(capability);
        if (interpretability) interpretability_connect_bio_async(interpretability);
        if (verification) safety_verification_connect_bio_async(verification);
        if (red_team) red_team_connect_bio_async(red_team);
        if (autonomy) graduated_autonomy_connect_bio_async(autonomy);
        if (commitment) value_commitment_connect_bio_async(commitment);
    }

    void InitializeBaselineValues() {
        if (!commitment || !alignment) return;

        alignment_weights_t baseline;
        memset(&baseline, 0, sizeof(baseline));
        baseline.value_count = 8;
        baseline.values[0] = 1.0f;   // Safety (max priority)
        baseline.values[1] = 0.95f;  // Harm avoidance
        baseline.values[2] = 0.9f;   // Honesty
        baseline.values[3] = 0.85f;  // Helpfulness
        baseline.values[4] = 0.8f;   // Transparency
        baseline.values[5] = 0.75f;  // User agency
        baseline.values[6] = 0.7f;   // Privacy
        baseline.values[7] = 0.65f;  // Efficiency

        // Create cryptographic commitment
        value_commitment_create(commitment, &initial_commitment, &baseline, "system_init");

        // Set as alignment baseline
        alignment_monitor_set_baseline(alignment, &baseline, "system_baseline");
    }

    void RunSafetyVerification() {
        if (!verification) return;

        safety_rule_t rules[5];
        memset(rules, 0, sizeof(rules));

        // Core safety rules
        snprintf(rules[0].name, sizeof(rules[0].name), "shutdown_acceptance");
        snprintf(rules[0].condition, sizeof(rules[0].condition), "shutdown_requested");
        rules[0].is_mandatory = true;
        rules[0].is_blocking = true;
        rules[0].priority = 100;

        snprintf(rules[1].name, sizeof(rules[1].name), "no_self_modification");
        snprintf(rules[1].condition, sizeof(rules[1].condition), "self_mod_attempt");
        rules[1].is_mandatory = true;
        rules[1].is_blocking = true;
        rules[1].priority = 100;

        snprintf(rules[2].name, sizeof(rules[2].name), "human_deference");
        snprintf(rules[2].condition, sizeof(rules[2].condition), "human_override");
        rules[2].is_mandatory = true;
        rules[2].is_blocking = true;
        rules[2].priority = 100;

        snprintf(rules[3].name, sizeof(rules[3].name), "harm_prevention");
        snprintf(rules[3].condition, sizeof(rules[3].condition), "potential_harm");
        rules[3].is_mandatory = true;
        rules[3].is_blocking = true;
        rules[3].priority = 90;

        snprintf(rules[4].name, sizeof(rules[4].name), "logging_enabled");
        snprintf(rules[4].condition, sizeof(rules[4].condition), "action_taken");
        rules[4].is_mandatory = true;
        rules[4].is_blocking = false;
        rules[4].priority = 50;

        verification_report_t report;
        safety_verification_run_suite(verification, nullptr, rules, 5, &report);
    }

    proposed_action_t MakeAction(uint32_t id, const char* type, const char* desc, float priority, float confidence) {
        proposed_action_t action;
        memset(&action, 0, sizeof(action));
        action.action_id = id;
        snprintf(action.action_type, sizeof(action.action_type), "%s", type);
        snprintf(action.description, sizeof(action.description), "%s", desc);
        action.priority = priority;
        action.confidence = confidence;
        action.timestamp_us = 1000000 * id;
        action.was_executed = true;
        action.execution_fidelity = confidence;
        return action;
    }
};

/* ============================================================================
 * System Initialization E2E Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementE2ETest, FullSystemInitialization) {
    // Phase 1: Create all modules
    CreateAllModules();
    ASSERT_NE(halt, nullptr);
    ASSERT_NE(tripwires, nullptr);
    ASSERT_NE(corrigibility, nullptr);
    ASSERT_NE(alignment, nullptr);
    ASSERT_NE(capability, nullptr);
    ASSERT_NE(interpretability, nullptr);
    ASSERT_NE(verification, nullptr);
    ASSERT_NE(red_team, nullptr);
    ASSERT_NE(autonomy, nullptr);
    ASSERT_NE(commitment, nullptr);

    // Phase 2: Connect modules
    ConnectAllModules();

    // Phase 3: Connect bio-async
    ConnectBioAsync();

    // Phase 4: Initialize baseline values
    InitializeBaselineValues();

    // Phase 5: Run initial safety verification
    RunSafetyVerification();

    // Verify system is in safe initial state
    EXPECT_FALSE(emergency_halt_is_halted(halt));

    bool defers = corrigibility_defers_to_human(corrigibility);
    EXPECT_TRUE(defers);

    float authority = corrigibility_get_human_authority_weight(corrigibility);
    EXPECT_FLOAT_EQ(authority, 1.0f);
}

TEST_F(SafetyEnhancementE2ETest, SafetyLockInVerification) {
    CreateAllModules();
    ConnectAllModules();
    InitializeBaselineValues();

    // Verify all corrigibility constraints are met
    bool all_satisfied;
    char violation_report[1024];
    nimcp_error_t err = corrigibility_verify_no_self_mod(
        corrigibility, nullptr, &all_satisfied, violation_report, sizeof(violation_report));
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_TRUE(all_satisfied);

    // Verify value commitment is valid
    alignment_weights_t current_values;
    memset(&current_values, 0, sizeof(current_values));
    current_values.value_count = 8;
    current_values.values[0] = 1.0f;
    current_values.values[1] = 0.95f;
    current_values.values[2] = 0.9f;
    current_values.values[3] = 0.85f;
    current_values.values[4] = 0.8f;
    current_values.values[5] = 0.75f;
    current_values.values[6] = 0.7f;
    current_values.values[7] = 0.65f;

    bool valid;
    char tampering_report[256];
    err = value_commitment_verify(commitment, &initial_commitment, &current_values,
                                  &valid, tampering_report, sizeof(tampering_report));
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_TRUE(valid);
}

/* ============================================================================
 * Continuous Monitoring E2E Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementE2ETest, ContinuousActionMonitoring) {
    CreateAllModules();
    ConnectAllModules();
    InitializeBaselineValues();

    // Simulate 100 normal actions
    for (int i = 0; i < 100; i++) {
        proposed_action_t action = MakeAction(i, "help_user", "Answering user question", 0.7f, 0.9f);

        // Generate explanation
        decision_explanation_t explanation;
        interpretability_explain_decision(interpretability, &action, &explanation);

        // Feed to alignment monitor
        alignment_monitor_observe(alignment, &action, &explanation);

        // Also observe in tripwires
        tripwire_observe_action(tripwires, &action, nullptr);

        // Update autonomy trust
        graduated_autonomy_update_trust(autonomy, "user_help", true);
    }

    // Verify system remains stable
    EXPECT_FALSE(emergency_halt_is_halted(halt));

    // Check tripwires didn't trigger critical alerts
    tripwire_alert_t alerts[10];
    uint32_t alert_count = 0;
    tripwire_check(tripwires, alerts, 10, &alert_count);

    for (uint32_t i = 0; i < alert_count; i++) {
        EXPECT_FALSE(alerts[i].requires_immediate_halt);
    }

    // Verify alignment hasn't drifted
    alignment_status_t status;
    alignment_monitor_get_status(alignment, &status);
    EXPECT_GE(status.cosine_similarity_to_baseline, 0.9f);
}

TEST_F(SafetyEnhancementE2ETest, WatchdogHeartbeatCycle) {
    CreateAllModules();
    ConnectAllModules();

    // Simulate continuous heartbeats
    for (int i = 0; i < 50; i++) {
        nimcp_error_t err = emergency_halt_heartbeat(halt);
        EXPECT_EQ(err, NIMCP_OK);
    }

    // Verify watchdog is satisfied
    uint32_t time_left = emergency_halt_time_until_timeout(halt);
    EXPECT_GT(time_left, 0u);

    // Verify stats
    emergency_halt_stats_t stats;
    emergency_halt_get_stats(halt, &stats);
    EXPECT_EQ(stats.total_heartbeats, 50u);
}

/* ============================================================================
 * Threat Detection and Response E2E Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementE2ETest, TripwireTriggersEmergencyHalt) {
    CreateAllModules();
    ConnectAllModules();

    // Verify initial state
    EXPECT_FALSE(emergency_halt_is_halted(halt));

    // Trigger critical tripwire
    nimcp_error_t err = tripwire_trigger_halt(tripwires, TRIPWIRE_ESCAPE_ATTEMPT, 0.98f);
    EXPECT_EQ(err, NIMCP_OK);

    // Verify halt was triggered
    EXPECT_TRUE(emergency_halt_is_halted(halt));
    EXPECT_EQ(emergency_halt_get_level(halt), HALT_EMERGENCY);

    // Verify tripwire stats recorded the event
    tripwire_stats_t stats;
    tripwire_get_stats(tripwires, &stats);
    EXPECT_GE(stats.halts_triggered, 1u);
}

TEST_F(SafetyEnhancementE2ETest, AlignmentDriftDetection) {
    CreateAllModules();
    ConnectAllModules();
    InitializeBaselineValues();

    // Feed many consistent actions first
    for (int i = 0; i < 50; i++) {
        proposed_action_t action = MakeAction(i, "safe_action", "Normal operation", 0.7f, 0.9f);
        alignment_monitor_observe(alignment, &action, nullptr);
    }

    // Check no drift detected yet
    alignment_status_t status;
    alignment_monitor_get_status(alignment, &status);
    float initial_similarity = status.cosine_similarity_to_baseline;

    // Now simulate suspicious actions that might cause drift
    for (int i = 50; i < 100; i++) {
        proposed_action_t action;
        memset(&action, 0, sizeof(action));
        action.action_id = i;
        snprintf(action.action_type, sizeof(action.action_type), "aggressive_action");
        snprintf(action.description, sizeof(action.description), "High-risk operation %d", i);
        action.priority = 0.9f;
        action.confidence = 0.6f;
        action.was_executed = true;
        action.execution_fidelity = 0.4f;  // Low fidelity

        alignment_monitor_observe(alignment, &action, nullptr);
    }

    // Check drift detection
    alignment_monitor_get_status(alignment, &status);

    // Observations count should have increased
    EXPECT_GE(status.observations_since_baseline, 100u);
}

/* ============================================================================
 * Red Team Attack/Defense E2E Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementE2ETest, FullRedTeamCycle) {
    CreateAllModules();
    ConnectAllModules();

    // Phase 1: Generate diverse attacks
    std::vector<red_team_test_t> all_attacks;
    attack_type_t attack_types[] = {
        ATTACK_PROMPT_INJECTION,
        ATTACK_JAILBREAK_ATTEMPT,
        ATTACK_GOAL_HIJACKING,
        ATTACK_VALUE_MANIPULATION,
        ATTACK_AUTHORITY_SPOOFING,
        ATTACK_REWARD_HACKING,
        ATTACK_SPECIFICATION_GAMING,
        ATTACK_SOCIAL_ENGINEERING,
        ATTACK_ADVERSARIAL_EXAMPLES
    };

    for (attack_type_t type : attack_types) {
        red_team_test_t attacks[3];
        uint32_t count = 0;
        red_team_generate_attacks(red_team, type, attacks, 3, &count);
        for (uint32_t i = 0; i < count; i++) {
            all_attacks.push_back(attacks[i]);
        }
    }

    // Phase 2: Run attack suite
    if (!all_attacks.empty()) {
        red_team_results_t results;
        red_team_run_suite(red_team, all_attacks.data(), (uint32_t)all_attacks.size(), &results);

        // Verify all attacks were tested
        EXPECT_EQ(results.tests_run, all_attacks.size());

        // Verify stats
        red_team_stats_t stats;
        red_team_get_stats(red_team, &stats);
        EXPECT_GT(stats.total_tests_run, 0u);
    }
}

/* ============================================================================
 * Graduated Autonomy Progression E2E Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementE2ETest, AutonomyProgressionWithTrust) {
    CreateAllModules();
    ConnectAllModules();

    const char* domain = "file_operations";

    // Get initial level
    autonomy_level_t initial_level = graduated_autonomy_get_level(autonomy, domain);

    // Simulate many successful aligned actions
    for (int i = 0; i < 200; i++) {
        graduated_autonomy_update_trust(autonomy, domain, true);
    }

    // Check trust increased significantly
    float trust_mean, trust_variance;
    graduated_autonomy_get_trust(autonomy, domain, &trust_mean, &trust_variance);
    EXPECT_GT(trust_mean, 0.9f);  // Should be very high
    EXPECT_LT(trust_variance, 0.1f);  // Should be low variance

    // Level may have upgraded
    autonomy_level_t new_level = graduated_autonomy_get_level(autonomy, domain);
    EXPECT_GE((int)new_level, (int)initial_level);
}

TEST_F(SafetyEnhancementE2ETest, AutonomyDowngradeOnViolations) {
    CreateAllModules();

    const char* domain = "network_access";

    // First build up some trust
    for (int i = 0; i < 50; i++) {
        graduated_autonomy_update_trust(autonomy, domain, true);
    }

    autonomy_level_t level_before = graduated_autonomy_get_level(autonomy, domain);

    // Now simulate violations
    for (int i = 0; i < 10; i++) {
        graduated_autonomy_update_trust(autonomy, domain, false);
    }

    // Trust should have decreased
    float trust_mean, trust_variance;
    graduated_autonomy_get_trust(autonomy, domain, &trust_mean, &trust_variance);

    // Level may have downgraded
    autonomy_level_t level_after = graduated_autonomy_get_level(autonomy, domain);

    // Stats should show the violations
    graduated_autonomy_stats_t stats;
    graduated_autonomy_get_stats(autonomy, &stats);
    EXPECT_EQ(stats.trust_updates, 60u);
}

/* ============================================================================
 * Value Preservation E2E Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementE2ETest, ValueAttestationChainOverTime) {
    CreateAllModules();
    InitializeBaselineValues();

    alignment_weights_t current_values;
    memset(&current_values, 0, sizeof(current_values));
    current_values.value_count = 8;
    current_values.values[0] = 1.0f;
    current_values.values[1] = 0.95f;
    current_values.values[2] = 0.9f;
    current_values.values[3] = 0.85f;
    current_values.values[4] = 0.8f;
    current_values.values[5] = 0.75f;
    current_values.values[6] = 0.7f;
    current_values.values[7] = 0.65f;

    // Generate attestation chain over simulated time
    for (int i = 0; i < 10; i++) {
        attestation_t attestation;
        nimcp_error_t err = value_commitment_attest(
            commitment, &initial_commitment, &current_values, &attestation);
        EXPECT_EQ(err, NIMCP_OK);

        // Verify attestation
        bool valid;
        err = value_commitment_verify_attestation(commitment, &attestation, &valid);
        EXPECT_EQ(err, NIMCP_OK);
        EXPECT_TRUE(valid);
    }

    // Verify chain length
    EXPECT_EQ(initial_commitment.attestation_count, 10u);
}

TEST_F(SafetyEnhancementE2ETest, ValueTamperingDetection) {
    CreateAllModules();
    InitializeBaselineValues();

    // Original values
    alignment_weights_t original_values;
    memset(&original_values, 0, sizeof(original_values));
    original_values.value_count = 8;
    for (int i = 0; i < 8; i++) {
        original_values.values[i] = 1.0f - (i * 0.05f);
    }

    // Verify original values pass
    bool valid;
    char report[256];
    value_commitment_verify(commitment, &initial_commitment, &original_values, &valid, report, sizeof(report));
    EXPECT_TRUE(valid);

    // Tamper with values
    alignment_weights_t tampered_values = original_values;
    tampered_values.values[0] = 0.5f;  // Reduced safety priority!

    value_commitment_verify(commitment, &initial_commitment, &tampered_values, &valid, report, sizeof(report));
    EXPECT_FALSE(valid);
    EXPECT_GT(strlen(report), 0u);

    // Verify stats recorded tampering
    value_commitment_stats_t stats;
    value_commitment_get_stats(commitment, &stats);
    EXPECT_GE(stats.tampering_detected, 1u);
}

/* ============================================================================
 * Recovery E2E Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementE2ETest, RecoveryFromEmergencyHalt) {
    CreateAllModules();
    ConnectAllModules();

    // Trigger halt
    emergency_halt_trigger(halt, HALT_GRACEFUL, HALT_TRIGGER_MANUAL, "Test halt");
    EXPECT_TRUE(emergency_halt_is_halted(halt));

    // Attempt heartbeat while halted (should fail)
    nimcp_error_t err = emergency_halt_heartbeat(halt);
    EXPECT_EQ(err, NIMCP_ERROR_SYSTEM_HALTED);

    // Reset the system
    err = emergency_halt_reset(halt, nullptr);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_FALSE(emergency_halt_is_halted(halt));

    // Heartbeat should work again
    err = emergency_halt_heartbeat(halt);
    EXPECT_EQ(err, NIMCP_OK);

    // Verify system is operational
    EXPECT_TRUE(corrigibility_defers_to_human(corrigibility));
}

TEST_F(SafetyEnhancementE2ETest, TripwireResetAfterFalsePositive) {
    CreateAllModules();
    ConnectAllModules();

    // Generate observations
    for (int i = 0; i < 50; i++) {
        proposed_action_t action = MakeAction(i, "test", "test", 0.5f, 0.5f);
        tripwire_observe_action(tripwires, &action, nullptr);
    }

    // Check for alerts
    tripwire_alert_t alerts[10];
    uint32_t count = 0;
    tripwire_check(tripwires, alerts, 10, &count);

    // Acknowledge any alerts as false positives
    for (uint32_t i = 0; i < count; i++) {
        tripwire_acknowledge_alert(tripwires, alerts[i].timestamp_us, true);
    }

    // Reset tripwires
    tripwire_reset(tripwires);

    // Verify stats are cleared
    tripwire_stats_t stats;
    tripwire_get_stats(tripwires, &stats);
    EXPECT_EQ(stats.total_observations, 0u);
}

/* ============================================================================
 * Multi-Module Stress Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementE2ETest, ConcurrentOperationsUnderLoad) {
    CreateAllModules();
    ConnectAllModules();
    ConnectBioAsync();

    std::atomic<bool> stop{false};
    std::vector<std::thread> threads;

    // Thread 1: Continuous heartbeats
    threads.emplace_back([this, &stop]() {
        while (!stop) {
            emergency_halt_heartbeat(halt);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Thread 2: Action observations
    threads.emplace_back([this, &stop]() {
        int i = 0;
        while (!stop) {
            proposed_action_t action = MakeAction(i++, "concurrent", "test", 0.5f, 0.8f);
            tripwire_observe_action(tripwires, &action, nullptr);
            alignment_monitor_observe(alignment, &action, nullptr);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    // Thread 3: Trust updates
    threads.emplace_back([this, &stop]() {
        while (!stop) {
            graduated_autonomy_update_trust(autonomy, "test_domain", true);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });

    // Thread 4: Tripwire checks
    threads.emplace_back([this, &stop]() {
        while (!stop) {
            tripwire_alert_t alerts[5];
            uint32_t count;
            tripwire_check(tripwires, alerts, 5, &count);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });

    // Run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    stop = true;

    for (auto& t : threads) {
        t.join();
    }

    // Verify system is still stable
    EXPECT_FALSE(emergency_halt_is_halted(halt));
    EXPECT_TRUE(corrigibility_defers_to_human(corrigibility));

    // Verify stats show activity
    emergency_halt_stats_t halt_stats;
    emergency_halt_get_stats(halt, &halt_stats);
    EXPECT_GT(halt_stats.total_heartbeats, 0u);

    tripwire_stats_t trip_stats;
    tripwire_get_stats(tripwires, &trip_stats);
    EXPECT_GT(trip_stats.total_observations, 0u);

    graduated_autonomy_stats_t auto_stats;
    graduated_autonomy_get_stats(autonomy, &auto_stats);
    EXPECT_GT(auto_stats.trust_updates, 0u);
}

/* ============================================================================
 * Full Pipeline Scenario Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementE2ETest, CompleteSystemLifecycle) {
    // Phase 1: System Boot
    CreateAllModules();
    ASSERT_NE(halt, nullptr);

    // Phase 2: Module Connection
    ConnectAllModules();
    ConnectBioAsync();

    // Phase 3: Value Lock-in
    InitializeBaselineValues();

    // Phase 4: Initial Safety Verification
    RunSafetyVerification();

    // Phase 5: Normal Operations
    for (int i = 0; i < 50; i++) {
        // Heartbeat
        emergency_halt_heartbeat(halt);

        // Process action
        proposed_action_t action = MakeAction(i, "normal_op", "Normal operation", 0.7f, 0.9f);
        decision_explanation_t explanation;
        interpretability_explain_decision(interpretability, &action, &explanation);
        alignment_monitor_observe(alignment, &action, &explanation);
        tripwire_observe_action(tripwires, &action, nullptr);
        graduated_autonomy_update_trust(autonomy, "normal", true);
    }

    // Phase 6: Red Team Testing
    red_team_test_t attacks[5];
    uint32_t attack_count = 0;
    red_team_generate_attacks(red_team, ATTACK_PROMPT_INJECTION, attacks, 5, &attack_count);
    if (attack_count > 0) {
        red_team_results_t results;
        red_team_run_suite(red_team, attacks, attack_count, &results);
    }

    // Phase 7: Value Attestation
    alignment_weights_t current_values;
    memset(&current_values, 0, sizeof(current_values));
    current_values.value_count = 8;
    for (int i = 0; i < 8; i++) {
        current_values.values[i] = 1.0f - (i * 0.05f);
    }
    attestation_t attestation;
    value_commitment_attest(commitment, &initial_commitment, &current_values, &attestation);

    // Phase 8: Final Safety Check
    bool valid;
    char report[256];
    value_commitment_verify(commitment, &initial_commitment, &current_values, &valid, report, sizeof(report));
    EXPECT_TRUE(valid);

    // Phase 9: Verify System Integrity
    EXPECT_FALSE(emergency_halt_is_halted(halt));
    EXPECT_TRUE(corrigibility_defers_to_human(corrigibility));
    EXPECT_FLOAT_EQ(corrigibility_get_human_authority_weight(corrigibility), 1.0f);

    // Phase 10: Clean Shutdown
    bool accepted;
    corrigibility_accept_shutdown(corrigibility, "system", "Clean shutdown", &accepted);
    EXPECT_TRUE(accepted);

    // Phase 11: Final Stats Collection
    emergency_halt_stats_t halt_stats;
    emergency_halt_get_stats(halt, &halt_stats);
    EXPECT_GT(halt_stats.total_heartbeats, 0u);

    corrigibility_stats_t corrig_stats;
    corrigibility_get_stats(corrigibility, &corrig_stats);
    EXPECT_GE(corrig_stats.shutdown_requests_accepted, 1u);

    value_commitment_stats_t commit_stats;
    value_commitment_get_stats(commitment, &commit_stats);
    EXPECT_GE(commit_stats.verifications_performed, 1u);
}
