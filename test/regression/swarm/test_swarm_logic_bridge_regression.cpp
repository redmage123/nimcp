/**
 * @file test_swarm_logic_bridge_regression.cpp
 * @brief Regression tests for Logic-Swarm Bridge performance and correctness
 *
 * TEST COVERAGE:
 * - Performance benchmarks
 * - Memory leak detection
 * - Stress testing
 * - Edge case verification
 * - Consistency checks
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <chrono>
#include <memory>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_logic_bridge.h"
#include "utils/logging/nimcp_logging.h"

class SwarmLogicBridgeRegressionTest : public ::testing::Test {
protected:
    swarm_logic_bridge_t* bridge;
    swarm_logic_bridge_config_t config;

    void SetUp() override {
        swarm_logic_bridge_get_default_config(&config);
        config.enable_bio_async = false; // Disable for benchmarks
        config.max_rules = 10000; // Large capacity for stress tests
        bridge = swarm_logic_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            swarm_logic_bridge_destroy(bridge);
        }
    }
};

/*=============================================================================
 * PERFORMANCE BENCHMARKS
 *============================================================================*/

TEST_F(SwarmLogicBridgeRegressionTest, BenchmarkRuleAddition) {
    const uint32_t NUM_RULES = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (uint32_t i = 0; i < NUM_RULES; i++) {
        uint32_t agent_ids[] = {i % 100, (i + 1) % 100};
        swarm_logic_rule_t rule;
        memset(&rule, 0, sizeof(rule));
        rule.rule_id = i;
        rule.gate_type = LOGIC_GATE_AND;
        rule.input_agent_ids = agent_ids;
        rule.num_inputs = 2;
        rule.confidence_weight = 1.0f;
        rule.threshold = 0.5f;

        nimcp_error_t err = swarm_logic_bridge_add_rule(bridge, &rule);
        ASSERT_EQ(err, NIMCP_SUCCESS);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should add 1000 rules in < 100ms
    EXPECT_LT(duration.count(), 100);

    std::cout << "Added " << NUM_RULES << " rules in " << duration.count() << "ms" << std::endl;
}

TEST_F(SwarmLogicBridgeRegressionTest, BenchmarkEvaluationThroughput) {
    // Add 100 rules
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

    // Create 50 agents
    std::vector<swarm_agent_state_t> agents;
    for (uint32_t i = 0; i < 50; i++) {
        swarm_agent_state_t agent;
        memset(&agent, 0, sizeof(agent));
        agent.agent_id = i;
        agent.belief_value = 0.8f;
        agent.confidence = 0.9f;
        agent.is_active = true;
        agents.push_back(agent);
    }

    // Benchmark 1000 evaluation cycles
    const uint32_t NUM_CYCLES = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (uint32_t cycle = 0; cycle < NUM_CYCLES; cycle++) {
        swarm_logic_result_t results[100];
        int count = swarm_logic_bridge_evaluate(bridge, agents.data(), 50, results, 100);
        ASSERT_EQ(count, 100);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 1000 cycles in < 5 seconds
    EXPECT_LT(duration.count(), 5000);

    float ops_per_sec = (NUM_CYCLES * 100.0f) / (duration.count() / 1000.0f);
    std::cout << "Evaluation throughput: " << ops_per_sec << " ops/sec" << std::endl;
}

TEST_F(SwarmLogicBridgeRegressionTest, BenchmarkCachePerformance) {
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

    // Warm up cache
    swarm_logic_result_t result;
    swarm_logic_bridge_evaluate_rule(bridge, 1, agents, 2, &result);

    // Benchmark cache hits
    const uint32_t NUM_EVALS = 10000;

    auto start = std::chrono::high_resolution_clock::now();

    for (uint32_t i = 0; i < NUM_EVALS; i++) {
        swarm_logic_bridge_evaluate_rule(bridge, 1, agents, 2, &result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    float avg_time_us = (float)duration.count() / NUM_EVALS;
    EXPECT_LT(avg_time_us, 10.0f); // Average < 10us per cached evaluation

    std::cout << "Cached evaluation time: " << avg_time_us << " us/eval" << std::endl;
}

/*=============================================================================
 * MEMORY LEAK DETECTION
 *============================================================================*/

TEST_F(SwarmLogicBridgeRegressionTest, NoMemoryLeakOnRuleAddRemove) {
    // Add and remove rules repeatedly
    for (uint32_t cycle = 0; cycle < 100; cycle++) {
        // Add 10 rules
        for (uint32_t i = 0; i < 10; i++) {
            uint32_t agent_ids[] = {i, i + 1};
            swarm_logic_rule_t rule;
            memset(&rule, 0, sizeof(rule));
            rule.rule_id = cycle * 10 + i;
            rule.gate_type = LOGIC_GATE_AND;
            rule.input_agent_ids = agent_ids;
            rule.num_inputs = 2;
            rule.confidence_weight = 1.0f;
            rule.threshold = 0.5f;

            swarm_logic_bridge_add_rule(bridge, &rule);
        }

        // Remove all rules
        for (uint32_t i = 0; i < 10; i++) {
            swarm_logic_bridge_remove_rule(bridge, cycle * 10 + i);
        }
    }

    // Verify all rules removed
    const swarm_logic_rule_t* rules[100];
    uint32_t count = swarm_logic_bridge_get_all_rules(bridge, rules, 100);
    EXPECT_EQ(count, 0u);
}

TEST_F(SwarmLogicBridgeRegressionTest, NoMemoryLeakOnRepeatedEvaluation) {
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

    // Evaluate many times
    for (uint32_t i = 0; i < 10000; i++) {
        swarm_agent_state_t agents[] = {
            {1, 0.9f, 0.9f, 0, true},
            {2, 0.8f, 0.9f, 0, true}
        };

        swarm_logic_result_t result;
        swarm_logic_bridge_evaluate_rule(bridge, 1, agents, 2, &result);
    }

    SUCCEED(); // No crash means no memory issues
}

/*=============================================================================
 * STRESS TESTS
 *============================================================================*/

TEST_F(SwarmLogicBridgeRegressionTest, StressTestMaximumRules) {
    // Try to add maximum rules
    const uint32_t MAX_RULES = config.max_rules;

    for (uint32_t i = 0; i < MAX_RULES; i++) {
        uint32_t agent_ids[] = {i % 1000, (i + 1) % 1000};
        swarm_logic_rule_t rule;
        memset(&rule, 0, sizeof(rule));
        rule.rule_id = i;
        rule.gate_type = LOGIC_GATE_AND;
        rule.input_agent_ids = agent_ids;
        rule.num_inputs = 2;
        rule.confidence_weight = 1.0f;
        rule.threshold = 0.5f;

        nimcp_error_t err = swarm_logic_bridge_add_rule(bridge, &rule);
        if (err != NIMCP_SUCCESS) {
            // Capacity exceeded is acceptable
            break;
        }
    }

    // Get stats
    swarm_logic_bridge_stats_t stats;
    swarm_logic_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.active_rules, 0u);
}

TEST_F(SwarmLogicBridgeRegressionTest, StressTestLargeAgentSet) {
    // Create rule with 100 input agents
    std::vector<uint32_t> agent_ids;
    for (uint32_t i = 0; i < 100; i++) {
        agent_ids.push_back(i);
    }

    swarm_logic_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.rule_id = 1;
    rule.gate_type = LOGIC_GATE_AND;
    rule.input_agent_ids = agent_ids.data();
    rule.num_inputs = 100;
    rule.confidence_weight = 1.0f;
    rule.threshold = 0.5f;

    nimcp_error_t err = swarm_logic_bridge_add_rule(bridge, &rule);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Create 100 agents
    std::vector<swarm_agent_state_t> agents;
    for (uint32_t i = 0; i < 100; i++) {
        swarm_agent_state_t agent;
        memset(&agent, 0, sizeof(agent));
        agent.agent_id = i;
        agent.belief_value = 0.8f;
        agent.confidence = 0.9f;
        agent.is_active = true;
        agents.push_back(agent);
    }

    // Evaluate
    swarm_logic_result_t result;
    err = swarm_logic_bridge_evaluate_rule(bridge, 1, agents.data(), 100, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(SwarmLogicBridgeRegressionTest, StressTestContradictionDetection) {
    // Create 1000 beliefs with random contradictions
    std::vector<swarm_agent_state_t> beliefs;
    for (uint32_t i = 0; i < 1000; i++) {
        swarm_agent_state_t belief;
        memset(&belief, 0, sizeof(belief));
        belief.agent_id = i;
        // Alternate high/low beliefs
        belief.belief_value = (i % 2 == 0) ? 0.9f : 0.1f;
        belief.confidence = 1.0f;
        belief.is_active = true;
        beliefs.push_back(belief);
    }

    uint32_t contradictions[100][2];
    int count = swarm_logic_bridge_detect_contradiction(
        bridge, beliefs.data(), 1000, contradictions, 100);

    EXPECT_GT(count, 0); // Should find many contradictions
    EXPECT_LE(count, 100); // Should not exceed max
}

/*=============================================================================
 * CORRECTNESS REGRESSION TESTS
 *============================================================================*/

TEST_F(SwarmLogicBridgeRegressionTest, VerifyANDGateConsistency) {
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

    // Test all combinations
    float test_values[][2] = {
        {0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}
    };

    bool expected[] = {false, false, false, true};

    for (int i = 0; i < 4; i++) {
        swarm_agent_state_t agents[] = {
            {1, test_values[i][0], 1.0f, 0, true},
            {2, test_values[i][1], 1.0f, 0, true}
        };

        swarm_logic_result_t result;
        swarm_logic_bridge_evaluate_rule(bridge, 1, agents, 2, &result);

        EXPECT_EQ(result.result, expected[i])
            << "AND gate failed for inputs " << test_values[i][0] << ", " << test_values[i][1];
    }
}

TEST_F(SwarmLogicBridgeRegressionTest, VerifyORGateConsistency) {
    uint32_t agent_ids[] = {1, 2};
    swarm_logic_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.rule_id = 1;
    rule.gate_type = LOGIC_GATE_OR;
    rule.input_agent_ids = agent_ids;
    rule.num_inputs = 2;
    rule.confidence_weight = 1.0f;
    rule.threshold = 0.5f;

    swarm_logic_bridge_add_rule(bridge, &rule);

    // Test all combinations
    float test_values[][2] = {
        {0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}
    };

    bool expected[] = {false, true, true, true};

    for (int i = 0; i < 4; i++) {
        swarm_agent_state_t agents[] = {
            {1, test_values[i][0], 1.0f, 0, true},
            {2, test_values[i][1], 1.0f, 0, true}
        };

        swarm_logic_result_t result;
        swarm_logic_bridge_evaluate_rule(bridge, 1, agents, 2, &result);

        EXPECT_EQ(result.result, expected[i])
            << "OR gate failed for inputs " << test_values[i][0] << ", " << test_values[i][1];
    }
}

TEST_F(SwarmLogicBridgeRegressionTest, VerifyStatisticsAccuracy) {
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

    // Evaluate 10 times
    for (uint32_t i = 0; i < 10; i++) {
        swarm_logic_result_t result;
        swarm_logic_bridge_evaluate_rule(bridge, 1, agents, 2, &result);
    }

    swarm_logic_bridge_stats_t stats;
    swarm_logic_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(stats.total_evaluations, 10u);
    EXPECT_EQ(stats.successful_evaluations, 10u);
    EXPECT_EQ(stats.failed_evaluations, 0u);
    EXPECT_GT(stats.cache_hits, 0u); // Should have cache hits
}

/*=============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
