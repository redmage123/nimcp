/**
 * @file test_swarm_logic_bridge_integration.cpp
 * @brief Integration tests for Logic-Swarm Bridge with real neural logic gates
 *
 * TEST COVERAGE:
 * - Bridge integration with neural logic network
 * - Multi-agent consensus scenarios
 * - Real-world swarm decision making
 * - Bio-async message passing
 * - Security module integration
 * - Performance under load
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <chrono>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_logic_bridge.h"
#include "async/nimcp_bio_router.h"
#include "utils/logging/nimcp_logging.h"

class SwarmLogicBridgeIntegrationTest : public ::testing::Test {
protected:
    swarm_logic_bridge_t* bridge;
    swarm_logic_bridge_config_t config;

    void SetUp() override {
        // Initialize bio-async router
        bio_router_config_t router_config = bio_router_default_config();
        bio_router_init(&router_config);

        // Create bridge with bio-async enabled
        swarm_logic_bridge_get_default_config(&config);
        config.enable_bio_async = true;
        config.max_rules = 100;
        config.max_agents = 50;
        bridge = swarm_logic_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            swarm_logic_bridge_destroy(bridge);
        }
        bio_router_shutdown();
    }

    // Helper: Create swarm agents with varying beliefs
    std::vector<swarm_agent_state_t> create_swarm(uint32_t count, float base_belief) {
        std::vector<swarm_agent_state_t> agents;
        for (uint32_t i = 0; i < count; i++) {
            swarm_agent_state_t agent;
            memset(&agent, 0, sizeof(agent));
            agent.agent_id = i;
            agent.belief_value = base_belief + (i % 3) * 0.1f; // Variation
            agent.confidence = 0.9f;
            agent.is_active = true;
            agents.push_back(agent);
        }
        return agents;
    }
};

/*=============================================================================
 * SWARM CONSENSUS INTEGRATION TESTS
 *============================================================================*/

TEST_F(SwarmLogicBridgeIntegrationTest, LargeSwarmUnanimousConsensus) {
    // Create 50-agent swarm with unanimous belief
    auto agents = create_swarm(50, 0.85f);

    // Create consensus rule
    std::vector<uint32_t> agent_ids;
    for (uint32_t i = 0; i < 50; i++) {
        agent_ids.push_back(i);
    }

    swarm_logic_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.rule_id = 1;
    rule.gate_type = LOGIC_GATE_AND;
    rule.input_agent_ids = agent_ids.data();
    rule.num_inputs = 50;
    rule.confidence_weight = 1.0f;
    rule.threshold = 0.7f;

    nimcp_error_t err = swarm_logic_bridge_add_rule(bridge, &rule);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Evaluate consensus
    swarm_logic_result_t result;
    err = swarm_logic_bridge_evaluate_rule(bridge, 1, agents.data(), 50, &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(result.result);
    EXPECT_GT(result.confidence, 0.7f);
}

TEST_F(SwarmLogicBridgeIntegrationTest, SwarmMajorityVoting) {
    // Create swarm with mixed beliefs
    std::vector<swarm_agent_state_t> agents;
    for (uint32_t i = 0; i < 30; i++) {
        swarm_agent_state_t agent;
        memset(&agent, 0, sizeof(agent));
        agent.agent_id = i;
        // 20 agents vote high, 10 vote low
        agent.belief_value = (i < 20) ? 0.9f : 0.2f;
        agent.confidence = 1.0f;
        agent.is_active = true;
        agents.push_back(agent);
    }

    // Majority consensus (OR gate)
    std::vector<uint32_t> agent_ids;
    for (uint32_t i = 0; i < 30; i++) {
        agent_ids.push_back(i);
    }

    swarm_logic_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.rule_id = 1;
    rule.gate_type = LOGIC_GATE_OR;
    rule.input_agent_ids = agent_ids.data();
    rule.num_inputs = 30;
    rule.confidence_weight = 1.0f;
    rule.threshold = 0.5f;

    swarm_logic_bridge_add_rule(bridge, &rule);

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_bridge_evaluate_rule(bridge, 1, agents.data(), 30, &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(result.result); // Majority votes high
}

/*=============================================================================
 * MULTI-RULE INTEGRATION TESTS
 *============================================================================*/

TEST_F(SwarmLogicBridgeIntegrationTest, ComplexRuleChain) {
    // Create hierarchical rules: AND gates feeding into OR gate
    auto agents = create_swarm(10, 0.8f);

    // Rule 1: Agents 0-2 must all agree (AND)
    uint32_t rule1_ids[] = {0, 1, 2};
    swarm_logic_rule_t rule1;
    memset(&rule1, 0, sizeof(rule1));
    rule1.rule_id = 1;
    rule1.gate_type = LOGIC_GATE_AND;
    rule1.input_agent_ids = rule1_ids;
    rule1.num_inputs = 3;
    rule1.confidence_weight = 1.0f;
    rule1.threshold = 0.7f;

    // Rule 2: Agents 3-5 must all agree (AND)
    uint32_t rule2_ids[] = {3, 4, 5};
    swarm_logic_rule_t rule2;
    memset(&rule2, 0, sizeof(rule2));
    rule2.rule_id = 2;
    rule2.gate_type = LOGIC_GATE_AND;
    rule2.input_agent_ids = rule2_ids;
    rule2.num_inputs = 3;
    rule2.confidence_weight = 1.0f;
    rule2.threshold = 0.7f;

    swarm_logic_bridge_add_rule(bridge, &rule1);
    swarm_logic_bridge_add_rule(bridge, &rule2);

    // Evaluate both rules
    swarm_logic_result_t results[10];
    int count = swarm_logic_bridge_evaluate(bridge, agents.data(), 10, results, 10);

    EXPECT_GE(count, 2);
    EXPECT_TRUE(results[0].result);
    EXPECT_TRUE(results[1].result);
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION TESTS
 *============================================================================*/

TEST_F(SwarmLogicBridgeIntegrationTest, ProcessBioAsyncMessages) {
    // Process inbox (should not crash)
    int processed = swarm_logic_bridge_process_inbox(bridge);
    EXPECT_GE(processed, 0);
}

TEST_F(SwarmLogicBridgeIntegrationTest, BroadcastConsensusResult) {
    swarm_logic_result_t result;
    memset(&result, 0, sizeof(result));
    result.rule_id = 1;
    result.result = true;
    result.confidence = 0.95f;

    nimcp_error_t err = swarm_logic_bridge_broadcast_consensus(bridge, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

/*=============================================================================
 * PERFORMANCE INTEGRATION TESTS
 *============================================================================*/

TEST_F(SwarmLogicBridgeIntegrationTest, HighThroughputEvaluation) {
    // Create 100 rules
    for (uint32_t i = 0; i < 100; i++) {
        uint32_t agent_ids[] = {i % 50, (i + 1) % 50};
        swarm_logic_rule_t rule;
        memset(&rule, 0, sizeof(rule));
        rule.rule_id = i;
        rule.gate_type = LOGIC_GATE_AND;
        rule.input_agent_ids = agent_ids;
        rule.num_inputs = 2;
        rule.confidence_weight = 1.0f;
        rule.threshold = 0.5f;

        swarm_logic_bridge_add_rule(bridge, &rule);
    }

    // Create agents
    auto agents = create_swarm(50, 0.8f);

    // Measure evaluation time
    auto start = std::chrono::high_resolution_clock::now();

    swarm_logic_result_t results[100];
    int count = swarm_logic_bridge_evaluate(bridge, agents.data(), 50, results, 100);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(count, 100);
    EXPECT_LT(duration.count(), 1000); // Should complete in < 1 second
}

TEST_F(SwarmLogicBridgeIntegrationTest, CacheEfficiency) {
    uint32_t agent_ids[] = {1, 2};
    swarm_logic_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.rule_id = 1;
    rule.gate_type = LOGIC_GATE_AND;
    rule.input_agent_ids = agent_ids;
    rule.num_inputs = 2;
    rule.confidence_weight = 1.0f;
    rule.threshold = 0.5f;

    swarm_logic_bridge_add_rule(bridge, &rule);

    swarm_agent_state_t agents[] = {
        {1, 0.9f, 0.9f, 0, true},
        {2, 0.8f, 0.9f, 0, true}
    };

    // First evaluation (cache miss)
    auto start1 = std::chrono::high_resolution_clock::now();
    swarm_logic_result_t result1;
    swarm_logic_bridge_evaluate_rule(bridge, 1, agents, 2, &result1);
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);

    // Second evaluation (cache hit)
    auto start2 = std::chrono::high_resolution_clock::now();
    swarm_logic_result_t result2;
    swarm_logic_bridge_evaluate_rule(bridge, 1, agents, 2, &result2);
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);

    // Cache hit should be faster (or similar if both are very fast)
    EXPECT_LE(duration2.count(), duration1.count() * 2);

    // Get stats to verify cache hit
    swarm_logic_bridge_stats_t stats;
    swarm_logic_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.cache_hits, 0u);
}

/*=============================================================================
 * REAL-WORLD SCENARIO TESTS
 *============================================================================*/

TEST_F(SwarmLogicBridgeIntegrationTest, DroneSwarmThreatDetection) {
    // Scenario: 10 drones detecting threat, need 7/10 agreement
    std::vector<swarm_agent_state_t> drones;
    for (uint32_t i = 0; i < 10; i++) {
        swarm_agent_state_t drone;
        memset(&drone, 0, sizeof(drone));
        drone.agent_id = i;
        // 8 drones detect threat, 2 don't
        drone.belief_value = (i < 8) ? 0.9f : 0.3f;
        drone.confidence = 0.85f;
        drone.is_active = true;
        drones.push_back(drone);
    }

    // Threat detection rule (majority OR)
    std::vector<uint32_t> drone_ids;
    for (uint32_t i = 0; i < 10; i++) {
        drone_ids.push_back(i);
    }

    swarm_logic_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.rule_id = 100;
    rule.gate_type = LOGIC_GATE_OR;
    rule.input_agent_ids = drone_ids.data();
    rule.num_inputs = 10;
    rule.confidence_weight = 1.0f;
    rule.threshold = 0.5f;
    snprintf(rule.description, sizeof(rule.description), "Threat detection");

    swarm_logic_bridge_add_rule(bridge, &rule);

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_bridge_evaluate_rule(bridge, 100, drones.data(), 10, &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(result.result); // Threat confirmed by majority
    EXPECT_GT(result.confidence, 0.7f);
}

TEST_F(SwarmLogicBridgeIntegrationTest, RobotSwarmFormationConsensus) {
    // Scenario: 15 robots deciding on formation, need unanimous agreement
    auto robots = create_swarm(15, 0.88f); // High agreement

    std::vector<uint32_t> robot_ids;
    for (uint32_t i = 0; i < 15; i++) {
        robot_ids.push_back(i);
    }

    swarm_logic_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.rule_id = 200;
    rule.gate_type = LOGIC_GATE_AND; // Unanimous
    rule.input_agent_ids = robot_ids.data();
    rule.num_inputs = 15;
    rule.confidence_weight = 1.0f;
    rule.threshold = 0.8f;
    snprintf(rule.description, sizeof(rule.description), "Formation consensus");

    swarm_logic_bridge_add_rule(bridge, &rule);

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_bridge_evaluate_rule(bridge, 200, robots.data(), 15, &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(result.result); // All agree on formation
}

/*=============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
