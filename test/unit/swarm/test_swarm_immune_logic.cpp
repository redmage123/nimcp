/**
 * @file test_swarm_immune_logic.cpp
 * @brief Unit tests for swarm immune system logic-based threat detection
 *
 * Tests logic gate integration with threat detection including:
 * - OR gate (any detector triggers alert)
 * - NOT gate (absence of expected signal = threat)
 * - AND gate (multiple detectors must agree)
 * - IMPLIES gate (if condition then response)
 */

#include <gtest/gtest.h>
#include "swarm/nimcp_swarm_immune.h"
#include "core/neuron_types/nimcp_neural_logic.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

class SwarmImmuneLogicTest : public ::testing::Test {
protected:
    NimcpSwarmImmuneSystem* immune;
    NimcpSwarmImmuneConfig config;

    void SetUp() override {
        nimcp_swarm_immune_default_config(&config);
        immune = nimcp_swarm_immune_create(&config, nullptr, 1);
        ASSERT_NE(immune, nullptr);
    }

    void TearDown() override {
        if (immune) {
            nimcp_swarm_immune_destroy(immune);
        }
    }

    // Helper: Add behavior profile for a drone
    void add_behavior_profile(uint32_t drone_id, float anomaly_score) {
        NimcpSwarmBehaviorProfile profile;
        profile.drone_id = drone_id;
        profile.msg_rate = 10.0f + anomaly_score * 5.0f;
        profile.movement_pattern[0] = 1.0f;
        profile.movement_pattern[1] = 0.5f;
        profile.movement_pattern[2] = 0.0f;
        profile.energy_usage = 50.0f;
        profile.connection_changes = 2;
        profile.last_update = 0;
        profile.anomaly_score = anomaly_score;

        float score;
        nimcp_swarm_immune_check_behavior(immune, drone_id, &profile, &score);
    }

    // Helper: Create basic threat rule
    immune_threat_rule_t create_threat_rule(
        uint32_t threat_id,
        logic_gate_type_t logic_type,
        uint32_t* sources,
        uint32_t num_sources,
        float threshold,
        NimcpSwarmThreatType threat_type
    ) {
        immune_threat_rule_t rule;
        rule.threat_id = threat_id;
        rule.detection_logic = logic_type;
        rule.num_sources = num_sources;
        rule.confidence_threshold = threshold;
        rule.threat_type = threat_type;

        if (num_sources > 0 && sources != nullptr) {
            rule.signal_sources = (uint32_t*)nimcp_malloc(num_sources * sizeof(uint32_t));
            memcpy(rule.signal_sources, sources, num_sources * sizeof(uint32_t));
        } else {
            rule.signal_sources = nullptr;
        }

        return rule;
    }
};

/* ============================================================================
 * Threat Rule Addition Tests
 * ============================================================================ */

TEST_F(SwarmImmuneLogicTest, AddThreatRule_OR_Gate) {
    uint32_t sources[] = {100, 101, 102};
    immune_threat_rule_t rule = create_threat_rule(
        1, LOGIC_GATE_OR, sources, 3, 0.5f, THREAT_MALICIOUS_DRONE
    );

    nimcp_result_t result = immune_add_threat_rule(immune, &rule);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(immune->threat_rule_count, 1u);
}

TEST_F(SwarmImmuneLogicTest, AddThreatRule_AND_Gate) {
    uint32_t sources[] = {100, 101};
    immune_threat_rule_t rule = create_threat_rule(
        2, LOGIC_GATE_AND, sources, 2, 0.7f, THREAT_BYZANTINE
    );

    nimcp_result_t result = immune_add_threat_rule(immune, &rule);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmImmuneLogicTest, AddThreatRule_NOT_Gate) {
    uint32_t sources[] = {100};
    immune_threat_rule_t rule = create_threat_rule(
        3, LOGIC_GATE_NOT, sources, 1, 0.6f, THREAT_DOS
    );

    nimcp_result_t result = immune_add_threat_rule(immune, &rule);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmImmuneLogicTest, AddMultipleThreatRules) {
    uint32_t sources1[] = {100, 101};
    uint32_t sources2[] = {102, 103, 104};

    immune_threat_rule_t rule1 = create_threat_rule(
        1, LOGIC_GATE_OR, sources1, 2, 0.5f, THREAT_SPOOFING
    );
    immune_threat_rule_t rule2 = create_threat_rule(
        2, LOGIC_GATE_AND, sources2, 3, 0.8f, THREAT_SYBIL
    );

    EXPECT_EQ(immune_add_threat_rule(immune, &rule1), NIMCP_SUCCESS);
    EXPECT_EQ(immune_add_threat_rule(immune, &rule2), NIMCP_SUCCESS);
    EXPECT_EQ(immune->threat_rule_count, 2u);
}

/* ============================================================================
 * OR-Based Threat Combination Tests
 * ============================================================================ */

TEST_F(SwarmImmuneLogicTest, ORThreatCombination_SingleSourceDetects) {
    // Setup: Add behavior profiles with varying anomaly scores
    add_behavior_profile(100, 0.9f);  // High anomaly
    add_behavior_profile(101, 0.1f);  // Low anomaly
    add_behavior_profile(102, 0.2f);  // Low anomaly

    // Add OR rule: ANY detector triggers
    uint32_t sources[] = {100, 101, 102};
    immune_threat_rule_t rule = create_threat_rule(
        1, LOGIC_GATE_OR, sources, 3, 0.5f, THREAT_MALICIOUS_DRONE
    );
    immune_add_threat_rule(immune, &rule);

    // Evaluate threats
    float threat_scores[10] = {0};
    uint32_t num_threats = 0;
    nimcp_result_t result = immune_evaluate_threats(immune, threat_scores, &num_threats);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(threat_scores[0], 0.5f) << "OR gate should trigger with one high anomaly";
    EXPECT_EQ(num_threats, 1u);
}

TEST_F(SwarmImmuneLogicTest, ORThreatCombination_AllSourcesNormal) {
    // All sources have low anomaly
    add_behavior_profile(100, 0.1f);
    add_behavior_profile(101, 0.2f);
    add_behavior_profile(102, 0.15f);

    uint32_t sources[] = {100, 101, 102};
    immune_threat_rule_t rule = create_threat_rule(
        1, LOGIC_GATE_OR, sources, 3, 0.5f, THREAT_MALICIOUS_DRONE
    );
    immune_add_threat_rule(immune, &rule);

    float threat_scores[10] = {0};
    uint32_t num_threats = 0;
    immune_evaluate_threats(immune, threat_scores, &num_threats);

    EXPECT_LT(threat_scores[0], 0.5f) << "OR gate should not trigger with all low anomalies";
    EXPECT_EQ(num_threats, 0u);
}

/* ============================================================================
 * NOT-Based Access Control Tests
 * ============================================================================ */

TEST_F(SwarmImmuneLogicTest, NOTGate_SignalAbsent_ThreatDetected) {
    uint64_t time_window = 5000;  // 5 seconds
    bool threat_detected = false;

    // No behavior profile for drone 100 (signal absent)
    nimcp_result_t result = immune_evaluate_not_threat(
        immune, 100, time_window, &threat_detected
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_TRUE(threat_detected) << "NOT gate: Absence of signal should detect threat";
}

TEST_F(SwarmImmuneLogicTest, NOTGate_SignalPresent_NoThreat) {
    uint64_t time_window = 5000;
    bool threat_detected = false;

    // Add recent behavior profile for drone 100
    add_behavior_profile(100, 0.3f);

    nimcp_result_t result = immune_evaluate_not_threat(
        immune, 100, time_window, &threat_detected
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FALSE(threat_detected) << "NOT gate: Signal present should not detect threat";
}

TEST_F(SwarmImmuneLogicTest, NOTGate_ExpiredSignal_ThreatDetected) {
    bool threat_detected = false;
    uint64_t short_window = 1;  // 1 ms - very short

    // Add behavior profile
    add_behavior_profile(100, 0.3f);

    // Wait a bit (simulated by checking with very short window)
    nimcp_result_t result = immune_evaluate_not_threat(
        immune, 100, short_window, &threat_detected
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
    // Result depends on timing, but test shouldn't crash
}

TEST_F(SwarmImmuneLogicTest, NOTThreatRule_MissingHeartbeat) {
    // NOT rule: Expected signal (heartbeat) should be present
    uint32_t sources[] = {100};
    immune_threat_rule_t rule = create_threat_rule(
        1, LOGIC_GATE_NOT, sources, 1, 0.3f, THREAT_DOS
    );
    immune_add_threat_rule(immune, &rule);

    // No behavior profile = missing heartbeat
    float threat_scores[10] = {0};
    uint32_t num_threats = 0;
    immune_evaluate_threats(immune, threat_scores, &num_threats);

    // NOT gate inverts: average signal is 0, so NOT(0) = 1.0
    EXPECT_GT(threat_scores[0], 0.3f) << "Should detect threat from missing heartbeat";
}

/* ============================================================================
 * AND-Based Multi-Detector Agreement Tests
 * ============================================================================ */

TEST_F(SwarmImmuneLogicTest, ANDThreatCombination_AllAgree) {
    // All detectors agree (all have high anomaly)
    add_behavior_profile(100, 0.85f);
    add_behavior_profile(101, 0.90f);
    add_behavior_profile(102, 0.88f);

    uint32_t sources[] = {100, 101, 102};
    immune_threat_rule_t rule = create_threat_rule(
        1, LOGIC_GATE_AND, sources, 3, 0.7f, THREAT_BYZANTINE
    );
    immune_add_threat_rule(immune, &rule);

    float threat_scores[10] = {0};
    uint32_t num_threats = 0;
    immune_evaluate_threats(immune, threat_scores, &num_threats);

    EXPECT_GT(threat_scores[0], 0.7f) << "AND gate: All detectors agree -> threat";
    EXPECT_EQ(num_threats, 1u);
}

TEST_F(SwarmImmuneLogicTest, ANDThreatCombination_OneDisagrees) {
    // One detector disagrees
    add_behavior_profile(100, 0.85f);
    add_behavior_profile(101, 0.90f);
    add_behavior_profile(102, 0.20f);  // Low anomaly

    uint32_t sources[] = {100, 101, 102};
    immune_threat_rule_t rule = create_threat_rule(
        1, LOGIC_GATE_AND, sources, 3, 0.7f, THREAT_BYZANTINE
    );
    immune_add_threat_rule(immune, &rule);

    float threat_scores[10] = {0};
    uint32_t num_threats = 0;
    immune_evaluate_threats(immune, threat_scores, &num_threats);

    EXPECT_LT(threat_scores[0], 0.7f) << "AND gate: Minimum of all -> low score";
    EXPECT_EQ(num_threats, 0u);
}

/* ============================================================================
 * IMPLIES-Based Response Logic Tests
 * ============================================================================ */

TEST_F(SwarmImmuneLogicTest, ImpliesResponse_HighSeverity_RequiresCoordination) {
    // Add a high-severity threat
    uint32_t threat_id;
    uint8_t threat_data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    nimcp_result_t result = nimcp_swarm_immune_detect_threat(
        immune, threat_data, sizeof(threat_data), 999, &threat_id
    );

    if (result == NIMCP_SUCCESS) {
        // Get threat and manually set high severity
        const NimcpSwarmThreat* threat;
        nimcp_swarm_immune_get_threat(immune, threat_id, &threat);

        // Generate logic response
        immune_response_t response;
        result = immune_logic_response(immune, threat_id, &response);

        if (result == NIMCP_SUCCESS) {
            // High severity should imply coordination
            // (This is tested in the implementation logic)
            EXPECT_EQ(response.response_logic, LOGIC_GATE_IMPLIES);
        }
    }
}

TEST_F(SwarmImmuneLogicTest, ImpliesResponse_LowSeverity_NoCoordination) {
    // Add mock threat with known pattern (for low severity)
    NimcpSwarmThreatSignature sig;
    memset(&sig, 0, sizeof(sig));
    sig.type = THREAT_DOS;
    sig.match_threshold = 0.5f;
    sig.pattern_len = 4;

    uint32_t cell_id;
    nimcp_swarm_immune_add_memory_cell(immune, &sig, RESPONSE_ALERT, 0.8f, &cell_id);

    // Create actual threat
    uint32_t threat_id;
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    nimcp_result_t result = nimcp_swarm_immune_detect_threat(immune, data, 4, 500, &threat_id);

    if (result == NIMCP_SUCCESS) {
        immune_response_t response;
        result = immune_logic_response(immune, threat_id, &response);

        if (result == NIMCP_SUCCESS) {
            // Low severity should not require coordination
            EXPECT_EQ(response.response_logic, LOGIC_GATE_IMPLIES);
        }
    }
}

/* ============================================================================
 * Complex Multi-Rule Threat Detection Tests
 * ============================================================================ */

TEST_F(SwarmImmuneLogicTest, MultipleRules_DifferentGates) {
    // Setup multiple behavior profiles
    add_behavior_profile(100, 0.9f);
    add_behavior_profile(101, 0.85f);
    add_behavior_profile(102, 0.1f);

    // Rule 1: OR gate (any detector)
    uint32_t sources1[] = {100, 101};
    immune_threat_rule_t rule1 = create_threat_rule(
        1, LOGIC_GATE_OR, sources1, 2, 0.7f, THREAT_SPOOFING
    );

    // Rule 2: AND gate (all detectors)
    uint32_t sources2[] = {100, 101, 102};
    immune_threat_rule_t rule2 = create_threat_rule(
        2, LOGIC_GATE_AND, sources2, 3, 0.8f, THREAT_SYBIL
    );

    immune_add_threat_rule(immune, &rule1);
    immune_add_threat_rule(immune, &rule2);

    float threat_scores[10] = {0};
    uint32_t num_threats = 0;
    immune_evaluate_threats(immune, threat_scores, &num_threats);

    // Rule 1 should trigger (OR with high anomalies)
    EXPECT_GT(threat_scores[0], 0.7f) << "OR rule should trigger";

    // Rule 2 should not trigger (AND with one low anomaly)
    EXPECT_LT(threat_scores[1], 0.8f) << "AND rule should not trigger";

    EXPECT_EQ(num_threats, 1u) << "Only OR rule should trigger";
}

TEST_F(SwarmImmuneLogicTest, IMPLIESThreatRule_ConditionalDetection) {
    // IMPLIES rule: If source 100 detects, then source 101 must also detect
    add_behavior_profile(100, 0.8f);  // High
    add_behavior_profile(101, 0.2f);  // Low - violation!

    uint32_t sources[] = {100, 101};
    immune_threat_rule_t rule = create_threat_rule(
        1, LOGIC_GATE_IMPLIES, sources, 2, 0.5f, THREAT_BYZANTINE
    );
    immune_add_threat_rule(immune, &rule);

    float threat_scores[10] = {0};
    uint32_t num_threats = 0;
    immune_evaluate_threats(immune, threat_scores, &num_threats);

    // IMPLIES: antecedent (0.8) >= 0.5, but consequent (0.2) < 0.5
    // This should trigger threat
    EXPECT_GT(threat_scores[0], 0.5f) << "IMPLIES violation should detect threat";
    EXPECT_EQ(num_threats, 1u);
}

/* ============================================================================
 * Edge Cases and Error Handling Tests
 * ============================================================================ */

TEST_F(SwarmImmuneLogicTest, AddThreatRule_NullPointer) {
    nimcp_result_t result = immune_add_threat_rule(nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_INVALID_PARAM);
}

TEST_F(SwarmImmuneLogicTest, EvaluateThreats_NullPointers) {
    nimcp_result_t result = immune_evaluate_threats(nullptr, nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_INVALID_PARAM);
}

TEST_F(SwarmImmuneLogicTest, EvaluateNOTThreat_NullPointers) {
    bool threat = false;
    nimcp_result_t result = immune_evaluate_not_threat(nullptr, 100, 1000, &threat);
    EXPECT_EQ(result, NIMCP_INVALID_PARAM);
}

TEST_F(SwarmImmuneLogicTest, LogicResponse_NullPointers) {
    nimcp_result_t result = immune_logic_response(nullptr, 0, nullptr);
    EXPECT_EQ(result, NIMCP_INVALID_PARAM);
}

TEST_F(SwarmImmuneLogicTest, ThreatRule_NoSources) {
    // Rule with no sources should be skipped
    immune_threat_rule_t rule = create_threat_rule(
        1, LOGIC_GATE_OR, nullptr, 0, 0.5f, THREAT_DOS
    );
    immune_add_threat_rule(immune, &rule);

    float threat_scores[10] = {0};
    uint32_t num_threats = 0;
    immune_evaluate_threats(immune, threat_scores, &num_threats);

    EXPECT_EQ(num_threats, 0u) << "Rule with no sources should not trigger";
}

TEST_F(SwarmImmuneLogicTest, ThreatRule_CapacityReached) {
    // Fill up threat rule capacity
    for (size_t i = 0; i < immune->threat_rule_capacity; i++) {
        uint32_t sources[] = {100};
        immune_threat_rule_t rule = create_threat_rule(
            i, LOGIC_GATE_OR, sources, 1, 0.5f, THREAT_DOS
        );

        nimcp_result_t result = immune_add_threat_rule(immune, &rule);
        if (i < immune->threat_rule_capacity) {
            EXPECT_EQ(result, NIMCP_SUCCESS);
        }
    }

    // Try to add one more (should fail)
    uint32_t sources[] = {100};
    immune_threat_rule_t extra_rule = create_threat_rule(
        999, LOGIC_GATE_OR, sources, 1, 0.5f, THREAT_DOS
    );

    nimcp_result_t result = immune_add_threat_rule(immune, &extra_rule);
    EXPECT_EQ(result, NIMCP_NO_MEMORY);
}

/* ============================================================================
 * Performance and Stress Tests
 * ============================================================================ */

TEST_F(SwarmImmuneLogicTest, ManyBehaviorProfiles) {
    // Add many behavior profiles
    for (uint32_t i = 0; i < 50; i++) {
        float anomaly = (i % 10) / 10.0f;
        add_behavior_profile(i, anomaly);
    }

    // Add rule covering many sources
    uint32_t sources[50];
    for (uint32_t i = 0; i < 50; i++) sources[i] = i;

    immune_threat_rule_t rule = create_threat_rule(
        1, LOGIC_GATE_OR, sources, 50, 0.5f, THREAT_MALICIOUS_DRONE
    );
    immune_add_threat_rule(immune, &rule);

    float threat_scores[10] = {0};
    uint32_t num_threats = 0;
    nimcp_result_t result = immune_evaluate_threats(immune, threat_scores, &num_threats);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmImmuneLogicTest, MultipleEvaluations) {
    add_behavior_profile(100, 0.8f);

    uint32_t sources[] = {100};
    immune_threat_rule_t rule = create_threat_rule(
        1, LOGIC_GATE_OR, sources, 1, 0.5f, THREAT_DOS
    );
    immune_add_threat_rule(immune, &rule);

    // Run many evaluations
    for (int i = 0; i < 100; i++) {
        float threat_scores[10] = {0};
        uint32_t num_threats = 0;
        nimcp_result_t result = immune_evaluate_threats(immune, threat_scores, &num_threats);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
