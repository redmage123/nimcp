/**
 * @file test_swarm_quorum_logic.cpp
 * @brief Unit tests for swarm quorum logic validation
 *
 * Tests logic gate integration with quorum decision-making including:
 * - AND gate (unanimous consensus)
 * - OR gate (permissive consensus)
 * - XOR gate (contradiction detection)
 * - IMPLIES gate (conditional consensus)
 */

#include <gtest/gtest.h>
#include "swarm/nimcp_swarm_quorum.h"
#include "core/neuron_types/nimcp_neural_logic.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

class SwarmQuorumLogicTest : public ::testing::Test {
protected:
    nimcp_swarm_quorum_t* quorum;
    nimcp_quorum_config_t config;

    void SetUp() override {
        nimcp_swarm_quorum_default_config(&config);
        config.min_quorum_size = 3;
        config.base_threshold = 0.5;
        quorum = nimcp_swarm_quorum_create(&config, nullptr);
        ASSERT_NE(quorum, nullptr);
    }

    void TearDown() override {
        if (quorum) {
            nimcp_swarm_quorum_destroy(quorum);
        }
    }

    // Helper: Add multiple drone commitments
    void add_commitments(nimcp_signal_type_t signal, uint32_t count, double strength) {
        for (uint32_t i = 0; i < count; i++) {
            ASSERT_TRUE(nimcp_quorum_update_commitment(quorum, 100 + i, signal, strength));
        }
    }

    // Helper: Add contradicting commitments
    void add_contradicting_commitments() {
        nimcp_quorum_update_commitment(quorum, 100, NIMCP_SIGNAL_ATTACK, 0.9);
        nimcp_quorum_update_commitment(quorum, 101, NIMCP_SIGNAL_RETREAT, 0.9);
        nimcp_quorum_update_commitment(quorum, 102, NIMCP_SIGNAL_ATTACK, 0.8);
    }
};

/* ============================================================================
 * Logic Rule Setting Tests
 * ============================================================================ */

TEST_F(SwarmQuorumLogicTest, DefaultLogicConfig) {
    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);

    EXPECT_EQ(logic_cfg.gate_type, LOGIC_GATE_AND);
    EXPECT_FLOAT_EQ(logic_cfg.threshold, 0.5f);
    EXPECT_TRUE(logic_cfg.require_consistency);
    EXPECT_EQ(logic_cfg.min_agents, 3u);
    EXPECT_FLOAT_EQ(logic_cfg.confidence_threshold, 0.7f);
}

TEST_F(SwarmQuorumLogicTest, SetLogicRuleAND) {
    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.gate_type = LOGIC_GATE_AND;
    logic_cfg.threshold = 0.99f;  // Require near-unanimity

    // Add unanimous votes
    add_commitments(NIMCP_SIGNAL_ATTACK, 5, 0.95);

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 1) << "AND gate should pass with unanimous agreement";
}

TEST_F(SwarmQuorumLogicTest, SetLogicRuleOR) {
    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.gate_type = LOGIC_GATE_OR;

    // Add just one strong vote
    nimcp_quorum_update_commitment(quorum, 100, NIMCP_SIGNAL_ATTACK, 0.9);

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 1) << "OR gate should pass with any support";
}

TEST_F(SwarmQuorumLogicTest, SetLogicRuleThreshold) {
    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.threshold = 0.75f;  // Require 75% consensus

    // Add 4 votes for ATTACK, 1 for RETREAT (80% consensus)
    add_commitments(NIMCP_SIGNAL_ATTACK, 4, 0.9);
    nimcp_quorum_update_commitment(quorum, 200, NIMCP_SIGNAL_RETREAT, 0.9);

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 1) << "Should pass with 80% consensus (above 75% threshold)";
}

/* ============================================================================
 * Decision Validation with IMPLIES Gate Tests
 * ============================================================================ */

TEST_F(SwarmQuorumLogicTest, ValidateDecisionWithIMPLIES_ThresholdMet) {
    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.gate_type = LOGIC_GATE_IMPLIES;
    logic_cfg.threshold = 0.6f;
    logic_cfg.confidence_threshold = 0.7f;

    // Add strong consensus above threshold
    add_commitments(NIMCP_SIGNAL_ATTACK, 5, 0.95);

    // Broadcast signal to reach concentration threshold
    for (int i = 0; i < 5; i++) {
        nimcp_quorum_broadcast_signal(quorum, 100 + i, NIMCP_SIGNAL_ATTACK, 0.9);
    }

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 1) << "IMPLIES: If threshold met, consensus required -> should pass";
}

TEST_F(SwarmQuorumLogicTest, ValidateDecisionWithIMPLIES_ThresholdNotMet) {
    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.gate_type = LOGIC_GATE_IMPLIES;
    logic_cfg.threshold = 0.9f;  // Very high threshold
    logic_cfg.confidence_threshold = 0.5f;

    // Add weak consensus below threshold
    add_commitments(NIMCP_SIGNAL_ATTACK, 3, 0.4);

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 1) << "IMPLIES: Threshold not met -> vacuously true";
}

TEST_F(SwarmQuorumLogicTest, ValidateDecisionWithIMPLIES_HighConfidence) {
    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.gate_type = LOGIC_GATE_IMPLIES;
    logic_cfg.threshold = 0.5f;
    logic_cfg.confidence_threshold = 0.95f;  // Very high confidence required

    // Add consensus but with low signal concentration
    add_commitments(NIMCP_SIGNAL_ATTACK, 5, 0.9);

    // Low signal broadcast - won't meet confidence
    nimcp_quorum_broadcast_signal(quorum, 100, NIMCP_SIGNAL_ATTACK, 0.3);

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 0) << "Should fail due to low confidence";
}

/* ============================================================================
 * Threshold Logic Tests
 * ============================================================================ */

TEST_F(SwarmQuorumLogicTest, ThresholdLogic_ExactThreshold) {
    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.threshold = 0.6f;

    // Add exactly 60% consensus (3 out of 5)
    add_commitments(NIMCP_SIGNAL_ATTACK, 3, 0.9);
    add_commitments(NIMCP_SIGNAL_RETREAT, 2, 0.9);

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 1) << "Should pass with exact threshold match";
}

TEST_F(SwarmQuorumLogicTest, ThresholdLogic_BelowThreshold) {
    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.threshold = 0.7f;

    // Add 60% consensus (below threshold)
    add_commitments(NIMCP_SIGNAL_ATTACK, 3, 0.9);
    add_commitments(NIMCP_SIGNAL_RETREAT, 2, 0.9);

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 0) << "Should fail below threshold";
}

TEST_F(SwarmQuorumLogicTest, ThresholdLogic_MinimumAgents) {
    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.min_agents = 5;

    // Add only 3 agents (below minimum)
    add_commitments(NIMCP_SIGNAL_ATTACK, 3, 0.9);

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 0) << "Should fail with insufficient agents";
}

/* ============================================================================
 * XOR Contradiction Detection Tests
 * ============================================================================ */

TEST_F(SwarmQuorumLogicTest, CheckVoteConsistency_NoContradictions) {
    // Add consistent votes
    add_commitments(NIMCP_SIGNAL_ATTACK, 5, 0.9);

    uint32_t contradicting_agents[256];
    uint32_t count = 0;

    int result = quorum_check_vote_consistency(quorum, contradicting_agents, &count);
    EXPECT_EQ(result, 0) << "Should find no contradictions";
    EXPECT_EQ(count, 0u);
}

TEST_F(SwarmQuorumLogicTest, CheckVoteConsistency_AttackVsRetreat) {
    // Add contradicting votes: ATTACK and RETREAT are mutually exclusive
    add_contradicting_commitments();

    uint32_t contradicting_agents[256];
    uint32_t count = 0;

    int result = quorum_check_vote_consistency(quorum, contradicting_agents, &count);
    EXPECT_EQ(result, 1) << "Should detect contradictions";
    EXPECT_GT(count, 0u) << "Should identify contradicting agents";
}

TEST_F(SwarmQuorumLogicTest, CheckVoteConsistency_MultipleExclusivePairs) {
    // ATTACK vs DEFEND should also be exclusive
    nimcp_quorum_update_commitment(quorum, 100, NIMCP_SIGNAL_ATTACK, 0.9);
    nimcp_quorum_update_commitment(quorum, 101, NIMCP_SIGNAL_DEFEND, 0.9);
    nimcp_quorum_update_commitment(quorum, 102, NIMCP_SIGNAL_EXPLORE, 0.8);
    nimcp_quorum_update_commitment(quorum, 103, NIMCP_SIGNAL_DEFEND, 0.9);

    uint32_t contradicting_agents[256];
    uint32_t count = 0;

    int result = quorum_check_vote_consistency(quorum, contradicting_agents, &count);
    EXPECT_EQ(result, 1) << "Should detect EXPLORE vs DEFEND contradiction";
}

TEST_F(SwarmQuorumLogicTest, ValidationWithConsistencyCheck_Passes) {
    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.require_consistency = true;

    // Add consistent votes
    add_commitments(NIMCP_SIGNAL_ATTACK, 5, 0.9);

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 1) << "Should pass with consistent votes";
}

TEST_F(SwarmQuorumLogicTest, ValidationWithConsistencyCheck_Fails) {
    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.require_consistency = true;
    logic_cfg.gate_type = LOGIC_GATE_OR;  // Would pass without consistency check

    // Add contradicting votes
    add_contradicting_commitments();

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 0) << "Should fail due to contradictions";
}

/* ============================================================================
 * IMPLIES Logic Evaluation Tests
 * ============================================================================ */

TEST_F(SwarmQuorumLogicTest, EvaluateImplication_AntecedentTrue_ConsequentTrue) {
    // IF ALERT THEN DEFEND
    nimcp_quorum_broadcast_signal(quorum, 100, NIMCP_SIGNAL_ALERT, 0.8);
    nimcp_quorum_broadcast_signal(quorum, 101, NIMCP_SIGNAL_DEFEND, 0.7);

    bool implication_holds = false;
    int result = quorum_evaluate_implication(
        quorum,
        NIMCP_SIGNAL_ALERT,
        NIMCP_SIGNAL_DEFEND,
        &implication_holds
    );

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(implication_holds) << "A→B should be true when both A and B are true";
}

TEST_F(SwarmQuorumLogicTest, EvaluateImplication_AntecedentTrue_ConsequentFalse) {
    // IF ALERT THEN DEFEND, but no DEFEND signal
    nimcp_quorum_broadcast_signal(quorum, 100, NIMCP_SIGNAL_ALERT, 0.8);

    bool implication_holds = true;  // Set to true initially
    int result = quorum_evaluate_implication(
        quorum,
        NIMCP_SIGNAL_ALERT,
        NIMCP_SIGNAL_DEFEND,
        &implication_holds
    );

    EXPECT_EQ(result, 0);
    EXPECT_FALSE(implication_holds) << "A→B should be false when A is true but B is false";
}

TEST_F(SwarmQuorumLogicTest, EvaluateImplication_AntecedentFalse) {
    // No ALERT signal - implication vacuously true
    bool implication_holds = false;
    int result = quorum_evaluate_implication(
        quorum,
        NIMCP_SIGNAL_ALERT,
        NIMCP_SIGNAL_DEFEND,
        &implication_holds
    );

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(implication_holds) << "A→B should be vacuously true when A is false";
}

/* ============================================================================
 * XOR Gate Tests
 * ============================================================================ */

TEST_F(SwarmQuorumLogicTest, XORValidation_ClearWinner) {
    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.gate_type = LOGIC_GATE_XOR;

    // Clear winner: 5 vs 1
    add_commitments(NIMCP_SIGNAL_ATTACK, 5, 0.9);
    nimcp_quorum_update_commitment(quorum, 200, NIMCP_SIGNAL_RETREAT, 0.9);

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 1) << "XOR: Should pass with clear winner (no tie)";
}

TEST_F(SwarmQuorumLogicTest, XORValidation_TiedVotes) {
    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.gate_type = LOGIC_GATE_XOR;

    // Tied votes: 3 vs 3
    add_commitments(NIMCP_SIGNAL_ATTACK, 3, 0.9);
    for (uint32_t i = 0; i < 3; i++) {
        nimcp_quorum_update_commitment(quorum, 200 + i, NIMCP_SIGNAL_RETREAT, 0.9);
    }

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 0) << "XOR: Should fail with tied votes";
}

/* ============================================================================
 * Edge Case and Error Handling Tests
 * ============================================================================ */

TEST_F(SwarmQuorumLogicTest, ValidateWithNullPointers) {
    int result = quorum_validate_with_logic(nullptr, nullptr);
    EXPECT_EQ(result, -1) << "Should return error with null pointers";
}

TEST_F(SwarmQuorumLogicTest, ConsistencyCheckWithNullPointers) {
    int result = quorum_check_vote_consistency(nullptr, nullptr, nullptr);
    EXPECT_EQ(result, -1) << "Should return error with null pointers";
}

TEST_F(SwarmQuorumLogicTest, ImplicationWithNullPointer) {
    bool holds = false;
    int result = quorum_evaluate_implication(nullptr, NIMCP_SIGNAL_ATTACK, NIMCP_SIGNAL_DEFEND, &holds);
    EXPECT_EQ(result, -1) << "Should return error with null quorum";
}

TEST_F(SwarmQuorumLogicTest, ValidateWithZeroCommitments) {
    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.min_agents = 0;  // Allow zero agents

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 0) << "Should return 0 with no commitments";
}

TEST_F(SwarmQuorumLogicTest, HighStressMultipleValidations) {
    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);

    // Add many agents
    for (uint32_t i = 0; i < 100; i++) {
        nimcp_quorum_update_commitment(quorum, i, NIMCP_SIGNAL_ATTACK, 0.85);
    }

    // Run multiple validations
    for (int i = 0; i < 10; i++) {
        int result = quorum_validate_with_logic(quorum, &logic_cfg);
        EXPECT_GE(result, 0) << "Validation " << i << " should not error";
    }
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
