/**
 * @file test_immune_threat_logic.cpp
 * @brief Unit tests for Immune System Threat Logic
 *
 * Tests the integration of neural logic gates with immune system
 * for threat detection and response generation.
 */

#include <gtest/gtest.h>
#include "swarm/nimcp_swarm_immune.h"
#include "core/neuron_types/nimcp_neural_logic.h"
#include "utils/logging/nimcp_logging.h"

class ImmuneThreatLogicTest : public ::testing::Test {
protected:
    NimcpSwarmImmuneSystem* immune;
    NimcpSwarmImmuneConfig config;

    void SetUp() override {
        nimcp_swarm_immune_default_config(&config);
        immune = nimcp_swarm_immune_create(&config, nullptr, 100);
        ASSERT_NE(immune, nullptr);
    }

    void TearDown() override {
        if (immune) {
            nimcp_swarm_immune_destroy(immune);
            immune = nullptr;
        }
    }

    // Helper: Add behavior profiles for testing
    void add_behavior_profile(uint32_t drone_id, float anomaly_score) {
        NimcpSwarmBehaviorProfile profile = {};
        profile.drone_id = drone_id;
        profile.anomaly_score = anomaly_score;
        profile.msg_rate = 10.0f;
        profile.energy_usage = 5.0f;
        profile.last_update = 1000;

        float temp_score;
        nimcp_swarm_immune_check_behavior(immune, drone_id, &profile, &temp_score);
    }

    // Helper: Add active threat for testing
    uint32_t add_test_threat(NimcpSwarmThreatType type, NimcpSwarmSeverity severity, float confidence) {
        uint8_t data[64] = {0xDE, 0xAD, 0xBE, 0xEF};
        uint32_t threat_id = 0;
        nimcp_swarm_immune_detect_threat(immune, data, 4, 200, &threat_id);
        return threat_id;
    }
};

// ============================================================================
// Threat Rule Addition Tests
// ============================================================================

TEST_F(ImmuneThreatLogicTest, AddThreatRuleBasic) {
    uint32_t sources[] = {1, 2, 3};
    immune_threat_rule_t rule = {};
    rule.threat_id = 1;
    rule.detection_logic = LOGIC_GATE_AND;
    rule.signal_sources = sources;
    rule.num_sources = 3;
    rule.confidence_threshold = 0.7f;
    rule.threat_type = THREAT_MALICIOUS_DRONE;

    nimcp_result_t result = immune_add_threat_rule(immune, &rule);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ImmuneThreatLogicTest, AddThreatRuleNullSystem) {
    uint32_t sources[] = {1, 2, 3};
    immune_threat_rule_t rule = {};
    rule.threat_id = 1;
    rule.detection_logic = LOGIC_GATE_AND;
    rule.signal_sources = sources;
    rule.num_sources = 3;

    nimcp_result_t result = immune_add_threat_rule(nullptr, &rule);
    EXPECT_EQ(result, NIMCP_INVALID_PARAM);
}

TEST_F(ImmuneThreatLogicTest, AddThreatRuleNullRule) {
    nimcp_result_t result = immune_add_threat_rule(immune, nullptr);
    EXPECT_EQ(result, NIMCP_INVALID_PARAM);
}

TEST_F(ImmuneThreatLogicTest, AddThreatRuleNoSources) {
    immune_threat_rule_t rule = {};
    rule.threat_id = 1;
    rule.detection_logic = LOGIC_GATE_AND;
    rule.signal_sources = nullptr;
    rule.num_sources = 0;
    rule.confidence_threshold = 0.7f;
    rule.threat_type = THREAT_MALICIOUS_DRONE;

    nimcp_result_t result = immune_add_threat_rule(immune, &rule);
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Should handle rules with no sources";
}

// ============================================================================
// AND Gate Threat Detection Tests
// ============================================================================

TEST_F(ImmuneThreatLogicTest, AndGateAllSourcesDetect) {
    // Setup: All sources detect threat (high anomaly scores)
    add_behavior_profile(1, 0.9f);
    add_behavior_profile(2, 0.85f);
    add_behavior_profile(3, 0.88f);

    uint32_t sources[] = {1, 2, 3};
    immune_threat_rule_t rule = {};
    rule.threat_id = 1;
    rule.detection_logic = LOGIC_GATE_AND;
    rule.signal_sources = sources;
    rule.num_sources = 3;
    rule.confidence_threshold = 0.8f;
    rule.threat_type = THREAT_BYZANTINE;

    immune_add_threat_rule(immune, &rule);

    float threat_scores[50] = {0};
    uint32_t num_threats = 0;
    nimcp_result_t result = immune_evaluate_threats(immune, threat_scores, &num_threats);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(num_threats, 1u) << "AND gate with all sources high should detect threat";
    EXPECT_GE(threat_scores[0], 0.8f) << "Threat score should be high";
}

TEST_F(ImmuneThreatLogicTest, AndGateOneSourceFails) {
    // Setup: One source has low anomaly score
    add_behavior_profile(1, 0.9f);
    add_behavior_profile(2, 0.2f); // Low score
    add_behavior_profile(3, 0.88f);

    uint32_t sources[] = {1, 2, 3};
    immune_threat_rule_t rule = {};
    rule.threat_id = 1;
    rule.detection_logic = LOGIC_GATE_AND;
    rule.signal_sources = sources;
    rule.num_sources = 3;
    rule.confidence_threshold = 0.8f;
    rule.threat_type = THREAT_BYZANTINE;

    immune_add_threat_rule(immune, &rule);

    float threat_scores[50] = {0};
    uint32_t num_threats = 0;
    immune_evaluate_threats(immune, threat_scores, &num_threats);

    EXPECT_EQ(num_threats, 0u) << "AND gate with one low source should not detect threat";
    EXPECT_LT(threat_scores[0], 0.8f);
}

// ============================================================================
// OR Gate Threat Detection Tests
// ============================================================================

TEST_F(ImmuneThreatLogicTest, OrGateAnySourceDetects) {
    // Setup: Only one source detects
    add_behavior_profile(1, 0.9f);  // High
    add_behavior_profile(2, 0.1f);  // Low
    add_behavior_profile(3, 0.15f); // Low

    uint32_t sources[] = {1, 2, 3};
    immune_threat_rule_t rule = {};
    rule.threat_id = 2;
    rule.detection_logic = LOGIC_GATE_OR;
    rule.signal_sources = sources;
    rule.num_sources = 3;
    rule.confidence_threshold = 0.8f;
    rule.threat_type = THREAT_JAMMING;

    immune_add_threat_rule(immune, &rule);

    float threat_scores[50] = {0};
    uint32_t num_threats = 0;
    immune_evaluate_threats(immune, threat_scores, &num_threats);

    EXPECT_EQ(num_threats, 1u) << "OR gate with one high source should detect threat";
    EXPECT_GE(threat_scores[0], 0.8f);
}

TEST_F(ImmuneThreatLogicTest, OrGateNoSourceDetects) {
    // Setup: All sources have low scores
    add_behavior_profile(1, 0.1f);
    add_behavior_profile(2, 0.2f);
    add_behavior_profile(3, 0.15f);

    uint32_t sources[] = {1, 2, 3};
    immune_threat_rule_t rule = {};
    rule.threat_id = 2;
    rule.detection_logic = LOGIC_GATE_OR;
    rule.signal_sources = sources;
    rule.num_sources = 3;
    rule.confidence_threshold = 0.8f;
    rule.threat_type = THREAT_JAMMING;

    immune_add_threat_rule(immune, &rule);

    float threat_scores[50] = {0};
    uint32_t num_threats = 0;
    immune_evaluate_threats(immune, threat_scores, &num_threats);

    EXPECT_EQ(num_threats, 0u) << "OR gate with all low sources should not detect";
}

// ============================================================================
// NOT Gate Threat Detection Tests
// ============================================================================

TEST_F(ImmuneThreatLogicTest, NotGateAbsenceDetected) {
    // Setup: All sources have low anomaly (expected signals absent)
    add_behavior_profile(1, 0.1f);
    add_behavior_profile(2, 0.15f);
    add_behavior_profile(3, 0.2f);

    uint32_t sources[] = {1, 2, 3};
    immune_threat_rule_t rule = {};
    rule.threat_id = 3;
    rule.detection_logic = LOGIC_GATE_NOT;
    rule.signal_sources = sources;
    rule.num_sources = 3;
    rule.confidence_threshold = 0.75f;
    rule.threat_type = THREAT_DOS;

    immune_add_threat_rule(immune, &rule);

    float threat_scores[50] = {0};
    uint32_t num_threats = 0;
    immune_evaluate_threats(immune, threat_scores, &num_threats);

    EXPECT_EQ(num_threats, 1u) << "NOT gate with low signals should detect threat";
    EXPECT_GE(threat_scores[0], 0.75f) << "Inverted score should be high";
}

TEST_F(ImmuneThreatLogicTest, NotGatePresenceNormal) {
    // Setup: Sources have high anomaly (expected signals present)
    add_behavior_profile(1, 0.9f);
    add_behavior_profile(2, 0.85f);
    add_behavior_profile(3, 0.88f);

    uint32_t sources[] = {1, 2, 3};
    immune_threat_rule_t rule = {};
    rule.threat_id = 3;
    rule.detection_logic = LOGIC_GATE_NOT;
    rule.signal_sources = sources;
    rule.num_sources = 3;
    rule.confidence_threshold = 0.75f;
    rule.threat_type = THREAT_DOS;

    immune_add_threat_rule(immune, &rule);

    float threat_scores[50] = {0};
    uint32_t num_threats = 0;
    immune_evaluate_threats(immune, threat_scores, &num_threats);

    EXPECT_EQ(num_threats, 0u) << "NOT gate with high signals should not detect";
}

// ============================================================================
// IMPLIES Gate Threat Detection Tests
// ============================================================================

TEST_F(ImmuneThreatLogicTest, ImpliesGateViolated) {
    // Setup: First signal high, second low (implication violated)
    add_behavior_profile(1, 0.9f);  // Antecedent high
    add_behavior_profile(2, 0.2f);  // Consequent low

    uint32_t sources[] = {1, 2};
    immune_threat_rule_t rule = {};
    rule.threat_id = 4;
    rule.detection_logic = LOGIC_GATE_IMPLIES;
    rule.signal_sources = sources;
    rule.num_sources = 2;
    rule.confidence_threshold = 0.9f;
    rule.threat_type = THREAT_INJECTION;

    immune_add_threat_rule(immune, &rule);

    float threat_scores[50] = {0};
    uint32_t num_threats = 0;
    immune_evaluate_threats(immune, threat_scores, &num_threats);

    EXPECT_EQ(num_threats, 1u) << "Violated IMPLIES should detect threat";
    EXPECT_FLOAT_EQ(threat_scores[0], 1.0f);
}

TEST_F(ImmuneThreatLogicTest, ImpliesGateHolds) {
    // Setup: If first high, then second high (implication holds)
    add_behavior_profile(1, 0.9f);
    add_behavior_profile(2, 0.85f);

    uint32_t sources[] = {1, 2};
    immune_threat_rule_t rule = {};
    rule.threat_id = 4;
    rule.detection_logic = LOGIC_GATE_IMPLIES;
    rule.signal_sources = sources;
    rule.num_sources = 2;
    rule.confidence_threshold = 0.9f;
    rule.threat_type = THREAT_INJECTION;

    immune_add_threat_rule(immune, &rule);

    float threat_scores[50] = {0};
    uint32_t num_threats = 0;
    immune_evaluate_threats(immune, threat_scores, &num_threats);

    EXPECT_EQ(num_threats, 0u) << "Valid IMPLIES should not detect threat";
}

// ============================================================================
// Logic Response Generation Tests
// ============================================================================

TEST_F(ImmuneThreatLogicTest, LogicResponseHighSeverity) {
    // Setup: First add a memory cell that matches our threat data
    uint8_t threat_data[64] = {0xDE, 0xAD};
    NimcpSwarmThreatSignature sig = {};
    memcpy(sig.pattern, threat_data, 2);
    sig.pattern_len = 2;
    sig.match_threshold = 0.5f;  // Low threshold to ensure match
    sig.type = THREAT_MALICIOUS_DRONE;  // High severity type

    uint32_t cell_id = 0;
    nimcp_swarm_immune_add_memory_cell(immune, &sig, RESPONSE_ISOLATION, 0.9f, &cell_id);

    // Now detect threat - should match the memory cell and create active threat
    uint32_t threat_id = 0;
    nimcp_swarm_immune_detect_threat(immune, threat_data, 2, 200, &threat_id);
    ASSERT_NE(threat_id, 0u) << "Threat should be detected with matching memory cell";

    immune_response_t response = {};
    nimcp_result_t result = immune_logic_response(immune, threat_id, &response);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(response.response_logic, LOGIC_GATE_IMPLIES);
    // Note: Actual coordination depends on severity set by detect_threat
}

TEST_F(ImmuneThreatLogicTest, LogicResponseNullSystem) {
    immune_response_t response = {};
    nimcp_result_t result = immune_logic_response(nullptr, 1, &response);
    EXPECT_EQ(result, NIMCP_INVALID_PARAM);
}

TEST_F(ImmuneThreatLogicTest, LogicResponseNullResponse) {
    nimcp_result_t result = immune_logic_response(immune, 1, nullptr);
    EXPECT_EQ(result, NIMCP_INVALID_PARAM);
}

TEST_F(ImmuneThreatLogicTest, LogicResponseNonexistentThreat) {
    immune_response_t response = {};
    nimcp_result_t result = immune_logic_response(immune, 9999, &response);
    EXPECT_EQ(result, NIMCP_NOT_FOUND);
}

// ============================================================================
// BBB Threat Alert Tests
// ============================================================================

TEST_F(ImmuneThreatLogicTest, BbbThreatAlertBasic) {
    // Setup: First add a memory cell that matches our threat data
    uint8_t threat_data[64] = {0xDE, 0xAD};
    NimcpSwarmThreatSignature sig = {};
    memcpy(sig.pattern, threat_data, 2);
    sig.pattern_len = 2;
    sig.match_threshold = 0.5f;
    sig.type = THREAT_INJECTION;

    uint32_t cell_id = 0;
    nimcp_swarm_immune_add_memory_cell(immune, &sig, RESPONSE_ALERT, 0.9f, &cell_id);

    // Now detect threat - should match the memory cell
    uint32_t threat_id = 0;
    nimcp_swarm_immune_detect_threat(immune, threat_data, 2, 200, &threat_id);
    ASSERT_NE(threat_id, 0u) << "Threat should be detected with matching memory cell";

    nimcp_result_t result = immune_send_bbb_threat_alert(
        immune,
        threat_id,
        SWARM_SEVERITY_HIGH
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ImmuneThreatLogicTest, BbbThreatAlertNullSystem) {
    nimcp_result_t result = immune_send_bbb_threat_alert(nullptr, 1, SWARM_SEVERITY_HIGH);
    EXPECT_EQ(result, NIMCP_INVALID_PARAM);
}

TEST_F(ImmuneThreatLogicTest, BbbThreatAlertNonexistentThreat) {
    nimcp_result_t result = immune_send_bbb_threat_alert(immune, 9999, SWARM_SEVERITY_HIGH);
    EXPECT_EQ(result, NIMCP_NOT_FOUND);
}

// ============================================================================
// NOT Gate Evaluation Tests
// ============================================================================

TEST_F(ImmuneThreatLogicTest, NotThreatAbsentSignal) {
    // Setup: No recent behavior profile for expected signal
    bool threat_detected = false;
    nimcp_result_t result = immune_evaluate_not_threat(
        immune,
        999, // Non-existent signal source
        5000, // 5 second window
        &threat_detected
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_TRUE(threat_detected) << "Absent signal should trigger NOT threat";
}

TEST_F(ImmuneThreatLogicTest, NotThreatPresentSignal) {
    // Setup: Add recent behavior profile
    add_behavior_profile(100, 0.5f);

    bool threat_detected = true; // Initialize opposite of expected
    nimcp_result_t result = immune_evaluate_not_threat(
        immune,
        100,
        5000,
        &threat_detected
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FALSE(threat_detected) << "Present signal should not trigger NOT threat";
}

TEST_F(ImmuneThreatLogicTest, NotThreatExpiredSignal) {
    // Setup: Add old behavior profile (beyond time window)
    add_behavior_profile(100, 0.5f);
    // Note: In real test, would need to manipulate timestamps

    bool threat_detected = false;
    nimcp_result_t result = immune_evaluate_not_threat(
        immune,
        100,
        1, // Very short window (1ms)
        &threat_detected
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
    // Result depends on actual timestamps
}

TEST_F(ImmuneThreatLogicTest, NotThreatNullParams) {
    bool threat_detected = false;

    // Null system
    nimcp_result_t result1 = immune_evaluate_not_threat(nullptr, 100, 5000, &threat_detected);
    EXPECT_EQ(result1, NIMCP_INVALID_PARAM);

    // Null output
    nimcp_result_t result2 = immune_evaluate_not_threat(immune, 100, 5000, nullptr);
    EXPECT_EQ(result2, NIMCP_INVALID_PARAM);
}

// ============================================================================
// Multiple Threat Rules Tests
// ============================================================================

TEST_F(ImmuneThreatLogicTest, MultipleRulesDetect) {
    // Setup behavior profiles
    add_behavior_profile(1, 0.9f);
    add_behavior_profile(2, 0.85f);
    add_behavior_profile(3, 0.2f);

    // Rule 1: AND gate (should pass)
    uint32_t sources1[] = {1, 2};
    immune_threat_rule_t rule1 = {};
    rule1.threat_id = 1;
    rule1.detection_logic = LOGIC_GATE_AND;
    rule1.signal_sources = sources1;
    rule1.num_sources = 2;
    rule1.confidence_threshold = 0.8f;
    rule1.threat_type = THREAT_MALICIOUS_DRONE;
    immune_add_threat_rule(immune, &rule1);

    // Rule 2: OR gate (should pass)
    uint32_t sources2[] = {1, 3};
    immune_threat_rule_t rule2 = {};
    rule2.threat_id = 2;
    rule2.detection_logic = LOGIC_GATE_OR;
    rule2.signal_sources = sources2;
    rule2.num_sources = 2;
    rule2.confidence_threshold = 0.8f;
    rule2.threat_type = THREAT_SYBIL;
    immune_add_threat_rule(immune, &rule2);

    float threat_scores[50] = {0};
    uint32_t num_threats = 0;
    immune_evaluate_threats(immune, threat_scores, &num_threats);

    EXPECT_EQ(num_threats, 2u) << "Both rules should detect threats";
}

TEST_F(ImmuneThreatLogicTest, EvaluateThreatsEmptyRules) {
    // No rules added
    float threat_scores[50] = {0};
    uint32_t num_threats = 0;
    nimcp_result_t result = immune_evaluate_threats(immune, threat_scores, &num_threats);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(num_threats, 0u);
}

TEST_F(ImmuneThreatLogicTest, EvaluateThreatsNullParams) {
    float threat_scores[50] = {0};
    uint32_t num_threats = 0;

    // Null system
    nimcp_result_t result1 = immune_evaluate_threats(nullptr, threat_scores, &num_threats);
    EXPECT_EQ(result1, NIMCP_INVALID_PARAM);

    // Null scores
    nimcp_result_t result2 = immune_evaluate_threats(immune, nullptr, &num_threats);
    EXPECT_EQ(result2, NIMCP_INVALID_PARAM);

    // Null count
    nimcp_result_t result3 = immune_evaluate_threats(immune, threat_scores, nullptr);
    EXPECT_EQ(result3, NIMCP_INVALID_PARAM);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
