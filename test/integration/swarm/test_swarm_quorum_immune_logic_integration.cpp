/**
 * @file test_swarm_quorum_immune_logic_integration.cpp
 * @brief Integration tests for quorum + immune logic validation working together
 *
 * Tests the interaction between:
 * - Quorum decision-making with logic validation
 * - Immune threat detection with logic rules
 * - Logic propagation between modules via bio-async
 * - Coordinated responses to threats detected through voting
 */

#include <gtest/gtest.h>
#include "swarm/nimcp_swarm_quorum.h"
#include "swarm/nimcp_swarm_immune.h"
#include "core/neuron_types/nimcp_neural_logic.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

class SwarmQuorumImmuneIntegrationTest : public ::testing::Test {
protected:
    nimcp_swarm_quorum_t* quorum;
    NimcpSwarmImmuneSystem* immune;
    nimcp_quorum_config_t quorum_config;
    NimcpSwarmImmuneConfig immune_config;

    void SetUp() override {
        // Initialize bio-async router
        if (!bio_router_is_initialized()) {
            bio_router_init(1024, 4);
        }

        // Create quorum system
        nimcp_swarm_quorum_default_config(&quorum_config);
        quorum_config.min_quorum_size = 3;
        quorum = nimcp_swarm_quorum_create(&quorum_config, nullptr);
        ASSERT_NE(quorum, nullptr);

        // Create immune system
        nimcp_swarm_immune_default_config(&immune_config);
        immune = nimcp_swarm_immune_create(&immune_config, nullptr, 1);
        ASSERT_NE(immune, nullptr);
    }

    void TearDown() override {
        if (quorum) {
            nimcp_swarm_quorum_destroy(quorum);
        }
        if (immune) {
            nimcp_swarm_immune_destroy(immune);
        }
        if (bio_router_is_initialized()) {
            bio_router_shutdown();
        }
    }

    // Helper: Add quorum commitments
    void add_quorum_votes(nimcp_signal_type_t signal, uint32_t count, double strength) {
        for (uint32_t i = 0; i < count; i++) {
            nimcp_quorum_update_commitment(quorum, 100 + i, signal, strength);
            nimcp_quorum_broadcast_signal(quorum, 100 + i, signal, strength);
        }
    }

    // Helper: Add immune behavior profiles
    void add_immune_profiles(uint32_t start_id, uint32_t count, float anomaly_score) {
        for (uint32_t i = 0; i < count; i++) {
            NimcpSwarmBehaviorProfile profile;
            profile.drone_id = start_id + i;
            profile.msg_rate = 10.0f + anomaly_score * 5.0f;
            profile.movement_pattern[0] = 1.0f;
            profile.movement_pattern[1] = 0.5f;
            profile.movement_pattern[2] = 0.0f;
            profile.energy_usage = 50.0f;
            profile.connection_changes = 2;
            profile.last_update = 0;
            profile.anomaly_score = anomaly_score;

            float score;
            nimcp_swarm_immune_check_behavior(immune, profile.drone_id, &profile, &score);
        }
    }
};

/* ============================================================================
 * Basic Integration Tests
 * ============================================================================ */

TEST_F(SwarmQuorumImmuneIntegrationTest, QuorumAndImmuneCoexist) {
    // Both systems should work independently
    add_quorum_votes(NIMCP_SIGNAL_ATTACK, 5, 0.9);

    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    int quorum_result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(quorum_result, 1);

    // Add immune profiles
    add_immune_profiles(200, 3, 0.8f);

    uint32_t sources[] = {200, 201, 202};
    immune_threat_rule_t rule;
    rule.threat_id = 1;
    rule.detection_logic = LOGIC_GATE_OR;
    rule.num_sources = 3;
    rule.confidence_threshold = 0.5f;
    rule.threat_type = THREAT_MALICIOUS_DRONE;
    rule.signal_sources = sources;

    nimcp_result_t immune_result = immune_add_threat_rule(immune, &rule);
    EXPECT_EQ(immune_result, NIMCP_SUCCESS);

    float threat_scores[10] = {0};
    uint32_t num_threats = 0;
    immune_evaluate_threats(immune, threat_scores, &num_threats);
    EXPECT_GT(num_threats, 0u);
}

/* ============================================================================
 * Quorum-Driven Threat Detection
 * ============================================================================ */

TEST_F(SwarmQuorumImmuneIntegrationTest, QuorumDecisionTriggersImmuneScan) {
    // Scenario: Quorum votes to investigate drones
    add_quorum_votes(NIMCP_SIGNAL_ALERT, 7, 0.95);

    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.gate_type = LOGIC_GATE_AND;  // Require strong consensus

    int quorum_valid = quorum_validate_with_logic(quorum, &logic_cfg);
    ASSERT_EQ(quorum_valid, 1) << "Quorum should reach valid consensus";

    // Make quorum decision
    bool decision_made = nimcp_quorum_make_decision(
        quorum,
        NIMCP_DECISION_TARGET_SELECT,
        nullptr
    );
    ASSERT_TRUE(decision_made);

    // Based on quorum decision, scan those drones with immune system
    add_immune_profiles(100, 7, 0.6f);  // Same drones that voted

    uint32_t sources[7];
    for (int i = 0; i < 7; i++) sources[i] = 100 + i;

    immune_threat_rule_t rule;
    rule.threat_id = 1;
    rule.detection_logic = LOGIC_GATE_OR;
    rule.num_sources = 7;
    rule.confidence_threshold = 0.5f;
    rule.threat_type = THREAT_SYBIL;
    rule.signal_sources = sources;

    immune_add_threat_rule(immune, &rule);

    float threat_scores[10] = {0};
    uint32_t num_threats = 0;
    immune_evaluate_threats(immune, threat_scores, &num_threats);

    EXPECT_GT(num_threats, 0u) << "Immune should detect threats in voting drones";
}

/* ============================================================================
 * Immune-Driven Quorum Invalidation
 * ============================================================================ */

TEST_F(SwarmQuorumImmuneIntegrationTest, ImmuneThreatInvalidatesQuorum) {
    // Scenario: Immune detects compromised drone, invalidates its vote

    // Initial quorum vote
    add_quorum_votes(NIMCP_SIGNAL_ATTACK, 5, 0.9);

    // Detect threat in one of the voting drones
    add_immune_profiles(102, 1, 0.95f);  // High anomaly on drone 102

    uint32_t sources[] = {102};
    immune_threat_rule_t rule;
    rule.threat_id = 1;
    rule.detection_logic = LOGIC_GATE_OR;
    rule.num_sources = 1;
    rule.confidence_threshold = 0.7f;
    rule.threat_type = THREAT_MALICIOUS_DRONE;
    rule.signal_sources = sources;

    immune_add_threat_rule(immune, &rule);

    float threat_scores[10] = {0};
    uint32_t num_threats = 0;
    immune_evaluate_threats(immune, threat_scores, &num_threats);

    ASSERT_GT(num_threats, 0u) << "Should detect threat in drone 102";

    // Remove compromised drone from quorum
    nimcp_quorum_remove_commitment(quorum, 102);

    // Re-validate quorum without compromised drone
    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.min_agents = 4;  // Still have 4 good drones

    int quorum_valid = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(quorum_valid, 1) << "Quorum should still be valid after removing threat";
}

/* ============================================================================
 * Logic Propagation Between Modules
 * ============================================================================ */

TEST_F(SwarmQuorumImmuneIntegrationTest, LogicPropagation_ConsistencyCheck) {
    // Both modules use consistency checking

    // Quorum: Add contradicting votes
    nimcp_quorum_update_commitment(quorum, 100, NIMCP_SIGNAL_ATTACK, 0.9);
    nimcp_quorum_update_commitment(quorum, 101, NIMCP_SIGNAL_RETREAT, 0.9);
    nimcp_quorum_update_commitment(quorum, 102, NIMCP_SIGNAL_ATTACK, 0.8);

    uint32_t contradicting_agents[256];
    uint32_t quorum_contradictions = 0;
    int quorum_consistent = quorum_check_vote_consistency(
        quorum,
        contradicting_agents,
        &quorum_contradictions
    );

    EXPECT_EQ(quorum_consistent, 1) << "Quorum should detect contradictions";
    EXPECT_GT(quorum_contradictions, 0u);

    // Immune: Check if same drones show anomalous behavior
    for (uint32_t i = 0; i < quorum_contradictions && i < 3; i++) {
        uint32_t drone_id = contradicting_agents[i];
        add_immune_profiles(drone_id, 1, 0.85f);
    }

    uint32_t immune_sources[3];
    for (uint32_t i = 0; i < quorum_contradictions && i < 3; i++) {
        immune_sources[i] = contradicting_agents[i];
    }

    immune_threat_rule_t rule;
    rule.threat_id = 1;
    rule.detection_logic = LOGIC_GATE_OR;
    rule.num_sources = quorum_contradictions < 3 ? quorum_contradictions : 3;
    rule.confidence_threshold = 0.5f;
    rule.threat_type = THREAT_BYZANTINE;
    rule.signal_sources = immune_sources;

    immune_add_threat_rule(immune, &rule);

    float threat_scores[10] = {0};
    uint32_t num_threats = 0;
    immune_evaluate_threats(immune, threat_scores, &num_threats);

    EXPECT_GT(num_threats, 0u) << "Immune should confirm Byzantine behavior";
}

/* ============================================================================
 * Coordinated Response Scenarios
 * ============================================================================ */

TEST_F(SwarmQuorumImmuneIntegrationTest, CoordinatedThreatResponse) {
    // Scenario: Quorum votes on response to immune-detected threat

    // 1. Immune detects threat
    add_immune_profiles(200, 1, 0.95f);

    uint32_t sources[] = {200};
    immune_threat_rule_t rule;
    rule.threat_id = 1;
    rule.detection_logic = LOGIC_GATE_OR;
    rule.num_sources = 1;
    rule.confidence_threshold = 0.7f;
    rule.threat_type = THREAT_MALICIOUS_DRONE;
    rule.signal_sources = sources;

    immune_add_threat_rule(immune, &rule);

    float threat_scores[10] = {0};
    uint32_t num_threats = 0;
    immune_evaluate_threats(immune, threat_scores, &num_threats);
    ASSERT_GT(num_threats, 0u);

    // 2. Quorum votes on isolation response
    add_quorum_votes(NIMCP_SIGNAL_DEFEND, 6, 0.9);

    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.gate_type = LOGIC_GATE_IMPLIES;
    logic_cfg.threshold = 0.7f;

    int quorum_valid = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(quorum_valid, 1);

    // 3. Make coordinated decision
    bool decision = nimcp_quorum_make_decision(
        quorum,
        NIMCP_DECISION_TARGET_SELECT,
        nullptr
    );
    EXPECT_TRUE(decision);

    const nimcp_quorum_decision_t* last_decision = nimcp_quorum_get_last_decision(quorum);
    ASSERT_NE(last_decision, nullptr);
    EXPECT_GT(last_decision->consensus_strength, 0.7);
}

TEST_F(SwarmQuorumImmuneIntegrationTest, IMPLIES_ThreatResponse) {
    // IF high_threat THEN coordinated_response
    // Use IMPLIES logic in both modules

    // High threat detected
    add_immune_profiles(300, 3, 0.92f);

    uint32_t sources[] = {300, 301, 302};
    immune_threat_rule_t rule;
    rule.threat_id = 1;
    rule.detection_logic = LOGIC_GATE_AND;  // All agree
    rule.num_sources = 3;
    rule.confidence_threshold = 0.8f;
    rule.threat_type = THREAT_BYZANTINE;
    rule.signal_sources = sources;

    immune_add_threat_rule(immune, &rule);

    float threat_scores[10] = {0};
    uint32_t num_threats = 0;
    immune_evaluate_threats(immune, threat_scores, &num_threats);

    if (num_threats > 0 && threat_scores[0] > 0.8f) {
        // High threat detected -> require coordinated quorum response

        // Quorum votes on response with IMPLIES validation
        add_quorum_votes(NIMCP_SIGNAL_DEFEND, 8, 0.95);

        quorum_logic_config_t logic_cfg;
        quorum_logic_default_config(&logic_cfg);
        logic_cfg.gate_type = LOGIC_GATE_IMPLIES;
        logic_cfg.threshold = 0.8f;  // High threshold for high threat

        int valid = quorum_validate_with_logic(quorum, &logic_cfg);
        EXPECT_EQ(valid, 1) << "IMPLIES: High threat requires strong consensus";
    }
}

/* ============================================================================
 * NOT Gate Integration
 * ============================================================================ */

TEST_F(SwarmQuorumImmuneIntegrationTest, NOT_MissingVote_ThreatDetected) {
    // Scenario: Expected drone doesn't vote -> immune detects as threat

    // Drones 100-104 should vote
    add_quorum_votes(NIMCP_SIGNAL_EXPLORE, 4, 0.9);
    // But drone 103 is missing!

    // Check quorum
    const nimcp_drone_commitment_t* commit = nimcp_quorum_get_commitment(quorum, 103);
    if (!commit) {
        // Drone 103 missing from quorum -> check with immune NOT gate
        bool threat_detected = false;
        nimcp_result_t result = immune_evaluate_not_threat(
            immune, 103, 5000, &threat_detected
        );

        EXPECT_EQ(result, NIMCP_SUCCESS);
        EXPECT_TRUE(threat_detected) << "NOT: Missing vote should be detected as threat";
    }
}

/* ============================================================================
 * Complex Multi-Module Scenarios
 * ============================================================================ */

TEST_F(SwarmQuorumImmuneIntegrationTest, FullWorkflow_Detection_Vote_Response) {
    // 1. Immune detects multiple threats
    add_immune_profiles(400, 3, 0.88f);
    add_immune_profiles(403, 2, 0.75f);

    uint32_t sources1[] = {400, 401, 402};
    uint32_t sources2[] = {403, 404};

    immune_threat_rule_t rule1;
    rule1.threat_id = 1;
    rule1.detection_logic = LOGIC_GATE_OR;
    rule1.num_sources = 3;
    rule1.confidence_threshold = 0.7f;
    rule1.threat_type = THREAT_SYBIL;
    rule1.signal_sources = sources1;

    immune_threat_rule_t rule2;
    rule2.threat_id = 2;
    rule2.detection_logic = LOGIC_GATE_AND;
    rule2.num_sources = 2;
    rule2.confidence_threshold = 0.7f;
    rule2.threat_type = THREAT_SPOOFING;
    rule2.signal_sources = sources2;

    immune_add_threat_rule(immune, &rule1);
    immune_add_threat_rule(immune, &rule2);

    float threat_scores[10] = {0};
    uint32_t num_threats = 0;
    immune_evaluate_threats(immune, threat_scores, &num_threats);

    ASSERT_GT(num_threats, 0u) << "Should detect at least one threat";

    // 2. Healthy drones vote on response
    add_quorum_votes(NIMCP_SIGNAL_ALERT, 10, 0.93);

    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.gate_type = LOGIC_GATE_AND;
    logic_cfg.min_agents = 8;

    int valid = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(valid, 1);

    // 3. Make and finalize decision
    bool decision = nimcp_quorum_make_decision(
        quorum,
        NIMCP_DECISION_TARGET_SELECT,
        nullptr
    );
    EXPECT_TRUE(decision);

    const nimcp_quorum_decision_t* last = nimcp_quorum_get_last_decision(quorum);
    if (last) {
        nimcp_quorum_finalize_decision(quorum, quorum->decision_count - 1);
        EXPECT_TRUE(last->is_final);
    }
}

/* ============================================================================
 * Performance Tests
 * ============================================================================ */

TEST_F(SwarmQuorumImmuneIntegrationTest, ScalabilityTest) {
    // Test with many drones in both systems

    // 50 drones voting
    for (uint32_t i = 0; i < 50; i++) {
        nimcp_quorum_update_commitment(quorum, 1000 + i, NIMCP_SIGNAL_FORMATION, 0.85);
    }

    // 50 drones monitored by immune
    add_immune_profiles(1000, 50, 0.3f);

    // Validate quorum
    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.min_agents = 30;

    int quorum_valid = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_GE(quorum_valid, 0);

    // Check immune with large rule
    uint32_t sources[50];
    for (uint32_t i = 0; i < 50; i++) sources[i] = 1000 + i;

    immune_threat_rule_t rule;
    rule.threat_id = 1;
    rule.detection_logic = LOGIC_GATE_OR;
    rule.num_sources = 50;
    rule.confidence_threshold = 0.5f;
    rule.threat_type = THREAT_DOS;
    rule.signal_sources = sources;

    nimcp_result_t result = immune_add_threat_rule(immune, &rule);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    float threat_scores[10] = {0};
    uint32_t num_threats = 0;
    immune_evaluate_threats(immune, threat_scores, &num_threats);
    // Should complete without crashing
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
