/**
 * @file test_quorum_logic.cpp
 * @brief Unit tests for Quorum Logic Validation
 *
 * Tests the integration of neural logic gates with quorum sensing system
 * for distributed decision validation.
 */

#include <gtest/gtest.h>
#include "swarm/nimcp_swarm_quorum.h"
#include "core/neuron_types/nimcp_neural_logic.h"
#include "utils/logging/nimcp_logging.h"

class QuorumLogicTest : public ::testing::Test {
protected:
    nimcp_swarm_quorum_t* quorum;
    nimcp_quorum_config_t config;

    void SetUp() override {
        nimcp_swarm_quorum_default_config(&config);
        quorum = nimcp_swarm_quorum_create(&config, nullptr);
        ASSERT_NE(quorum, nullptr);
    }

    void TearDown() override {
        if (quorum) {
            nimcp_swarm_quorum_destroy(quorum);
            quorum = nullptr;
        }
    }

    // Helper: Add commitments for testing
    void add_test_commitments(nimcp_signal_type_t signal, uint32_t count, double strength) {
        for (uint32_t i = 0; i < count; i++) {
            nimcp_quorum_update_commitment(quorum, i, signal, strength);
        }
    }
};

// ============================================================================
// Basic Configuration Tests
// ============================================================================

TEST_F(QuorumLogicTest, DefaultLogicConfig) {
    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);

    EXPECT_EQ(logic_cfg.gate_type, LOGIC_GATE_AND);
    EXPECT_FLOAT_EQ(logic_cfg.threshold, 0.5f);
    EXPECT_TRUE(logic_cfg.require_consistency);
    EXPECT_EQ(logic_cfg.min_agents, 3u);
    EXPECT_FLOAT_EQ(logic_cfg.confidence_threshold, 0.7f);
}

TEST_F(QuorumLogicTest, NullConfigHandling) {
    quorum_logic_config_t* null_cfg = nullptr;
    quorum_logic_default_config(null_cfg); // Should not crash
}

// ============================================================================
// AND Gate Validation Tests
// ============================================================================

TEST_F(QuorumLogicTest, AndGateUnanimous) {
    // Setup: All 5 agents vote for ATTACK
    add_test_commitments(NIMCP_SIGNAL_ATTACK, 5, 0.9);
    nimcp_quorum_broadcast_signal(quorum, 0, NIMCP_SIGNAL_ATTACK, 0.9);

    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.gate_type = LOGIC_GATE_AND;
    logic_cfg.min_agents = 3;
    logic_cfg.confidence_threshold = 0.5f;

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 1) << "Unanimous vote should pass AND validation";
}

TEST_F(QuorumLogicTest, AndGateNotUnanimous) {
    // Setup: 4 agents vote ATTACK, 1 votes RETREAT
    add_test_commitments(NIMCP_SIGNAL_ATTACK, 4, 0.9);
    nimcp_quorum_update_commitment(quorum, 5, NIMCP_SIGNAL_RETREAT, 0.9);
    nimcp_quorum_broadcast_signal(quorum, 0, NIMCP_SIGNAL_ATTACK, 0.9);

    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.gate_type = LOGIC_GATE_AND;

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 0) << "Non-unanimous vote should fail AND validation";
}

TEST_F(QuorumLogicTest, AndGateInsufficientAgents) {
    // Setup: Only 2 agents (less than minimum 3)
    add_test_commitments(NIMCP_SIGNAL_ATTACK, 2, 0.9);

    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.gate_type = LOGIC_GATE_AND;
    logic_cfg.min_agents = 3;

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 0) << "Insufficient agents should fail validation";
}

// ============================================================================
// OR Gate Validation Tests
// ============================================================================

TEST_F(QuorumLogicTest, OrGateAnySupportPasses) {
    // Setup: Only 1 agent votes for ATTACK
    add_test_commitments(NIMCP_SIGNAL_ATTACK, 1, 0.9);
    nimcp_quorum_broadcast_signal(quorum, 0, NIMCP_SIGNAL_ATTACK, 0.9);

    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.gate_type = LOGIC_GATE_OR;
    logic_cfg.min_agents = 1;
    logic_cfg.confidence_threshold = 0.5f;

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 1) << "Any support should pass OR validation";
}

TEST_F(QuorumLogicTest, OrGateNoSupport) {
    // Setup: Agents exist but no commitments
    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.gate_type = LOGIC_GATE_OR;
    logic_cfg.min_agents = 0;

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 0) << "No support should fail OR validation";
}

// ============================================================================
// XOR Gate Validation Tests
// ============================================================================

TEST_F(QuorumLogicTest, XorGateClearWinner) {
    // Setup: 8 agents vote ATTACK, 2 vote RETREAT (clear winner)
    add_test_commitments(NIMCP_SIGNAL_ATTACK, 8, 0.9);
    nimcp_quorum_update_commitment(quorum, 10, NIMCP_SIGNAL_RETREAT, 0.9);
    nimcp_quorum_update_commitment(quorum, 11, NIMCP_SIGNAL_RETREAT, 0.9);
    nimcp_quorum_broadcast_signal(quorum, 0, NIMCP_SIGNAL_ATTACK, 0.9);

    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.gate_type = LOGIC_GATE_XOR;
    logic_cfg.min_agents = 5;
    logic_cfg.confidence_threshold = 0.5f;
    /* XOR tests for exclusive winner, not vote consistency */
    logic_cfg.require_consistency = false;

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 1) << "Clear winner should pass XOR validation";
}

TEST_F(QuorumLogicTest, XorGateTied) {
    // Setup: 5 agents vote ATTACK, 5 vote RETREAT (tie)
    add_test_commitments(NIMCP_SIGNAL_ATTACK, 5, 0.9);
    for (uint32_t i = 5; i < 10; i++) {
        nimcp_quorum_update_commitment(quorum, i, NIMCP_SIGNAL_RETREAT, 0.9);
    }
    nimcp_quorum_broadcast_signal(quorum, 0, NIMCP_SIGNAL_ATTACK, 0.5);

    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.gate_type = LOGIC_GATE_XOR;
    logic_cfg.confidence_threshold = 0.3f;
    /* XOR tests for exclusive winner, not vote consistency */
    logic_cfg.require_consistency = false;

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 0) << "Tied vote should fail XOR validation";
}

// ============================================================================
// IMPLIES Gate Validation Tests
// ============================================================================

TEST_F(QuorumLogicTest, ImpliesGateAntecedentTrue) {
    // Setup: High confidence signal with strong consensus
    add_test_commitments(NIMCP_SIGNAL_ATTACK, 8, 0.9);
    nimcp_quorum_broadcast_signal(quorum, 0, NIMCP_SIGNAL_ATTACK, 0.9);

    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.gate_type = LOGIC_GATE_IMPLIES;
    logic_cfg.threshold = 0.6f;
    logic_cfg.min_agents = 5;
    logic_cfg.confidence_threshold = 0.6f;

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 1) << "If confidence high, consensus high: should pass";
}

TEST_F(QuorumLogicTest, ImpliesGateAntecedentFalse) {
    // Setup: Low confidence signal (antecedent false)
    add_test_commitments(NIMCP_SIGNAL_ATTACK, 3, 0.9);
    nimcp_quorum_broadcast_signal(quorum, 0, NIMCP_SIGNAL_ATTACK, 0.3);

    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.gate_type = LOGIC_GATE_IMPLIES;
    logic_cfg.threshold = 0.6f;
    logic_cfg.confidence_threshold = 0.1f; // Allow low confidence

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 1) << "Antecedent false: implication vacuously true";
}

// ============================================================================
// Consistency Check Tests
// ============================================================================

TEST_F(QuorumLogicTest, ConsistencyCheckPass) {
    // Setup: All agents agree on ATTACK
    add_test_commitments(NIMCP_SIGNAL_ATTACK, 5, 0.9);

    uint32_t contradicting[256];
    uint32_t count = 0;

    int result = quorum_check_vote_consistency(quorum, contradicting, &count);
    EXPECT_EQ(result, 0) << "Consistent votes should return 0";
    EXPECT_EQ(count, 0u) << "No contradicting agents";
}

TEST_F(QuorumLogicTest, ConsistencyCheckContradiction) {
    // Setup: Agents vote for mutually exclusive signals
    add_test_commitments(NIMCP_SIGNAL_ATTACK, 3, 0.9);
    nimcp_quorum_update_commitment(quorum, 5, NIMCP_SIGNAL_RETREAT, 0.9);
    nimcp_quorum_update_commitment(quorum, 6, NIMCP_SIGNAL_RETREAT, 0.9);

    uint32_t contradicting[256];
    uint32_t count = 0;

    int result = quorum_check_vote_consistency(quorum, contradicting, &count);
    EXPECT_EQ(result, 1) << "Contradicting votes should return 1";
    EXPECT_GT(count, 0u) << "Should detect contradicting agents";
}

TEST_F(QuorumLogicTest, ConsistencyCheckAttackDefend) {
    // Setup: ATTACK and DEFEND are mutually exclusive
    add_test_commitments(NIMCP_SIGNAL_ATTACK, 3, 0.9);
    nimcp_quorum_update_commitment(quorum, 5, NIMCP_SIGNAL_DEFEND, 0.9);

    uint32_t contradicting[256];
    uint32_t count = 0;

    int result = quorum_check_vote_consistency(quorum, contradicting, &count);
    EXPECT_EQ(result, 1) << "ATTACK/DEFEND contradiction";
    EXPECT_GT(count, 0u);
}

// ============================================================================
// Implication Evaluation Tests
// ============================================================================

TEST_F(QuorumLogicTest, ImplicationHolds) {
    // Setup: ALERT signal present, DEFEND signal also present
    nimcp_quorum_broadcast_signal(quorum, 0, NIMCP_SIGNAL_ALERT, 0.8);
    nimcp_quorum_broadcast_signal(quorum, 1, NIMCP_SIGNAL_DEFEND, 0.7);

    bool holds = false;
    int result = quorum_evaluate_implication(
        quorum,
        NIMCP_SIGNAL_ALERT,
        NIMCP_SIGNAL_DEFEND,
        &holds
    );

    EXPECT_EQ(result, 0) << "Function should succeed";
    EXPECT_TRUE(holds) << "ALERT → DEFEND should hold";
}

TEST_F(QuorumLogicTest, ImplicationFails) {
    // Setup: ALERT signal present, DEFEND signal absent
    nimcp_quorum_broadcast_signal(quorum, 0, NIMCP_SIGNAL_ALERT, 0.8);
    nimcp_quorum_broadcast_signal(quorum, 1, NIMCP_SIGNAL_DEFEND, 0.2);

    bool holds = true; // Initialize to opposite of expected
    int result = quorum_evaluate_implication(
        quorum,
        NIMCP_SIGNAL_ALERT,
        NIMCP_SIGNAL_DEFEND,
        &holds
    );

    EXPECT_EQ(result, 0) << "Function should succeed";
    EXPECT_FALSE(holds) << "ALERT → DEFEND should fail";
}

TEST_F(QuorumLogicTest, ImplicationVacuous) {
    // Setup: Antecedent false (ALERT absent)
    nimcp_quorum_broadcast_signal(quorum, 0, NIMCP_SIGNAL_ALERT, 0.2);
    nimcp_quorum_broadcast_signal(quorum, 1, NIMCP_SIGNAL_DEFEND, 0.1);

    bool holds = false;
    int result = quorum_evaluate_implication(
        quorum,
        NIMCP_SIGNAL_ALERT,
        NIMCP_SIGNAL_DEFEND,
        &holds
    );

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(holds) << "False antecedent: implication vacuously true";
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(QuorumLogicTest, ValidateNullQuorum) {
    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);

    int result = quorum_validate_with_logic(nullptr, &logic_cfg);
    EXPECT_EQ(result, -1) << "Null quorum should return error";
}

TEST_F(QuorumLogicTest, ValidateNullConfig) {
    int result = quorum_validate_with_logic(quorum, nullptr);
    EXPECT_EQ(result, -1) << "Null config should return error";
}

TEST_F(QuorumLogicTest, ConsistencyCheckNullParams) {
    uint32_t contradicting[256];
    uint32_t count = 0;

    // Test null quorum
    int result1 = quorum_check_vote_consistency(nullptr, contradicting, &count);
    EXPECT_EQ(result1, -1);

    // Test null contradicting array
    int result2 = quorum_check_vote_consistency(quorum, nullptr, &count);
    EXPECT_EQ(result2, -1);

    // Test null count
    int result3 = quorum_check_vote_consistency(quorum, contradicting, nullptr);
    EXPECT_EQ(result3, -1);
}

TEST_F(QuorumLogicTest, ImplicationInvalidSignals) {
    bool holds = false;

    // Test invalid antecedent
    int result1 = quorum_evaluate_implication(
        quorum,
        NIMCP_SIGNAL_COUNT,
        NIMCP_SIGNAL_DEFEND,
        &holds
    );
    EXPECT_EQ(result1, -1);

    // Test invalid consequent
    int result2 = quorum_evaluate_implication(
        quorum,
        NIMCP_SIGNAL_ALERT,
        NIMCP_SIGNAL_COUNT,
        &holds
    );
    EXPECT_EQ(result2, -1);
}

// ============================================================================
// Integration with Consistency Check
// ============================================================================

TEST_F(QuorumLogicTest, ValidationWithConsistencyEnabled) {
    // Setup: Contradictory votes
    add_test_commitments(NIMCP_SIGNAL_ATTACK, 3, 0.9);
    nimcp_quorum_update_commitment(quorum, 5, NIMCP_SIGNAL_RETREAT, 0.9);
    nimcp_quorum_broadcast_signal(quorum, 0, NIMCP_SIGNAL_ATTACK, 0.8);

    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.gate_type = LOGIC_GATE_OR;
    logic_cfg.require_consistency = true; // Enable consistency check
    logic_cfg.confidence_threshold = 0.5f;

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 0) << "Should fail due to consistency check";
}

TEST_F(QuorumLogicTest, ValidationWithConsistencyDisabled) {
    // Setup: Contradictory votes
    add_test_commitments(NIMCP_SIGNAL_ATTACK, 3, 0.9);
    nimcp_quorum_update_commitment(quorum, 5, NIMCP_SIGNAL_RETREAT, 0.9);
    nimcp_quorum_broadcast_signal(quorum, 0, NIMCP_SIGNAL_ATTACK, 0.8);

    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.gate_type = LOGIC_GATE_OR;
    logic_cfg.require_consistency = false; // Disable consistency check
    logic_cfg.confidence_threshold = 0.5f;
    logic_cfg.min_agents = 3;

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 1) << "Should pass (consistency check disabled)";
}

// ============================================================================
// Confidence Threshold Tests
// ============================================================================

TEST_F(QuorumLogicTest, ConfidenceBelowThreshold) {
    add_test_commitments(NIMCP_SIGNAL_ATTACK, 5, 0.9);
    nimcp_quorum_broadcast_signal(quorum, 0, NIMCP_SIGNAL_ATTACK, 0.3);

    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.gate_type = LOGIC_GATE_OR;
    logic_cfg.confidence_threshold = 0.8f; // High threshold

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 0) << "Low confidence should fail validation";
}

TEST_F(QuorumLogicTest, ConfidenceAboveThreshold) {
    add_test_commitments(NIMCP_SIGNAL_ATTACK, 5, 0.9);
    nimcp_quorum_broadcast_signal(quorum, 0, NIMCP_SIGNAL_ATTACK, 0.9);

    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.gate_type = LOGIC_GATE_OR;
    logic_cfg.confidence_threshold = 0.7f;
    logic_cfg.min_agents = 3;

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 1) << "High confidence should pass validation";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
