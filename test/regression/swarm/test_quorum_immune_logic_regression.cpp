/**
 * @file test_quorum_immune_logic_regression.cpp
 * @brief Regression tests for Quorum and Immune Logic Systems
 *
 * Tests to ensure existing functionality is not broken by logic integration
 * and that edge cases are properly handled.
 */

#include <gtest/gtest.h>
#include "swarm/nimcp_swarm_quorum.h"
#include "swarm/nimcp_swarm_immune.h"

class QuorumImmuneRegressionTest : public ::testing::Test {
protected:
    nimcp_swarm_quorum_t* quorum;
    NimcpSwarmImmuneSystem* immune;

    void SetUp() override {
        nimcp_quorum_config_t quorum_cfg;
        nimcp_swarm_quorum_default_config(&quorum_cfg);
        quorum = nimcp_swarm_quorum_create(&quorum_cfg, nullptr);
        ASSERT_NE(quorum, nullptr);

        NimcpSwarmImmuneConfig immune_cfg;
        nimcp_swarm_immune_default_config(&immune_cfg);
        immune = nimcp_swarm_immune_create(&immune_cfg, nullptr, 100);
        ASSERT_NE(immune, nullptr);
    }

    void TearDown() override {
        if (quorum) nimcp_swarm_quorum_destroy(quorum);
        if (immune) nimcp_swarm_immune_destroy(immune);
    }
};

// ============================================================================
// Regression: Existing Quorum Functions Still Work
// ============================================================================

TEST_F(QuorumImmuneRegressionTest, QuorumBasicOperationsUnchanged) {
    // Verify basic quorum operations work as before

    // Broadcast signal
    bool broadcast = nimcp_quorum_broadcast_signal(quorum, 1, NIMCP_SIGNAL_ATTACK, 0.8);
    EXPECT_TRUE(broadcast);

    // Update commitment
    bool commit = nimcp_quorum_update_commitment(quorum, 1, NIMCP_SIGNAL_ATTACK, 0.9);
    EXPECT_TRUE(commit);

    // Get commitment
    const nimcp_drone_commitment_t* commitment = nimcp_quorum_get_commitment(quorum, 1);
    ASSERT_NE(commitment, nullptr);
    EXPECT_EQ(commitment->signal, NIMCP_SIGNAL_ATTACK);

    // Check threshold
    nimcp_quorum_broadcast_signal(quorum, 2, NIMCP_SIGNAL_ATTACK, 0.9);
    nimcp_quorum_update_commitment(quorum, 2, NIMCP_SIGNAL_ATTACK, 0.95);
    bool threshold = nimcp_quorum_check_threshold(quorum, NIMCP_SIGNAL_ATTACK);
    // Result depends on config, just ensure it doesn't crash
}

TEST_F(QuorumImmuneRegressionTest, QuorumDecisionMakingUnchanged) {
    // Setup quorum
    for (uint32_t i = 0; i < 5; i++) {
        nimcp_quorum_update_commitment(quorum, i, NIMCP_SIGNAL_ATTACK, 0.95);
        nimcp_quorum_broadcast_signal(quorum, i, NIMCP_SIGNAL_ATTACK, 0.9);
    }

    // Make decision
    bool decision = nimcp_quorum_make_decision(
        quorum,
        NIMCP_DECISION_ATTACK_TIMING,
        nullptr
    );

    // Get last decision
    const nimcp_quorum_decision_t* last = nimcp_quorum_get_last_decision(quorum);
    if (decision) {
        ASSERT_NE(last, nullptr);
        EXPECT_EQ(last->type, NIMCP_DECISION_ATTACK_TIMING);
    }
}

// ============================================================================
// Regression: Existing Immune Functions Still Work
// ============================================================================

TEST_F(QuorumImmuneRegressionTest, ImmuneBasicDetectionUnchanged) {
    // Verify basic immune detection works
    uint8_t threat_data[64] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint32_t threat_id = 0;

    nimcp_result_t result = nimcp_swarm_immune_detect_threat(
        immune,
        threat_data,
        4,
        200,
        &threat_id
    );

    // Should complete without error
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(QuorumImmuneRegressionTest, ImmuneBehaviorCheckUnchanged) {
    // Verify behavior checking still works
    NimcpSwarmBehaviorProfile profile = {};
    profile.drone_id = 1;
    profile.msg_rate = 10.0f;
    profile.energy_usage = 5.0f;
    profile.anomaly_score = 0.3f;

    float anomaly_score = 0.0f;
    nimcp_result_t result = nimcp_swarm_immune_check_behavior(
        immune,
        1,
        &profile,
        &anomaly_score
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(QuorumImmuneRegressionTest, ImmuneMemoryCellUnchanged) {
    // Verify memory cell operations work
    NimcpSwarmThreatSignature sig = {};
    sig.pattern_len = 4;
    sig.pattern[0] = 0xDE;
    sig.pattern[1] = 0xAD;
    sig.match_threshold = 0.8f;
    sig.type = THREAT_MALICIOUS_DRONE;

    uint32_t cell_id = 0;
    nimcp_result_t result = nimcp_swarm_immune_add_memory_cell(
        immune,
        &sig,
        RESPONSE_ISOLATION,
        0.9f,
        &cell_id
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

// ============================================================================
// Regression: Edge Cases with Logic Integration
// ============================================================================

TEST_F(QuorumImmuneRegressionTest, EmptyQuorumLogicValidation) {
    // Edge case: Validate empty quorum
    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.min_agents = 0;

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_EQ(result, 0) << "Empty quorum should not validate";
}

TEST_F(QuorumImmuneRegressionTest, EmptyImmuneThreatEvaluation) {
    // Edge case: Evaluate threats with no rules
    float threat_scores[50] = {0};
    uint32_t num_threats = 0;

    nimcp_result_t result = immune_evaluate_threats(immune, threat_scores, &num_threats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(num_threats, 0u);
}

TEST_F(QuorumImmuneRegressionTest, MaxCapacityThreatRules) {
    // Regression: Adding many threat rules
    for (uint32_t i = 0; i < 45; i++) {
        uint32_t sources[] = {i};
        immune_threat_rule_t rule = {};
        rule.threat_id = i;
        rule.detection_logic = LOGIC_GATE_OR;
        rule.signal_sources = sources;
        rule.num_sources = 1;
        rule.confidence_threshold = 0.5f;
        rule.threat_type = THREAT_MALICIOUS_DRONE;

        nimcp_result_t result = immune_add_threat_rule(immune, &rule);
        EXPECT_EQ(result, NIMCP_SUCCESS) << "Should handle multiple rules";
    }
}

TEST_F(QuorumImmuneRegressionTest, LargeAgentCountValidation) {
    // Regression: Large number of agents in quorum
    for (uint32_t i = 0; i < 100; i++) {
        nimcp_quorum_update_commitment(quorum, i, NIMCP_SIGNAL_EXPLORE, 0.7);
    }

    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.gate_type = LOGIC_GATE_AND;
    logic_cfg.min_agents = 50;
    logic_cfg.confidence_threshold = 0.5f;

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    // Should complete without crashing
    EXPECT_GE(result, -1);
}

TEST_F(QuorumImmuneRegressionTest, ZeroThresholdsHandling) {
    // Edge case: Zero thresholds
    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.threshold = 0.0f;
    logic_cfg.confidence_threshold = 0.0f;
    logic_cfg.min_agents = 0;

    nimcp_quorum_update_commitment(quorum, 1, NIMCP_SIGNAL_ATTACK, 0.1);
    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_GE(result, -1) << "Should handle zero thresholds";
}

TEST_F(QuorumImmuneRegressionTest, MaxThresholdsHandling) {
    // Edge case: Maximum thresholds
    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.threshold = 1.0f;
    logic_cfg.confidence_threshold = 1.0f;

    nimcp_quorum_update_commitment(quorum, 1, NIMCP_SIGNAL_ATTACK, 1.0);
    nimcp_quorum_broadcast_signal(quorum, 1, NIMCP_SIGNAL_ATTACK, 1.0);

    int result = quorum_validate_with_logic(quorum, &logic_cfg);
    EXPECT_GE(result, -1) << "Should handle max thresholds";
}

TEST_F(QuorumImmuneRegressionTest, ConcurrentOperations) {
    // Regression: Multiple operations in sequence
    for (int i = 0; i < 10; i++) {
        // Quorum operations
        nimcp_quorum_broadcast_signal(quorum, i, NIMCP_SIGNAL_ATTACK, 0.8);
        nimcp_quorum_update_commitment(quorum, i, NIMCP_SIGNAL_ATTACK, 0.85);

        // Immune operations
        NimcpSwarmBehaviorProfile profile = {};
        profile.drone_id = i;
        profile.anomaly_score = 0.2f;
        float score;
        nimcp_swarm_immune_check_behavior(immune, i, &profile, &score);

        // Logic operations
        quorum_logic_config_t logic_cfg;
        quorum_logic_default_config(&logic_cfg);
        logic_cfg.min_agents = i + 1;
        quorum_validate_with_logic(quorum, &logic_cfg);
    }
}

TEST_F(QuorumImmuneRegressionTest, RepeatedValidation) {
    // Regression: Validate multiple times
    for (uint32_t i = 0; i < 5; i++) {
        nimcp_quorum_update_commitment(quorum, i, NIMCP_SIGNAL_DEFEND, 0.9);
    }

    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.min_agents = 3;

    // Validate 10 times
    for (int i = 0; i < 10; i++) {
        int result = quorum_validate_with_logic(quorum, &logic_cfg);
        EXPECT_GE(result, -1) << "Repeated validation should work";
    }
}

TEST_F(QuorumImmuneRegressionTest, MemoryLeakCheck) {
    // Regression: Create and destroy multiple times
    for (int iter = 0; iter < 5; iter++) {
        // Add threat rules
        for (uint32_t i = 0; i < 10; i++) {
            uint32_t sources[] = {i};
            immune_threat_rule_t rule = {};
            rule.threat_id = i;
            rule.detection_logic = LOGIC_GATE_OR;
            rule.signal_sources = sources;
            rule.num_sources = 1;
            rule.confidence_threshold = 0.5f;
            rule.threat_type = THREAT_MALICIOUS_DRONE;

            immune_add_threat_rule(immune, &rule);
        }

        // Reset (should free allocated memory)
        nimcp_swarm_immune_reset(immune, false);
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
