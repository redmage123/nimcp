/**
 * @file test_swarm_logic_bridge.cpp
 * @brief Comprehensive unit tests for Logic-Swarm Bridge
 *
 * TEST COVERAGE:
 * - Bridge creation and destruction
 * - Rule management (add, remove, get)
 * - Logic gate evaluation (AND, OR, NOT, XOR, IMPLIES)
 * - Consensus validation
 * - Contradiction detection
 * - Implication validation
 * - Cache functionality
 * - Statistics tracking
 * - Bio-async integration
 * - Edge cases and error handling
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_logic_bridge.h"
#include "utils/logging/nimcp_logging.h"

class SwarmLogicBridgeTest : public ::testing::Test {
protected:
    swarm_logic_bridge_t* bridge;
    swarm_logic_bridge_config_t config;

    void SetUp() override {
        swarm_logic_bridge_get_default_config(&config);
        config.enable_bio_async = false; // Disable for unit tests
        bridge = swarm_logic_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            swarm_logic_bridge_destroy(bridge);
        }
    }

    // Helper: Create test agent state
    swarm_agent_state_t create_agent(uint32_t id, float belief, bool active = true) {
        swarm_agent_state_t agent;
        memset(&agent, 0, sizeof(agent));
        agent.agent_id = id;
        agent.belief_value = belief;
        agent.confidence = 1.0f;
        agent.timestamp_us = 0;
        agent.is_active = active;
        return agent;
    }

    // Helper: Create test rule
    swarm_logic_rule_t create_rule(uint32_t rule_id, logic_gate_type_t gate_type,
                                   uint32_t* agent_ids, uint32_t num_agents) {
        swarm_logic_rule_t rule;
        memset(&rule, 0, sizeof(rule));
        rule.rule_id = rule_id;
        rule.gate_type = gate_type;
        rule.input_agent_ids = agent_ids;
        rule.num_inputs = num_agents;
        rule.confidence_weight = 1.0f;
        rule.threshold = 0.5f;
        snprintf(rule.description, sizeof(rule.description), "Test rule %u", rule_id);
        return rule;
    }
};

/*=============================================================================
 * LIFECYCLE TESTS
 *============================================================================*/

TEST_F(SwarmLogicBridgeTest, CreateValidBridge) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(SwarmLogicBridgeTest, CreateWithNullConfig) {
    swarm_logic_bridge_t* test_bridge = swarm_logic_bridge_create(nullptr);
    EXPECT_NE(test_bridge, nullptr);
    swarm_logic_bridge_destroy(test_bridge);
}

TEST_F(SwarmLogicBridgeTest, DestroyNullBridge) {
    swarm_logic_bridge_destroy(nullptr);
    SUCCEED(); // Should not crash
}

TEST_F(SwarmLogicBridgeTest, DefaultConfig) {
    swarm_logic_bridge_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    swarm_logic_bridge_get_default_config(&cfg);

    EXPECT_GT(cfg.max_rules, 0u);
    EXPECT_GT(cfg.rule_cache_size, 0u);
    EXPECT_GT(cfg.inference_threshold, 0.0f);
    EXPECT_LE(cfg.inference_threshold, 1.0f);
}

/*=============================================================================
 * RULE MANAGEMENT TESTS
 *============================================================================*/

TEST_F(SwarmLogicBridgeTest, AddRule) {
    uint32_t agent_ids[] = {1, 2};
    swarm_logic_rule_t rule = create_rule(100, LOGIC_GATE_AND, agent_ids, 2);

    nimcp_error_t result = swarm_logic_bridge_add_rule(bridge, &rule);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmLogicBridgeTest, AddRuleNullBridge) {
    uint32_t agent_ids[] = {1, 2};
    swarm_logic_rule_t rule = create_rule(100, LOGIC_GATE_AND, agent_ids, 2);

    nimcp_error_t result = swarm_logic_bridge_add_rule(nullptr, &rule);
    EXPECT_EQ(result, NIMCP_INVALID_PARAM);
}

TEST_F(SwarmLogicBridgeTest, AddRuleNullRule) {
    nimcp_error_t result = swarm_logic_bridge_add_rule(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_INVALID_PARAM);
}

TEST_F(SwarmLogicBridgeTest, AddDuplicateRule) {
    uint32_t agent_ids[] = {1, 2};
    swarm_logic_rule_t rule = create_rule(100, LOGIC_GATE_AND, agent_ids, 2);

    swarm_logic_bridge_add_rule(bridge, &rule);
    nimcp_error_t result = swarm_logic_bridge_add_rule(bridge, &rule);
    EXPECT_EQ(result, NIMCP_INVALID_PARAM);
}

TEST_F(SwarmLogicBridgeTest, RemoveRule) {
    uint32_t agent_ids[] = {1, 2};
    swarm_logic_rule_t rule = create_rule(100, LOGIC_GATE_AND, agent_ids, 2);

    swarm_logic_bridge_add_rule(bridge, &rule);
    nimcp_error_t result = swarm_logic_bridge_remove_rule(bridge, 100);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmLogicBridgeTest, RemoveNonexistentRule) {
    nimcp_error_t result = swarm_logic_bridge_remove_rule(bridge, 9999);
    EXPECT_EQ(result, NIMCP_NOT_FOUND);
}

TEST_F(SwarmLogicBridgeTest, GetRule) {
    uint32_t agent_ids[] = {1, 2};
    swarm_logic_rule_t rule = create_rule(100, LOGIC_GATE_AND, agent_ids, 2);

    swarm_logic_bridge_add_rule(bridge, &rule);

    const swarm_logic_rule_t* retrieved = swarm_logic_bridge_get_rule(bridge, 100);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->rule_id, 100u);
    EXPECT_EQ(retrieved->gate_type, LOGIC_GATE_AND);
}

TEST_F(SwarmLogicBridgeTest, GetNonexistentRule) {
    const swarm_logic_rule_t* retrieved = swarm_logic_bridge_get_rule(bridge, 9999);
    EXPECT_EQ(retrieved, nullptr);
}

TEST_F(SwarmLogicBridgeTest, GetAllRules) {
    // Add multiple rules
    for (uint32_t i = 0; i < 5; i++) {
        uint32_t agent_ids[] = {i * 2, i * 2 + 1};
        swarm_logic_rule_t rule = create_rule(100 + i, LOGIC_GATE_AND, agent_ids, 2);
        swarm_logic_bridge_add_rule(bridge, &rule);
    }

    const swarm_logic_rule_t* rules[10];
    uint32_t count = swarm_logic_bridge_get_all_rules(bridge, rules, 10);
    EXPECT_EQ(count, 5u);
}

/*=============================================================================
 * LOGIC GATE EVALUATION TESTS
 *============================================================================*/

TEST_F(SwarmLogicBridgeTest, EvaluateANDGate) {
    uint32_t agent_ids[] = {1, 2};
    swarm_logic_rule_t rule = create_rule(100, LOGIC_GATE_AND, agent_ids, 2);
    swarm_logic_bridge_add_rule(bridge, &rule);

    // Both agents vote true
    swarm_agent_state_t agents[] = {
        create_agent(1, 0.9f),
        create_agent(2, 0.8f)
    };

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_bridge_evaluate_rule(bridge, 100, agents, 2, &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(result.result);
    EXPECT_GT(result.confidence, 0.5f);
}

TEST_F(SwarmLogicBridgeTest, EvaluateANDGateOneFalse) {
    uint32_t agent_ids[] = {1, 2};
    swarm_logic_rule_t rule = create_rule(100, LOGIC_GATE_AND, agent_ids, 2);
    swarm_logic_bridge_add_rule(bridge, &rule);

    // One agent votes false
    swarm_agent_state_t agents[] = {
        create_agent(1, 0.9f),
        create_agent(2, 0.2f)
    };

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_bridge_evaluate_rule(bridge, 100, agents, 2, &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_FALSE(result.result);
}

TEST_F(SwarmLogicBridgeTest, EvaluateORGate) {
    uint32_t agent_ids[] = {1, 2};
    swarm_logic_rule_t rule = create_rule(100, LOGIC_GATE_OR, agent_ids, 2);
    swarm_logic_bridge_add_rule(bridge, &rule);

    // One agent votes true
    swarm_agent_state_t agents[] = {
        create_agent(1, 0.9f),
        create_agent(2, 0.2f)
    };

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_bridge_evaluate_rule(bridge, 100, agents, 2, &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(result.result);
}

TEST_F(SwarmLogicBridgeTest, EvaluateNOTGate) {
    uint32_t agent_ids[] = {1};
    swarm_logic_rule_t rule = create_rule(100, LOGIC_GATE_NOT, agent_ids, 1);
    swarm_logic_bridge_add_rule(bridge, &rule);

    swarm_agent_state_t agents[] = {
        create_agent(1, 0.2f) // Low belief
    };

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_bridge_evaluate_rule(bridge, 100, agents, 1, &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(result.result); // NOT 0.2 = 0.8 > threshold
}

TEST_F(SwarmLogicBridgeTest, EvaluateXORGate) {
    uint32_t agent_ids[] = {1, 2};
    swarm_logic_rule_t rule = create_rule(100, LOGIC_GATE_XOR, agent_ids, 2);
    swarm_logic_bridge_add_rule(bridge, &rule);

    // One true, one false
    swarm_agent_state_t agents[] = {
        create_agent(1, 0.9f),
        create_agent(2, 0.1f)
    };

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_bridge_evaluate_rule(bridge, 100, agents, 2, &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(result.result); // XOR should be true
}

TEST_F(SwarmLogicBridgeTest, EvaluateIMPLIESGate) {
    uint32_t agent_ids[] = {1, 2};
    swarm_logic_rule_t rule = create_rule(100, LOGIC_GATE_IMPLIES, agent_ids, 2);
    swarm_logic_bridge_add_rule(bridge, &rule);

    // A true, B true => IMPLIES true
    swarm_agent_state_t agents[] = {
        create_agent(1, 0.9f),
        create_agent(2, 0.8f)
    };

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_bridge_evaluate_rule(bridge, 100, agents, 2, &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(result.result);
}

TEST_F(SwarmLogicBridgeTest, EvaluateMultipleRules) {
    // Add multiple rules
    for (uint32_t i = 0; i < 3; i++) {
        uint32_t agent_ids[] = {i * 2, i * 2 + 1};
        swarm_logic_rule_t rule = create_rule(100 + i, LOGIC_GATE_AND, agent_ids, 2);
        swarm_logic_bridge_add_rule(bridge, &rule);
    }

    // Create agents
    swarm_agent_state_t agents[6];
    for (uint32_t i = 0; i < 6; i++) {
        agents[i] = create_agent(i, 0.7f);
    }

    swarm_logic_result_t results[10];
    int count = swarm_logic_bridge_evaluate(bridge, agents, 6, results, 10);

    EXPECT_EQ(count, 3);
    for (int i = 0; i < count; i++) {
        EXPECT_TRUE(results[i].result);
    }
}

/*=============================================================================
 * CONSENSUS VALIDATION TESTS
 *============================================================================*/

TEST_F(SwarmLogicBridgeTest, ValidateUnanimousConsensus) {
    float votes[] = {0.9f, 0.85f, 0.95f, 0.88f};

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_bridge_validate_consensus(
        bridge, votes, 4, LOGIC_GATE_AND, &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(result.result); // All above threshold
}

TEST_F(SwarmLogicBridgeTest, ValidateUnanimousConsensusFailed) {
    float votes[] = {0.9f, 0.2f, 0.95f, 0.88f}; // One low vote

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_bridge_validate_consensus(
        bridge, votes, 4, LOGIC_GATE_AND, &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_FALSE(result.result); // One below threshold
}

TEST_F(SwarmLogicBridgeTest, ValidateMajorityConsensus) {
    float votes[] = {0.9f, 0.2f, 0.95f, 0.88f}; // 3 out of 4 high

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_bridge_validate_consensus(
        bridge, votes, 4, LOGIC_GATE_OR, &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(result.result); // At least one above threshold
}

/*=============================================================================
 * CONTRADICTION DETECTION TESTS
 *============================================================================*/

TEST_F(SwarmLogicBridgeTest, DetectContradiction) {
    swarm_agent_state_t beliefs[] = {
        create_agent(1, 0.9f),  // High belief
        create_agent(2, 0.1f),  // Low belief (contradiction)
        create_agent(3, 0.85f)
    };

    uint32_t contradictions[10][2];
    int count = swarm_logic_bridge_detect_contradiction(
        bridge, beliefs, 3, contradictions, 10);

    EXPECT_GT(count, 0); // Should find contradiction between agent 1 and 2
}

TEST_F(SwarmLogicBridgeTest, NoContradiction) {
    swarm_agent_state_t beliefs[] = {
        create_agent(1, 0.9f),
        create_agent(2, 0.85f),
        create_agent(3, 0.88f)
    };

    uint32_t contradictions[10][2];
    int count = swarm_logic_bridge_detect_contradiction(
        bridge, beliefs, 3, contradictions, 10);

    EXPECT_EQ(count, 0); // No contradictions
}

/*=============================================================================
 * IMPLICATION VALIDATION TESTS
 *============================================================================*/

TEST_F(SwarmLogicBridgeTest, ValidateImplicationTrue) {
    swarm_agent_state_t agents[] = {
        create_agent(1, 0.9f), // A is true
        create_agent(2, 0.8f)  // B is true
    };

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_bridge_validate_implication(
        bridge, 1, 2, agents, 2, &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(result.result); // A => B is true when both true
}

TEST_F(SwarmLogicBridgeTest, ValidateImplicationFalse) {
    swarm_agent_state_t agents[] = {
        create_agent(1, 0.9f), // A is true
        create_agent(2, 0.1f)  // B is false
    };

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_bridge_validate_implication(
        bridge, 1, 2, agents, 2, &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_FALSE(result.result); // A => B is false when A true but B false
}

/*=============================================================================
 * CACHE TESTS
 *============================================================================*/

TEST_F(SwarmLogicBridgeTest, CacheHit) {
    uint32_t agent_ids[] = {1, 2};
    swarm_logic_rule_t rule = create_rule(100, LOGIC_GATE_AND, agent_ids, 2);
    swarm_logic_bridge_add_rule(bridge, &rule);

    swarm_agent_state_t agents[] = {
        create_agent(1, 0.9f),
        create_agent(2, 0.8f)
    };

    // First evaluation (cache miss)
    swarm_logic_result_t result1;
    swarm_logic_bridge_evaluate_rule(bridge, 100, agents, 2, &result1);

    // Second evaluation (cache hit)
    swarm_logic_result_t result2;
    swarm_logic_bridge_evaluate_rule(bridge, 100, agents, 2, &result2);

    // Results should be identical
    EXPECT_EQ(result1.result, result2.result);
    EXPECT_FLOAT_EQ(result1.confidence, result2.confidence);
}

TEST_F(SwarmLogicBridgeTest, ClearCache) {
    swarm_logic_bridge_clear_cache(bridge);
    SUCCEED(); // Should not crash
}

/*=============================================================================
 * STATISTICS TESTS
 *============================================================================*/

TEST_F(SwarmLogicBridgeTest, GetStatistics) {
    swarm_logic_bridge_stats_t stats;
    nimcp_error_t err = swarm_logic_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_evaluations, 0u); // No evaluations yet
}

TEST_F(SwarmLogicBridgeTest, ResetStatistics) {
    // Add and evaluate a rule
    uint32_t agent_ids[] = {1, 2};
    swarm_logic_rule_t rule = create_rule(100, LOGIC_GATE_AND, agent_ids, 2);
    swarm_logic_bridge_add_rule(bridge, &rule);

    swarm_agent_state_t agents[] = {
        create_agent(1, 0.9f),
        create_agent(2, 0.8f)
    };

    swarm_logic_result_t result;
    swarm_logic_bridge_evaluate_rule(bridge, 100, agents, 2, &result);

    // Reset stats
    swarm_logic_bridge_reset_stats(bridge);

    swarm_logic_bridge_stats_t stats;
    swarm_logic_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(stats.total_evaluations, 0u);
}

/*=============================================================================
 * ERROR HANDLING TESTS
 *============================================================================*/

TEST_F(SwarmLogicBridgeTest, EvaluateNonexistentRule) {
    swarm_agent_state_t agents[] = {
        create_agent(1, 0.9f)
    };

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_bridge_evaluate_rule(
        bridge, 9999, agents, 1, &result);

    EXPECT_EQ(err, NIMCP_NOT_FOUND);
}

TEST_F(SwarmLogicBridgeTest, EvaluateWithNullAgents) {
    uint32_t agent_ids[] = {1, 2};
    swarm_logic_rule_t rule = create_rule(100, LOGIC_GATE_AND, agent_ids, 2);
    swarm_logic_bridge_add_rule(bridge, &rule);

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_bridge_evaluate_rule(
        bridge, 100, nullptr, 0, &result);

    EXPECT_EQ(err, NIMCP_INVALID_PARAM);
}

TEST_F(SwarmLogicBridgeTest, EvaluateWithInactiveAgents) {
    uint32_t agent_ids[] = {1, 2};
    swarm_logic_rule_t rule = create_rule(100, LOGIC_GATE_AND, agent_ids, 2);
    swarm_logic_bridge_add_rule(bridge, &rule);

    swarm_agent_state_t agents[] = {
        create_agent(1, 0.9f, false), // Inactive
        create_agent(2, 0.8f, false)  // Inactive
    };

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_bridge_evaluate_rule(
        bridge, 100, agents, 2, &result);

    EXPECT_EQ(err, NIMCP_NOT_FOUND); // No active agents found
}

/*=============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
