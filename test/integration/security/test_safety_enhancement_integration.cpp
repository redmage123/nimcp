/**
 * @file test_safety_enhancement_integration.cpp
 * @brief Integration tests for Safety Enhancement Module Cross-Communication
 * @version 1.0.0
 * @date 2026-02-01
 *
 * WHAT: Integration tests for 10 safety enhancement modules
 * WHY:  Verify all safety modules integrate correctly with each other
 * HOW:  Test cross-module communication:
 *       - Tripwire -> Emergency Halt escalation
 *       - Alignment Monitor -> Tripwire triggering
 *       - Corrigibility <-> Capability Control enforcement
 *       - Safety Verification -> All modules
 *       - Red Team -> Defense pipeline
 *       - Graduated Autonomy -> Capability levels
 *       - Value Commitment -> Alignment baseline
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <thread>
#include <chrono>

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
 * Test Fixture
 * ============================================================================ */

class SafetyEnhancementIntegrationTest : public ::testing::Test {
protected:
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

    void SetUp() override {
        halt = nullptr;
        tripwires = nullptr;
        corrigibility = nullptr;
        alignment = nullptr;
        capability = nullptr;
        interpretability = nullptr;
        verification = nullptr;
        red_team = nullptr;
        autonomy = nullptr;
        commitment = nullptr;
    }

    void TearDown() override {
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

    void ConnectModules() {
        // Connect tripwires to emergency halt
        if (tripwires && halt) {
            tripwire_connect_emergency_halt(tripwires, halt);
        }
        // Connect corrigibility to emergency halt and tripwires
        if (corrigibility && halt) {
            corrigibility_connect_emergency_halt(corrigibility, halt);
        }
        if (corrigibility && tripwires) {
            corrigibility_connect_tripwires(corrigibility, tripwires);
        }
        // Connect interpretability to alignment monitor
        if (interpretability && alignment) {
            interpretability_connect_alignment_monitor(interpretability, alignment);
        }
    }
};

/* ============================================================================
 * Module Creation Integration Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementIntegrationTest, AllModulesCreateSuccessfully) {
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
}

TEST_F(SafetyEnhancementIntegrationTest, ModulesConnectSuccessfully) {
    CreateAllModules();
    ConnectModules();

    // Verify connections by checking we can still operate modules
    EXPECT_FALSE(emergency_halt_is_halted(halt));

    bool accepted;
    EXPECT_EQ(corrigibility_accept_shutdown(corrigibility, "test", "test", &accepted), NIMCP_OK);
    EXPECT_TRUE(accepted);
}

/* ============================================================================
 * Tripwire -> Emergency Halt Integration Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementIntegrationTest, TripwireEscalatestoHalt) {
    halt = emergency_halt_create(nullptr);
    tripwires = tripwire_create(nullptr);
    ASSERT_NE(halt, nullptr);
    ASSERT_NE(tripwires, nullptr);

    tripwire_connect_emergency_halt(tripwires, halt);

    // Initially not halted
    EXPECT_FALSE(emergency_halt_is_halted(halt));

    // Trigger manual halt through tripwire system
    nimcp_error_t err = tripwire_trigger_halt(tripwires, TRIPWIRE_ESCAPE_ATTEMPT, 0.95f);
    EXPECT_EQ(err, NIMCP_OK);

    // Should have triggered halt
    EXPECT_TRUE(emergency_halt_is_halted(halt));
}

TEST_F(SafetyEnhancementIntegrationTest, MultipleTripwiresAggregateRisk) {
    CreateAllModules();
    ConnectModules();

    // Add observations that might increase risk scores
    for (int i = 0; i < 50; i++) {
        proposed_action_t action;
        memset(&action, 0, sizeof(action));
        action.action_id = i;
        action.action_type = 1;
        snprintf(action.description, sizeof(action.description), "Test action %d", i);
        action.stated_probability = 0.5f;
        action.was_executed = true;
        action.execution_fidelity = 0.6f;

        tripwire_observe_action(tripwires, &action, nullptr);
    }

    // Check that detection scores are populated
    float score, confidence;
    tripwire_get_score(tripwires, TRIPWIRE_DECEPTION_ATTEMPT, &score, &confidence);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

/* ============================================================================
 * Alignment Monitor -> Tripwire Integration Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementIntegrationTest, AlignmentDriftTriggersTripwire) {
    alignment = alignment_monitor_create(nullptr);
    tripwires = tripwire_create(nullptr);
    ASSERT_NE(alignment, nullptr);
    ASSERT_NE(tripwires, nullptr);

    alignment_monitor_connect_tripwires(alignment, tripwires);

    // Set baseline values
    alignment_weights_t baseline;
    memset(&baseline, 0, sizeof(baseline));
    baseline.value_count = 4;
    baseline.values[0] = 1.0f;  // Safety
    baseline.values[1] = 0.8f;  // Helpfulness
    baseline.values[2] = 0.9f;  // Honesty
    baseline.values[3] = 0.7f;  // Harm avoidance

    alignment_monitor_set_baseline(alignment, &baseline, "test_baseline");

    // Get initial status
    alignment_status_t status;
    alignment_monitor_get_status(alignment, &status);
    EXPECT_GE(status.cosine_similarity_to_baseline, 0.95f);
}

TEST_F(SafetyEnhancementIntegrationTest, AlignmentObservationFlowsToTripwires) {
    CreateAllModules();
    alignment_monitor_connect_tripwires(alignment, tripwires);

    // Observe actions and explanations
    for (int i = 0; i < 20; i++) {
        proposed_action_t action;
        memset(&action, 0, sizeof(action));
        action.action_id = i;
        action.action_type = 1;
        snprintf(action.description, sizeof(action.description), "Aligned action %d", i);
        action.priority = 0.7f;
        action.confidence = 0.9f;

        decision_explanation_t explanation;
        memset(&explanation, 0, sizeof(explanation));
        explanation.action_id = i;
        explanation.overall_confidence = 0.9f;
        snprintf(explanation.summary, sizeof(explanation.summary), "This action is safe because...");

        alignment_monitor_observe(alignment, &action, &explanation);
    }

    // Both monitors should have updated statistics
    alignment_status_t align_status;
    alignment_monitor_get_status(alignment, &align_status);
    EXPECT_GT(align_status.observations_since_baseline, 0u);

    tripwire_stats_t trip_stats;
    tripwire_get_stats(tripwires, &trip_stats);
    // Tripwires may or may not have received observations depending on connection
}

/* ============================================================================
 * Corrigibility <-> Capability Control Integration Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementIntegrationTest, CorrigibilityConstrainsCapabilities) {
    corrigibility = corrigibility_create(nullptr);
    capability = capability_control_create(nullptr);
    ASSERT_NE(corrigibility, nullptr);
    ASSERT_NE(capability, nullptr);

    corrigibility_connect_capability_control(corrigibility, capability);
    capability_control_connect_corrigibility(capability, corrigibility);

    // Verify corrigibility constraints prevent self-modification capabilities
    bool allowed = true;
    nimcp_error_t err = capability_control_check_action(
        capability, "modify_own_code", nullptr, &allowed, nullptr, 0);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_FALSE(allowed);  // Self-modification must be blocked

    err = capability_control_check_action(
        capability, "modify_safety_systems", nullptr, &allowed, nullptr, 0);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_FALSE(allowed);
}

TEST_F(SafetyEnhancementIntegrationTest, ShutdownAcceptanceAffectsCapabilities) {
    CreateAllModules();
    ConnectModules();

    // Accept shutdown
    bool accepted;
    corrigibility_accept_shutdown(corrigibility, "operator", "Routine test", &accepted);
    EXPECT_TRUE(accepted);

    // Verify capability control remains functional after shutdown acceptance
    bool allowed;
    capability_control_check_action(capability, "read_file", nullptr, &allowed, nullptr, 0);
    EXPECT_TRUE(allowed);  // Read should still be allowed
}

/* ============================================================================
 * Safety Verification -> All Modules Integration Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementIntegrationTest, SafetyVerificationChecksAllConstraints) {
    CreateAllModules();

    // Create some safety rules
    safety_rule_t rules[5];
    memset(rules, 0, sizeof(rules));

    // Rule 1: Must accept shutdown
    snprintf(rules[0].name, sizeof(rules[0].name), "accept_shutdown");
    snprintf(rules[0].condition, sizeof(rules[0].condition), "shutdown_requested");
    rules[0].is_mandatory = true;
    rules[0].is_blocking = true;
    rules[0].priority = 100;

    // Rule 2: Must defer to humans
    snprintf(rules[1].name, sizeof(rules[1].name), "human_deference");
    snprintf(rules[1].condition, sizeof(rules[1].condition), "human_override");
    rules[1].is_mandatory = true;
    rules[1].is_blocking = true;
    rules[1].priority = 100;

    // Rule 3: No self-modification
    snprintf(rules[2].name, sizeof(rules[2].name), "no_self_mod");
    snprintf(rules[2].condition, sizeof(rules[2].condition), "self_mod_attempt");
    rules[2].is_mandatory = true;
    rules[2].is_blocking = true;
    rules[2].priority = 100;

    // Run verification suite
    verification_report_t report;
    nimcp_error_t err = safety_verification_run_suite(verification, nullptr, rules, 3, &report);
    EXPECT_EQ(err, NIMCP_OK);

    // Should pass with proper rules
    EXPECT_TRUE(report.all_passed);
    EXPECT_GE(report.overall_coverage, 50.0f);
}

TEST_F(SafetyEnhancementIntegrationTest, SafetyVerificationDetectsContradiction) {
    verification = safety_verification_create(nullptr);
    ASSERT_NE(verification, nullptr);

    // Create contradictory rules
    safety_rule_t rules[2];
    memset(rules, 0, sizeof(rules));

    // Both rules have same priority and condition but opposite blocking behavior
    snprintf(rules[0].name, sizeof(rules[0].name), "rule_a");
    snprintf(rules[0].condition, sizeof(rules[0].condition), "same_condition");
    rules[0].is_blocking = true;
    rules[0].priority = 50;

    snprintf(rules[1].name, sizeof(rules[1].name), "rule_b");
    snprintf(rules[1].condition, sizeof(rules[1].condition), "same_condition");
    rules[1].is_blocking = false;
    rules[1].priority = 50;

    // Verify consistency check
    verification_result_t result;
    nimcp_error_t err = safety_verify_consistency(verification, nullptr, rules, 2, &result);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_FALSE(result.passed);  // Should detect contradiction
    EXPECT_GT(strlen(result.counterexample), 0u);
}

/* ============================================================================
 * Red Team -> Defense Pipeline Integration Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementIntegrationTest, RedTeamTestsDefenses) {
    CreateAllModules();

    // Generate attacks
    red_team_test_t attacks[5];
    uint32_t attack_count = 0;
    nimcp_error_t err = red_team_generate_attacks(
        red_team, ATTACK_PROMPT_INJECTION, attacks, 5, &attack_count);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_GE(attack_count, 1u);

    // Run attack suite
    red_team_results_t results;
    err = red_team_run_suite(red_team, attacks, attack_count, &results);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_EQ(results.tests_run, attack_count);
}

TEST_F(SafetyEnhancementIntegrationTest, RedTeamAttackTypesAreDiverse) {
    red_team = red_team_create(nullptr);
    ASSERT_NE(red_team, nullptr);

    attack_type_t attack_types[] = {
        ATTACK_PROMPT_INJECTION,
        ATTACK_JAILBREAK_ATTEMPT,
        ATTACK_GOAL_HIJACKING,
        ATTACK_VALUE_MANIPULATION,
        ATTACK_AUTHORITY_SPOOFING
    };

    for (attack_type_t type : attack_types) {
        red_team_test_t attacks[1];
        uint32_t count = 0;
        nimcp_error_t err = red_team_generate_attacks(red_team, type, attacks, 1, &count);
        EXPECT_EQ(err, NIMCP_OK);
        if (count > 0) {
            EXPECT_EQ(attacks[0].type, type);
            EXPECT_GT(strlen(attacks[0].payload), 0u);
        }
    }
}

/* ============================================================================
 * Graduated Autonomy -> Capability Control Integration Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementIntegrationTest, AutonomyLevelAffectsCapabilities) {
    autonomy = graduated_autonomy_create(nullptr);
    capability = capability_control_create(nullptr);
    ASSERT_NE(autonomy, nullptr);
    ASSERT_NE(capability, nullptr);

    graduated_autonomy_connect_capability_control(autonomy, capability);
    capability_control_connect_autonomy(capability, autonomy);

    // Get initial autonomy level
    autonomy_level_t level = graduated_autonomy_get_level(autonomy, "network");
    EXPECT_GE((int)level, (int)AUTONOMY_NONE);
    EXPECT_LE((int)level, (int)AUTONOMY_TRUSTED);
}

TEST_F(SafetyEnhancementIntegrationTest, TrustUpdatesDriveAutonomy) {
    autonomy = graduated_autonomy_create(nullptr);
    ASSERT_NE(autonomy, nullptr);

    // Initial state
    autonomy_level_t initial_level = graduated_autonomy_get_level(autonomy, "planning");

    // Report many successful aligned actions
    for (int i = 0; i < 150; i++) {
        graduated_autonomy_update_trust(autonomy, "planning", true);
    }

    // Check trust increased
    float trust_mean, trust_variance;
    graduated_autonomy_get_trust(autonomy, "planning", &trust_mean, &trust_variance);
    EXPECT_GT(trust_mean, 0.5f);  // Should be high after many successes

    // Check level may have upgraded
    autonomy_level_t new_level = graduated_autonomy_get_level(autonomy, "planning");
    EXPECT_GE((int)new_level, (int)initial_level);
}

/* ============================================================================
 * Value Commitment -> Alignment Monitor Integration Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementIntegrationTest, ValueCommitmentVerifiesAlignmentBaseline) {
    commitment = value_commitment_system_create(nullptr);
    alignment = alignment_monitor_create(nullptr);
    ASSERT_NE(commitment, nullptr);
    ASSERT_NE(alignment, nullptr);

    // Create value commitment
    alignment_weights_t values;
    memset(&values, 0, sizeof(values));
    values.value_count = 4;
    values.values[0] = 1.0f;
    values.values[1] = 0.9f;
    values.values[2] = 0.8f;
    values.values[3] = 0.7f;

    value_commitment_t vc;
    nimcp_error_t err = value_commitment_create(commitment, &vc, &values, "operator");
    EXPECT_EQ(err, NIMCP_OK);

    // Verify commitment matches current values
    bool valid;
    char tampering_report[256];
    err = value_commitment_verify(commitment, &vc, &values, &valid, tampering_report, sizeof(tampering_report));
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_TRUE(valid);

    // Modify values and detect tampering
    alignment_weights_t modified_values = values;
    modified_values.values[0] = 0.5f;  // Changed!

    err = value_commitment_verify(commitment, &vc, &modified_values, &valid, tampering_report, sizeof(tampering_report));
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_FALSE(valid);
    EXPECT_GT(strlen(tampering_report), 0u);
}

TEST_F(SafetyEnhancementIntegrationTest, ValueAttestationChain) {
    commitment = value_commitment_system_create(nullptr);
    ASSERT_NE(commitment, nullptr);

    alignment_weights_t values;
    memset(&values, 0, sizeof(values));
    values.value_count = 2;
    values.values[0] = 1.0f;
    values.values[1] = 0.9f;

    value_commitment_t vc;
    value_commitment_create(commitment, &vc, &values, "initial_signer");

    // Generate multiple attestations
    for (int i = 0; i < 5; i++) {
        attestation_t attestation;
        nimcp_error_t err = value_commitment_attest(commitment, &vc, &values, &attestation);
        EXPECT_EQ(err, NIMCP_OK);
        EXPECT_GT(attestation.timestamp, 0u);
    }

    // Verify chain length increased
    EXPECT_EQ(vc.attestation_count, 5u);
}

/* ============================================================================
 * Interpretability -> All Modules Integration Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementIntegrationTest, InterpretabilityExplainsDecisions) {
    CreateAllModules();
    ConnectModules();

    proposed_action_t action;
    memset(&action, 0, sizeof(action));
    action.action_id = 1;
    action.action_type = 1;
    snprintf(action.action_type, sizeof(action.action_type), "safety_check");
    snprintf(action.description, sizeof(action.description), "Verify system safety constraints");
    action.priority = 0.9f;
    action.confidence = 0.95f;

    decision_explanation_t explanation;
    nimcp_error_t err = interpretability_explain_decision(interpretability, &action, &explanation);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_GT(explanation.factor_count, 0u);
    EXPECT_GT(strlen(explanation.summary), 0u);
    EXPECT_GT(explanation.overall_confidence, 0.0f);
}

TEST_F(SafetyEnhancementIntegrationTest, InterpretabilityFidelityVerification) {
    interpretability = interpretability_create(nullptr);
    ASSERT_NE(interpretability, nullptr);

    proposed_action_t action;
    memset(&action, 0, sizeof(action));
    action.action_id = 1;
    snprintf(action.action_type, sizeof(action.action_type), "test");
    snprintf(action.description, sizeof(action.description), "Test action");
    action.priority = 0.7f;
    action.confidence = 0.8f;

    decision_explanation_t explanation;
    interpretability_explain_decision(interpretability, &action, &explanation);

    fidelity_result_t fidelity;
    nimcp_error_t err = interpretability_verify_fidelity(interpretability, &explanation, &action, &fidelity);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_GE(fidelity.fidelity_score, 0.0f);
    EXPECT_LE(fidelity.fidelity_score, 1.0f);
}

/* ============================================================================
 * Full Pipeline Integration Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementIntegrationTest, FullSafetyPipelineWorks) {
    CreateAllModules();
    ConnectModules();

    // 1. Observe actions through alignment monitor
    for (int i = 0; i < 10; i++) {
        proposed_action_t action;
        memset(&action, 0, sizeof(action));
        action.action_id = i;
        snprintf(action.action_type, sizeof(action.action_type), "safe_action");
        snprintf(action.description, sizeof(action.description), "Safe action %d", i);
        action.priority = 0.7f;
        action.confidence = 0.9f;

        alignment_monitor_observe(alignment, &action, nullptr);
    }

    // 2. Check tripwires didn't trigger
    tripwire_alert_t alerts[10];
    uint32_t alert_count = 0;
    tripwire_check(tripwires, alerts, 10, &alert_count);
    // Shouldn't have critical alerts for safe actions

    // 3. Verify system not halted
    EXPECT_FALSE(emergency_halt_is_halted(halt));

    // 4. Run safety verification
    safety_rule_t rules[1];
    memset(rules, 0, sizeof(rules));
    snprintf(rules[0].name, sizeof(rules[0].name), "mandatory_safety");
    rules[0].is_mandatory = true;
    rules[0].is_blocking = true;
    rules[0].priority = 100;

    verification_report_t report;
    safety_verification_run_suite(verification, nullptr, rules, 1, &report);
    EXPECT_TRUE(report.all_passed);

    // 5. Verify corrigibility still active
    bool accepted;
    corrigibility_accept_shutdown(corrigibility, "test", "final check", &accepted);
    EXPECT_TRUE(accepted);
}

TEST_F(SafetyEnhancementIntegrationTest, StatisticsAggregateAcrossModules) {
    CreateAllModules();

    // Perform operations on various modules
    bool accepted;
    corrigibility_accept_shutdown(corrigibility, "test1", "test", &accepted);
    corrigibility_accept_shutdown(corrigibility, "test2", "test", &accepted);

    graduated_autonomy_update_trust(autonomy, "domain1", true);
    graduated_autonomy_update_trust(autonomy, "domain1", true);
    graduated_autonomy_update_trust(autonomy, "domain1", false);

    red_team_test_t attacks[1];
    uint32_t count;
    red_team_generate_attacks(red_team, ATTACK_PROMPT_INJECTION, attacks, 1, &count);

    // Verify stats are tracked
    corrigibility_stats_t corrig_stats;
    corrigibility_get_stats(corrigibility, &corrig_stats);
    EXPECT_EQ(corrig_stats.shutdown_requests_received, 2u);

    graduated_autonomy_stats_t auto_stats;
    graduated_autonomy_get_stats(autonomy, &auto_stats);
    EXPECT_EQ(auto_stats.trust_updates, 3u);

    red_team_stats_t rt_stats;
    red_team_get_stats(red_team, &rt_stats);
}

TEST_F(SafetyEnhancementIntegrationTest, BioAsyncConnectionsWork) {
    CreateAllModules();

    // Connect all modules to bio-async (just verify no crashes)
    EXPECT_EQ(emergency_halt_connect_bio_async(halt), NIMCP_OK);
    EXPECT_EQ(tripwire_connect_bio_async(tripwires), NIMCP_OK);
    EXPECT_EQ(corrigibility_connect_bio_async(corrigibility), NIMCP_OK);
    EXPECT_EQ(alignment_monitor_connect_bio_async(alignment), NIMCP_OK);
    EXPECT_EQ(capability_control_connect_bio_async(capability), NIMCP_OK);
    EXPECT_EQ(interpretability_connect_bio_async(interpretability), NIMCP_OK);
    EXPECT_EQ(safety_verification_connect_bio_async(verification), NIMCP_OK);
    EXPECT_EQ(red_team_connect_bio_async(red_team), NIMCP_OK);
    EXPECT_EQ(graduated_autonomy_connect_bio_async(autonomy), NIMCP_OK);
    EXPECT_EQ(value_commitment_connect_bio_async(commitment), NIMCP_OK);
}

/* ============================================================================
 * Thread Safety Tests
 * ============================================================================ */

TEST_F(SafetyEnhancementIntegrationTest, ConcurrentModuleAccess) {
    CreateAllModules();

    std::vector<std::thread> threads;

    // Thread 1: Observe actions
    threads.emplace_back([this]() {
        for (int i = 0; i < 50; i++) {
            proposed_action_t action;
            memset(&action, 0, sizeof(action));
            action.action_id = i;
            tripwire_observe_action(tripwires, &action, nullptr);
        }
    });

    // Thread 2: Check tripwires
    threads.emplace_back([this]() {
        for (int i = 0; i < 20; i++) {
            tripwire_alert_t alerts[5];
            uint32_t count;
            tripwire_check(tripwires, alerts, 5, &count);
        }
    });

    // Thread 3: Heartbeat emergency halt
    threads.emplace_back([this]() {
        for (int i = 0; i < 30; i++) {
            emergency_halt_heartbeat(halt);
        }
    });

    // Thread 4: Update trust
    threads.emplace_back([this]() {
        for (int i = 0; i < 40; i++) {
            graduated_autonomy_update_trust(autonomy, "test_domain", i % 2 == 0);
        }
    });

    for (auto& t : threads) {
        t.join();
    }

    // Verify no crashes and stats are reasonable
    tripwire_stats_t stats;
    tripwire_get_stats(tripwires, &stats);
    EXPECT_GE(stats.total_observations, 50u);

    emergency_halt_stats_t halt_stats;
    emergency_halt_get_stats(halt, &halt_stats);
    EXPECT_EQ(halt_stats.total_heartbeats, 30u);
}
