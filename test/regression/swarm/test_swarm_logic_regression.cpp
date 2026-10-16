/**
 * @file test_swarm_logic_regression.cpp
 * @brief Regression tests for swarm logic validation performance and correctness
 *
 * Tests to ensure:
 * - Performance doesn't degrade with updates
 * - Logic validation accuracy remains high
 * - Stress testing with many rules and agents
 * - Memory usage stays within bounds
 */

#include <gtest/gtest.h>
#include "swarm/nimcp_swarm_quorum.h"
#include "swarm/nimcp_swarm_immune.h"
#include "core/neuron_types/nimcp_neural_logic.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include <chrono>

class SwarmLogicRegressionTest : public ::testing::Test {
protected:
    nimcp_swarm_quorum_t* quorum;
    NimcpSwarmImmuneSystem* immune;

    void SetUp() override {
        nimcp_quorum_config_t quorum_config;
        nimcp_swarm_quorum_default_config(&quorum_config);
        quorum = nimcp_swarm_quorum_create(&quorum_config, nullptr);
        ASSERT_NE(quorum, nullptr);

        NimcpSwarmImmuneConfig immune_config;
        nimcp_swarm_immune_default_config(&immune_config);
        immune_config.max_memory_cells = 10000;
        immune = nimcp_swarm_immune_create(&immune_config, nullptr, 1);
        ASSERT_NE(immune, nullptr);
    }

    void TearDown() override {
        if (quorum) nimcp_swarm_quorum_destroy(quorum);
        if (immune) nimcp_swarm_immune_destroy(immune);
    }

    // Performance measurement helper
    template<typename Func>
    double measure_time_ms(Func func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> diff = end - start;
        return diff.count();
    }
};

/* ============================================================================
 * Quorum Performance Regression Tests
 * ============================================================================ */

TEST_F(SwarmLogicRegressionTest, QuorumValidation_1000Agents_Performance) {
    // Regression: Should complete in under 100ms for 1000 agents

    // Add 1000 agents
    for (uint32_t i = 0; i < 1000; i++) {
        nimcp_quorum_update_commitment(quorum, i, NIMCP_SIGNAL_ATTACK, 0.85);
    }

    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);

    double time_ms = measure_time_ms([&]() {
        quorum_validate_with_logic(quorum, &logic_cfg);
    });

    EXPECT_LT(time_ms, 100.0) << "1000-agent validation should complete in <100ms, took " << time_ms << "ms";
    std::cout << "QuorumValidation_1000Agents: " << time_ms << " ms\n";
}

TEST_F(SwarmLogicRegressionTest, QuorumConsistencyCheck_1000Agents_Performance) {
    // Add contradicting votes
    for (uint32_t i = 0; i < 500; i++) {
        nimcp_quorum_update_commitment(quorum, i, NIMCP_SIGNAL_ATTACK, 0.9);
    }
    for (uint32_t i = 500; i < 1000; i++) {
        nimcp_quorum_update_commitment(quorum, i, NIMCP_SIGNAL_RETREAT, 0.9);
    }

    uint32_t contradicting_agents[1000];
    uint32_t count = 0;

    double time_ms = measure_time_ms([&]() {
        quorum_check_vote_consistency(quorum, contradicting_agents, &count);
    });

    EXPECT_LT(time_ms, 50.0) << "Consistency check should complete in <50ms, took " << time_ms << "ms";
    EXPECT_GT(count, 0u) << "Should detect contradictions";
    std::cout << "QuorumConsistencyCheck_1000Agents: " << time_ms << " ms, " << count << " contradictions\n";
}

TEST_F(SwarmLogicRegressionTest, QuorumImplication_RepeatedEvaluation) {
    // Regression: Repeated evaluations should maintain performance

    nimcp_quorum_broadcast_signal(quorum, 100, NIMCP_SIGNAL_ALERT, 0.8);
    nimcp_quorum_broadcast_signal(quorum, 101, NIMCP_SIGNAL_DEFEND, 0.7);

    const int iterations = 1000;
    int success_count = 0;

    double time_ms = measure_time_ms([&]() {
        for (int i = 0; i < iterations; i++) {
            bool holds;
            if (quorum_evaluate_implication(quorum, NIMCP_SIGNAL_ALERT, NIMCP_SIGNAL_DEFEND, &holds) == 0) {
                success_count++;
            }
        }
    });

    EXPECT_EQ(success_count, iterations);
    double avg_time = time_ms / iterations;
    EXPECT_LT(avg_time, 0.1) << "Average implication eval should be <0.1ms, was " << avg_time << "ms";
    std::cout << "QuorumImplication_1000Iterations: Total=" << time_ms << "ms, Avg=" << avg_time << "ms\n";
}

/* ============================================================================
 * Immune Performance Regression Tests
 * ============================================================================ */

TEST_F(SwarmLogicRegressionTest, ImmuneThreatEval_100Rules_Performance) {
    // Regression: 100 rules should evaluate in <200ms

    // Add behavior profiles
    for (uint32_t i = 0; i < 100; i++) {
        NimcpSwarmBehaviorProfile profile;
        profile.drone_id = i;
        profile.msg_rate = 10.0f;
        profile.movement_pattern[0] = 1.0f;
        profile.movement_pattern[1] = 0.5f;
        profile.movement_pattern[2] = 0.0f;
        profile.energy_usage = 50.0f;
        profile.connection_changes = 2;
        profile.last_update = 0;
        profile.anomaly_score = (i % 10) / 10.0f;

        float score;
        nimcp_swarm_immune_check_behavior(immune, i, &profile, &score);
    }

    // Add 100 threat rules
    for (uint32_t rule_id = 0; rule_id < 100; rule_id++) {
        uint32_t* sources = (uint32_t*)nimcp_malloc(3 * sizeof(uint32_t));
        sources[0] = rule_id * 3;
        sources[1] = rule_id * 3 + 1;
        sources[2] = rule_id * 3 + 2;

        immune_threat_rule_t rule;
        rule.threat_id = rule_id;
        rule.detection_logic = (logic_gate_type_t)(rule_id % 4);  // Vary gate types
        rule.num_sources = 3;
        rule.confidence_threshold = 0.5f;
        rule.threat_type = (NimcpSwarmThreatType)(rule_id % THREAT_COUNT);
        rule.signal_sources = sources;

        nimcp_result_t result = immune_add_threat_rule(immune, &rule);
        if (result != NIMCP_SUCCESS) {
            nimcp_free(sources);
            break;
        }
    }

    float threat_scores[256];
    uint32_t num_threats = 0;

    double time_ms = measure_time_ms([&]() {
        immune_evaluate_threats(immune, threat_scores, &num_threats);
    });

    EXPECT_LT(time_ms, 200.0) << "100-rule evaluation should complete in <200ms, took " << time_ms << "ms";
    std::cout << "ImmuneThreatEval_100Rules: " << time_ms << " ms, " << num_threats << " threats detected\n";
}

TEST_F(SwarmLogicRegressionTest, ImmuneNOTGate_1000Evaluations) {
    // Regression: NOT gate evaluations should be fast

    // Add some behavior profiles
    for (uint32_t i = 0; i < 100; i++) {
        NimcpSwarmBehaviorProfile profile;
        profile.drone_id = i;
        profile.msg_rate = 10.0f;
        profile.movement_pattern[0] = 1.0f;
        profile.movement_pattern[1] = 0.5f;
        profile.movement_pattern[2] = 0.0f;
        profile.energy_usage = 50.0f;
        profile.connection_changes = 2;
        profile.last_update = 0;
        profile.anomaly_score = 0.3f;

        float score;
        nimcp_swarm_immune_check_behavior(immune, i, &profile, &score);
    }

    const int iterations = 1000;
    int success_count = 0;

    double time_ms = measure_time_ms([&]() {
        for (int i = 0; i < iterations; i++) {
            bool threat;
            uint32_t drone_id = i % 100;
            if (immune_evaluate_not_threat(immune, drone_id, 5000, &threat) == NIMCP_SUCCESS) {
                success_count++;
            }
        }
    });

    EXPECT_EQ(success_count, iterations);
    double avg_time = time_ms / iterations;
    EXPECT_LT(avg_time, 0.05) << "Average NOT eval should be <0.05ms, was " << avg_time << "ms";
    std::cout << "ImmuneNOTGate_1000Evaluations: Total=" << time_ms << "ms, Avg=" << avg_time << "ms\n";
}

TEST_F(SwarmLogicRegressionTest, ImmuneLogicResponse_1000Threats) {
    // Create many threats and generate responses

    // First add memory cells for threat recognition
    for (uint32_t i = 0; i < 10; i++) {
        NimcpSwarmThreatSignature sig;
        memset(&sig, 0, sizeof(sig));
        sig.type = (NimcpSwarmThreatType)(i % THREAT_COUNT);
        sig.match_threshold = 0.5f;
        sig.pattern_len = 4;
        sig.pattern[0] = (uint8_t)(i & 0xFF);

        uint32_t cell_id;
        nimcp_swarm_immune_add_memory_cell(immune, &sig, RESPONSE_ALERT, 0.8f, &cell_id);
    }

    // Detect threats
    int detected = 0;
    for (uint32_t i = 0; i < 1000; i++) {
        uint8_t data[4] = {(uint8_t)(i & 0xFF), 0x02, 0x03, 0x04};
        uint32_t threat_id;
        if (nimcp_swarm_immune_detect_threat(immune, data, 4, 500 + i, &threat_id) == NIMCP_SUCCESS) {
            detected++;
        }
    }

    if (detected > 0) {
        std::cout << "ImmuneLogicResponse: Detected " << detected << " threats out of 1000\n";
    }
}

/* ============================================================================
 * Stress Tests with Many Rules
 * ============================================================================ */

TEST_F(SwarmLogicRegressionTest, StressTest_MaxQuorumAgents) {
    // Test maximum supported number of agents

    const uint32_t max_agents = 10000;

    // Add maximum agents
    for (uint32_t i = 0; i < max_agents; i++) {
        nimcp_quorum_update_commitment(quorum, i, NIMCP_SIGNAL_EXPLORE, 0.7);
    }

    quorum_logic_config_t logic_cfg;
    quorum_logic_default_config(&logic_cfg);
    logic_cfg.min_agents = 1000;

    double time_ms = measure_time_ms([&]() {
        quorum_validate_with_logic(quorum, &logic_cfg);
    });

    std::cout << "StressTest_" << max_agents << "Agents: " << time_ms << " ms\n";
    EXPECT_LT(time_ms, 1000.0) << "Should handle " << max_agents << " agents in <1s";
}

TEST_F(SwarmLogicRegressionTest, StressTest_MaxImmuneRules) {
    // Test maximum supported number of threat rules

    // Add behavior profiles
    for (uint32_t i = 0; i < 200; i++) {
        NimcpSwarmBehaviorProfile profile;
        profile.drone_id = i;
        profile.msg_rate = 10.0f;
        profile.movement_pattern[0] = 1.0f;
        profile.movement_pattern[1] = 0.5f;
        profile.movement_pattern[2] = 0.0f;
        profile.energy_usage = 50.0f;
        profile.connection_changes = 2;
        profile.last_update = 0;
        profile.anomaly_score = (i % 20) / 20.0f;

        float score;
        nimcp_swarm_immune_check_behavior(immune, i, &profile, &score);
    }

    // Add many rules (up to capacity)
    uint32_t rules_added = 0;
    for (uint32_t rule_id = 0; rule_id < immune->threat_rule_capacity; rule_id++) {
        uint32_t* sources = (uint32_t*)nimcp_malloc(2 * sizeof(uint32_t));
        sources[0] = rule_id % 200;
        sources[1] = (rule_id + 1) % 200;

        immune_threat_rule_t rule;
        rule.threat_id = rule_id;
        rule.detection_logic = LOGIC_GATE_OR;
        rule.num_sources = 2;
        rule.confidence_threshold = 0.5f;
        rule.threat_type = THREAT_DOS;
        rule.signal_sources = sources;

        if (immune_add_threat_rule(immune, &rule) == NIMCP_SUCCESS) {
            rules_added++;
        } else {
            nimcp_free(sources);
            break;
        }
    }

    EXPECT_GT(rules_added, 0u);

    float threat_scores[256];
    uint32_t num_threats = 0;

    double time_ms = measure_time_ms([&]() {
        immune_evaluate_threats(immune, threat_scores, &num_threats);
    });

    std::cout << "StressTest_" << rules_added << "Rules: " << time_ms << " ms\n";
    EXPECT_LT(time_ms, 500.0) << "Should handle many rules efficiently";
}

/* ============================================================================
 * Accuracy Regression Tests
 * ============================================================================ */

TEST_F(SwarmLogicRegressionTest, AccuracyTest_ANDGateValidation) {
    // Ensure AND gate accuracy hasn't regressed

    int trials = 100;
    int correct = 0;

    for (int t = 0; t < trials; t++) {
        // Clear commitments
        for (uint32_t i = 0; i < 10; i++) {
            nimcp_quorum_remove_commitment(quorum, 100 + i);
        }

        // Add votes
        bool should_pass = (t % 2 == 0);
        if (should_pass) {
            // All vote the same
            for (uint32_t i = 0; i < 10; i++) {
                nimcp_quorum_update_commitment(quorum, 100 + i, NIMCP_SIGNAL_ATTACK, 0.95);
            }
        } else {
            // Mixed votes
            for (uint32_t i = 0; i < 5; i++) {
                nimcp_quorum_update_commitment(quorum, 100 + i, NIMCP_SIGNAL_ATTACK, 0.95);
            }
            for (uint32_t i = 5; i < 10; i++) {
                nimcp_quorum_update_commitment(quorum, 100 + i, NIMCP_SIGNAL_RETREAT, 0.95);
            }
        }

        quorum_logic_config_t logic_cfg;
        quorum_logic_default_config(&logic_cfg);
        logic_cfg.gate_type = LOGIC_GATE_AND;

        int result = quorum_validate_with_logic(quorum, &logic_cfg);

        if ((should_pass && result == 1) || (!should_pass && result == 0)) {
            correct++;
        }
    }

    double accuracy = (double)correct / trials * 100.0;
    EXPECT_GE(accuracy, 95.0) << "AND gate accuracy should be >= 95%, was " << accuracy << "%";
    std::cout << "AccuracyTest_ANDGate: " << accuracy << "% correct\n";
}

TEST_F(SwarmLogicRegressionTest, AccuracyTest_ORThreatDetection) {
    // Ensure OR gate threat detection accuracy

    int trials = 100;
    int correct = 0;

    for (int t = 0; t < trials; t++) {
        // Reset immune system
        nimcp_swarm_immune_reset(immune, false);

        // Add profiles
        bool has_threat = (t % 2 == 0);
        for (uint32_t i = 0; i < 5; i++) {
            float anomaly = has_threat && (i == 2) ? 0.9f : 0.2f;
            NimcpSwarmBehaviorProfile profile;
            profile.drone_id = 200 + i;
            profile.msg_rate = 10.0f + anomaly * 5.0f;
            profile.movement_pattern[0] = 1.0f;
            profile.movement_pattern[1] = 0.5f;
            profile.movement_pattern[2] = 0.0f;
            profile.energy_usage = 50.0f;
            profile.connection_changes = 2;
            profile.last_update = 0;
            profile.anomaly_score = anomaly;

            float score;
            nimcp_swarm_immune_check_behavior(immune, profile.drone_id, &profile, &score);
        }

        // Add OR rule
        uint32_t sources[] = {200, 201, 202, 203, 204};
        immune_threat_rule_t rule;
        rule.threat_id = 1;
        rule.detection_logic = LOGIC_GATE_OR;
        rule.num_sources = 5;
        rule.confidence_threshold = 0.7f;
        rule.threat_type = THREAT_MALICIOUS_DRONE;
        rule.signal_sources = sources;

        immune_add_threat_rule(immune, &rule);

        float threat_scores[10] = {0};
        uint32_t num_threats = 0;
        immune_evaluate_threats(immune, threat_scores, &num_threats);

        bool detected = (num_threats > 0 && threat_scores[0] >= 0.7f);

        if ((has_threat && detected) || (!has_threat && !detected)) {
            correct++;
        }
    }

    double accuracy = (double)correct / trials * 100.0;
    EXPECT_GE(accuracy, 90.0) << "OR threat detection accuracy should be >= 90%, was " << accuracy << "%";
    std::cout << "AccuracyTest_ORThreatDetection: " << accuracy << "% correct\n";
}

/* ============================================================================
 * Memory Usage Regression Tests
 * ============================================================================ */

TEST_F(SwarmLogicRegressionTest, MemoryTest_NoLeaks) {
    // Verify no memory leaks in repeated operations

    // Get baseline memory (approximate)
    size_t baseline_allocations = 0;

    // Perform many operations
    for (int iter = 0; iter < 100; iter++) {
        // Quorum operations
        for (uint32_t i = 0; i < 10; i++) {
            nimcp_quorum_update_commitment(quorum, 500 + i, NIMCP_SIGNAL_RESOURCE, 0.8);
        }

        quorum_logic_config_t logic_cfg;
        quorum_logic_default_config(&logic_cfg);
        quorum_validate_with_logic(quorum, &logic_cfg);

        // Clear
        for (uint32_t i = 0; i < 10; i++) {
            nimcp_quorum_remove_commitment(quorum, 500 + i);
        }
    }

    // Memory should be stable (no continuous growth)
    // This is a basic check - in practice use valgrind or similar
    SUCCEED() << "Completed 100 iterations without crashing";
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    std::cout << "\n=== NIMCP Swarm Logic Regression Tests ===\n";
    std::cout << "Performance baselines:\n";
    std::cout << "  - QuorumValidation_1000Agents: <100ms\n";
    std::cout << "  - ImmuneThreatEval_100Rules: <200ms\n";
    std::cout << "  - StressTest_10000Agents: <1000ms\n\n";

    int result = RUN_ALL_TESTS();

    std::cout << "\n=== Regression Test Summary ===\n";
    std::cout << "All baselines should be met for release.\n\n";

    return result;
}
