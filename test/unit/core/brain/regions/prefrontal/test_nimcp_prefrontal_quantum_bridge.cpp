/**
 * @file test_nimcp_prefrontal_quantum_bridge.cpp
 * @brief Unit tests for nimcp_prefrontal_quantum_bridge.c
 *
 * WHAT: Comprehensive unit tests for the Prefrontal Quantum Bridge
 * WHY:  Ensure correct quantum-accelerated decision-making and planning
 * HOW:  Use Google Test framework to test lifecycle, decision acceleration,
 *       planning optimization, conflict resolution, and statistics tracking.
 *
 * COVERAGE TARGET: 100%
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>

// Headers have their own extern "C" guards
#include "core/brain/regions/prefrontal/nimcp_prefrontal_quantum_bridge.h"

// Test Fixture for Prefrontal Quantum Bridge
class PrefrontalQuantumBridgeTest : public ::testing::Test {
protected:
    prefrontal_quantum_bridge_t* bridge;
    prefrontal_quantum_config_t config;

    void SetUp() override {
        config = prefrontal_quantum_default_config();
        bridge = prefrontal_quantum_bridge_create(NULL, &config);  // NULL prefrontal for isolated testing
        ASSERT_NE(nullptr, bridge) << "Failed to create Prefrontal Quantum Bridge";
    }

    void TearDown() override {
        prefrontal_quantum_bridge_destroy(bridge);
        bridge = nullptr;
    }
};

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(PrefrontalQuantumBridgeTest, DefaultConfigHasReasonableValues) {
    prefrontal_quantum_config_t default_config = prefrontal_quantum_default_config();

    EXPECT_TRUE(default_config.enabled);
    EXPECT_EQ(default_config.max_decision_qubits, 10u);
    EXPECT_EQ(default_config.max_planning_qubits, 12u);
    EXPECT_EQ(default_config.max_grover_iterations, 10u);
    EXPECT_GT(default_config.min_decision_confidence, 0.0f);
    EXPECT_GT(default_config.min_speedup_threshold, 1.0f);
    EXPECT_TRUE(default_config.enable_superposition_eval);
    EXPECT_TRUE(default_config.enable_quantum_annealing);
    EXPECT_TRUE(default_config.use_amplitude_estimation);
}

TEST_F(PrefrontalQuantumBridgeTest, CreateWithNullConfigUsesDefaults) {
    prefrontal_quantum_bridge_t* bridge_null = prefrontal_quantum_bridge_create(NULL, NULL);
    ASSERT_NE(nullptr, bridge_null);

    prefrontal_quantum_config_t retrieved;
    EXPECT_EQ(prefrontal_quantum_get_config(bridge_null, &retrieved), 0);
    EXPECT_EQ(retrieved.max_decision_qubits, 10u);

    prefrontal_quantum_bridge_destroy(bridge_null);
}

TEST_F(PrefrontalQuantumBridgeTest, DestroyNullDoesNotCrash) {
    prefrontal_quantum_bridge_destroy(NULL);
    // Should not crash
}

TEST_F(PrefrontalQuantumBridgeTest, IsEnabledInitially) {
    EXPECT_TRUE(prefrontal_quantum_bridge_is_enabled(bridge));
}

TEST_F(PrefrontalQuantumBridgeTest, SetEnabled) {
    prefrontal_quantum_bridge_set_enabled(bridge, false);
    EXPECT_FALSE(prefrontal_quantum_bridge_is_enabled(bridge));

    prefrontal_quantum_bridge_set_enabled(bridge, true);
    EXPECT_TRUE(prefrontal_quantum_bridge_is_enabled(bridge));
}

TEST_F(PrefrontalQuantumBridgeTest, IsEnabledNull) {
    EXPECT_FALSE(prefrontal_quantum_bridge_is_enabled(NULL));
}

// ============================================================================
// QUANTUM DECISION ACCELERATION TESTS
// ============================================================================

TEST_F(PrefrontalQuantumBridgeTest, AccelerateDecisionBasic) {
    float utilities[] = {0.3f, 0.7f, 0.5f, 0.9f, 0.4f};
    uint32_t num_options = 5;
    float min_utility = 0.6f;

    quantum_decision_result_t result;
    int ret = prefrontal_quantum_accelerate_decision(bridge, utilities, num_options, min_utility, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_NE(result.best_candidate, nullptr);
    EXPECT_EQ(result.candidates_evaluated, num_options);
    EXPECT_GE(result.satisfaction_probability, 0.0f);
    EXPECT_GE(result.quantum_speedup, 1.0f);
}

TEST_F(PrefrontalQuantumBridgeTest, AccelerateDecisionNoSatisfying) {
    float utilities[] = {0.1f, 0.2f, 0.3f};
    uint32_t num_options = 3;
    float min_utility = 0.9f;  // No option satisfies this

    quantum_decision_result_t result;
    int ret = prefrontal_quantum_accelerate_decision(bridge, utilities, num_options, min_utility, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_NE(result.best_candidate, nullptr);  // Should fall back to best classical
    EXPECT_FALSE(result.used_quantum);  // Classical fallback
}

TEST_F(PrefrontalQuantumBridgeTest, AccelerateDecisionNullParams) {
    float utilities[] = {0.5f, 0.6f};
    quantum_decision_result_t result;

    EXPECT_EQ(prefrontal_quantum_accelerate_decision(NULL, utilities, 2, 0.5f, &result), -1);
    EXPECT_EQ(prefrontal_quantum_accelerate_decision(bridge, NULL, 2, 0.5f, &result), -1);
    EXPECT_EQ(prefrontal_quantum_accelerate_decision(bridge, utilities, 2, 0.5f, NULL), -1);
}

TEST_F(PrefrontalQuantumBridgeTest, AccelerateDecisionDisabled) {
    prefrontal_quantum_bridge_set_enabled(bridge, false);

    float utilities[] = {0.5f, 0.7f, 0.3f};
    quantum_decision_result_t result;

    EXPECT_EQ(prefrontal_quantum_accelerate_decision(bridge, utilities, 3, 0.5f, &result), -1);
}

TEST_F(PrefrontalQuantumBridgeTest, ParallelEvalBasic) {
    const float* options[3];
    float opt1[] = {0.5f, 0.3f, 0.8f};
    float opt2[] = {0.7f, 0.2f, 0.4f};
    float opt3[] = {0.4f, 0.6f, 0.5f};
    options[0] = opt1;
    options[1] = opt2;
    options[2] = opt3;

    float criteria[] = {0.5f, 0.3f, 0.2f};

    quantum_decision_result_t result;
    int ret = prefrontal_quantum_parallel_eval(bridge, options, 3, 3, criteria, 3, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_NE(result.best_candidate, nullptr);
}

TEST_F(PrefrontalQuantumBridgeTest, ParallelEvalNullParams) {
    float criteria[] = {0.5f, 0.5f};
    quantum_decision_result_t result;

    EXPECT_EQ(prefrontal_quantum_parallel_eval(NULL, NULL, 0, 0, criteria, 2, &result), -1);
    EXPECT_EQ(prefrontal_quantum_parallel_eval(bridge, NULL, 0, 0, criteria, 2, &result), -1);
}

// ============================================================================
// QUANTUM PLANNING OPTIMIZATION TESTS
// ============================================================================

TEST_F(PrefrontalQuantumBridgeTest, OptimizePlanBasic) {
    uint32_t available_actions[] = {1, 2, 3, 4, 5};
    float constraints[] = {0.0f};  // Simplified constraints
    float values[] = {0.5f, 0.8f, 0.3f, 0.7f, 0.6f};

    quantum_planning_result_t result;
    int ret = prefrontal_quantum_optimize_plan(bridge, available_actions, 5, constraints, values, 4, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_NE(result.best_plan, nullptr);
    EXPECT_GT(result.plans_explored, 0u);
    EXPECT_GT(result.quantum_speedup, 0.0f);
}

TEST_F(PrefrontalQuantumBridgeTest, OptimizePlanNullParams) {
    uint32_t actions[] = {1, 2, 3};
    float values[] = {0.5f, 0.6f, 0.7f};
    quantum_planning_result_t result;

    EXPECT_EQ(prefrontal_quantum_optimize_plan(NULL, actions, 3, NULL, values, 3, &result), -1);
    EXPECT_EQ(prefrontal_quantum_optimize_plan(bridge, NULL, 3, NULL, values, 3, &result), -1);
    EXPECT_EQ(prefrontal_quantum_optimize_plan(bridge, actions, 3, NULL, NULL, 3, &result), -1);
    EXPECT_EQ(prefrontal_quantum_optimize_plan(bridge, actions, 3, NULL, values, 3, NULL), -1);
}

TEST_F(PrefrontalQuantumBridgeTest, TreeSearchBasic) {
    // Simple 4-node tree
    uint8_t adjacency[] = {
        0, 1, 1, 0,
        0, 0, 0, 1,
        0, 0, 0, 1,
        0, 0, 0, 0
    };
    float node_values[] = {0.0f, 0.5f, 0.6f, 0.9f};
    bool goal_nodes[] = {false, false, false, true};

    quantum_planning_result_t result;
    int ret = prefrontal_quantum_tree_search(bridge, adjacency, 4, node_values, goal_nodes, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_NE(result.best_plan, nullptr);
    EXPECT_TRUE(result.constraints_satisfied);
}

TEST_F(PrefrontalQuantumBridgeTest, TreeSearchNoGoals) {
    uint8_t adjacency[] = {0, 1, 0, 0};
    float node_values[] = {0.5f, 0.7f};
    bool goal_nodes[] = {false, false};  // No goals

    quantum_planning_result_t result;
    int ret = prefrontal_quantum_tree_search(bridge, adjacency, 2, node_values, goal_nodes, &result);

    EXPECT_EQ(ret, -1);  // Should fail without goals
}

// ============================================================================
// QUANTUM CONFLICT RESOLUTION TESTS
// ============================================================================

TEST_F(PrefrontalQuantumBridgeTest, ResolveConflictBasic) {
    float goal_values[] = {0.8f, 0.6f, 0.9f};
    float goal_conflicts[] = {
        0.0f, 0.5f, 0.3f,
        0.5f, 0.0f, 0.2f,
        0.3f, 0.2f, 0.0f
    };

    quantum_conflict_result_t result;
    int ret = prefrontal_quantum_resolve_conflict(bridge, goal_values, goal_conflicts, 3, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.num_goals, 3u);
    EXPECT_NE(result.goal_weights, nullptr);
    EXPECT_NE(result.selected_priorities, nullptr);
    EXPECT_GT(result.resolution_quality, 0.0f);

    // Cleanup allocated memory
    if (result.goal_weights) free(result.goal_weights);
    if (result.selected_priorities) free(result.selected_priorities);
}

TEST_F(PrefrontalQuantumBridgeTest, ResolveConflictNullParams) {
    float goal_values[] = {0.5f, 0.6f};
    float conflicts[] = {0.0f, 0.1f, 0.1f, 0.0f};
    quantum_conflict_result_t result;

    EXPECT_EQ(prefrontal_quantum_resolve_conflict(NULL, goal_values, conflicts, 2, &result), -1);
    EXPECT_EQ(prefrontal_quantum_resolve_conflict(bridge, NULL, conflicts, 2, &result), -1);
    EXPECT_EQ(prefrontal_quantum_resolve_conflict(bridge, goal_values, NULL, 2, &result), -1);
    EXPECT_EQ(prefrontal_quantum_resolve_conflict(bridge, goal_values, conflicts, 2, NULL), -1);
}

TEST_F(PrefrontalQuantumBridgeTest, ResolveConflictAnnealingDisabled) {
    // Disable quantum annealing
    prefrontal_quantum_config_t cfg = prefrontal_quantum_default_config();
    cfg.enable_quantum_annealing = false;

    prefrontal_quantum_bridge_t* disabled_bridge = prefrontal_quantum_bridge_create(NULL, &cfg);
    ASSERT_NE(nullptr, disabled_bridge);

    float goal_values[] = {0.5f, 0.6f};
    float conflicts[] = {0.0f, 0.1f, 0.1f, 0.0f};
    quantum_conflict_result_t result;

    EXPECT_EQ(prefrontal_quantum_resolve_conflict(disabled_bridge, goal_values, conflicts, 2, &result), -1);

    prefrontal_quantum_bridge_destroy(disabled_bridge);
}

// ============================================================================
// PROBABILITY ESTIMATION TESTS
// ============================================================================

TEST_F(PrefrontalQuantumBridgeTest, EstimateProbabilityBasic) {
    float context[] = {0.5f, 0.3f, 0.8f};
    float probability;

    int ret = prefrontal_quantum_estimate_probability(bridge, 1, context, 3, &probability);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(probability, 0.0f);
    EXPECT_LE(probability, 1.0f);
}

TEST_F(PrefrontalQuantumBridgeTest, EstimateProbabilityNoContext) {
    float probability;
    int ret = prefrontal_quantum_estimate_probability(bridge, 1, NULL, 0, &probability);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(probability, 0.0f);
    EXPECT_LE(probability, 1.0f);
}

TEST_F(PrefrontalQuantumBridgeTest, EstimateProbabilityNullResult) {
    float context[] = {0.5f, 0.3f};
    EXPECT_EQ(prefrontal_quantum_estimate_probability(bridge, 1, context, 2, NULL), -1);
}

TEST_F(PrefrontalQuantumBridgeTest, EstimateProbabilityDisabled) {
    prefrontal_quantum_config_t cfg = prefrontal_quantum_default_config();
    cfg.use_amplitude_estimation = false;

    prefrontal_quantum_bridge_t* disabled_bridge = prefrontal_quantum_bridge_create(NULL, &cfg);
    ASSERT_NE(nullptr, disabled_bridge);

    float probability;
    EXPECT_EQ(prefrontal_quantum_estimate_probability(disabled_bridge, 1, NULL, 0, &probability), -1);

    prefrontal_quantum_bridge_destroy(disabled_bridge);
}

// ============================================================================
// STATISTICS TESTS
// ============================================================================

TEST_F(PrefrontalQuantumBridgeTest, GetStatsInitial) {
    prefrontal_quantum_stats_t stats;
    EXPECT_EQ(prefrontal_quantum_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.decisions_accelerated, 0u);
    EXPECT_EQ(stats.plans_optimized, 0u);
    EXPECT_EQ(stats.conflicts_resolved, 0u);
}

TEST_F(PrefrontalQuantumBridgeTest, GetStatsAfterOperations) {
    // Perform some operations
    float utilities[] = {0.5f, 0.7f, 0.9f};
    quantum_decision_result_t dec_result;
    prefrontal_quantum_accelerate_decision(bridge, utilities, 3, 0.6f, &dec_result);

    uint32_t actions[] = {1, 2, 3};
    float values[] = {0.5f, 0.6f, 0.7f};
    quantum_planning_result_t plan_result;
    prefrontal_quantum_optimize_plan(bridge, actions, 3, NULL, values, 3, &plan_result);

    prefrontal_quantum_stats_t stats;
    EXPECT_EQ(prefrontal_quantum_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.decisions_accelerated, 1u);
    EXPECT_EQ(stats.plans_optimized, 1u);
}

TEST_F(PrefrontalQuantumBridgeTest, GetStatsNull) {
    prefrontal_quantum_stats_t stats;
    EXPECT_EQ(prefrontal_quantum_get_stats(NULL, &stats), -1);
    EXPECT_EQ(prefrontal_quantum_get_stats(bridge, NULL), -1);
}

TEST_F(PrefrontalQuantumBridgeTest, ResetStats) {
    // First perform some operations
    float utilities[] = {0.5f, 0.7f};
    quantum_decision_result_t result;
    prefrontal_quantum_accelerate_decision(bridge, utilities, 2, 0.5f, &result);

    // Verify stats exist
    prefrontal_quantum_stats_t stats;
    EXPECT_EQ(prefrontal_quantum_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.decisions_accelerated, 0u);

    // Reset
    prefrontal_quantum_reset_stats(bridge);

    // Verify reset
    EXPECT_EQ(prefrontal_quantum_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.decisions_accelerated, 0u);
}

TEST_F(PrefrontalQuantumBridgeTest, ResetStatsNull) {
    prefrontal_quantum_reset_stats(NULL);  // Should not crash
}

// ============================================================================
// CONFIGURATION TESTS
// ============================================================================

TEST_F(PrefrontalQuantumBridgeTest, GetConfig) {
    prefrontal_quantum_config_t retrieved;
    EXPECT_EQ(prefrontal_quantum_get_config(bridge, &retrieved), 0);
    EXPECT_EQ(retrieved.max_decision_qubits, config.max_decision_qubits);
    EXPECT_EQ(retrieved.max_planning_qubits, config.max_planning_qubits);
}

TEST_F(PrefrontalQuantumBridgeTest, GetConfigNull) {
    prefrontal_quantum_config_t retrieved;
    EXPECT_EQ(prefrontal_quantum_get_config(NULL, &retrieved), -1);
    EXPECT_EQ(prefrontal_quantum_get_config(bridge, NULL), -1);
}

// ============================================================================
// RESOURCE CHECKING TESTS
// ============================================================================

TEST_F(PrefrontalQuantumBridgeTest, CheckResourcesEnabled) {
    uint32_t qubits;
    float coherence;

    bool available = prefrontal_quantum_check_resources(bridge, &qubits, &coherence);

    EXPECT_TRUE(available);
    EXPECT_GT(qubits, 0u);
    EXPECT_GE(coherence, 0.0f);
}

TEST_F(PrefrontalQuantumBridgeTest, CheckResourcesDisabled) {
    prefrontal_quantum_bridge_set_enabled(bridge, false);

    uint32_t qubits;
    float coherence;

    bool available = prefrontal_quantum_check_resources(bridge, &qubits, &coherence);

    EXPECT_FALSE(available);
}

TEST_F(PrefrontalQuantumBridgeTest, CheckResourcesNull) {
    EXPECT_FALSE(prefrontal_quantum_check_resources(NULL, NULL, NULL));
}

// ============================================================================
// SPEEDUP VERIFICATION TESTS
// ============================================================================

TEST_F(PrefrontalQuantumBridgeTest, SpeedupIncreaseWithSize) {
    // Larger search space should yield greater speedup
    float utilities_small[] = {0.5f, 0.7f, 0.3f, 0.8f};
    quantum_decision_result_t result_small;
    prefrontal_quantum_accelerate_decision(bridge, utilities_small, 4, 0.6f, &result_small);

    float utilities_large[] = {0.5f, 0.7f, 0.3f, 0.8f, 0.4f, 0.6f, 0.9f, 0.2f,
                               0.55f, 0.65f, 0.45f, 0.75f, 0.35f, 0.85f, 0.15f, 0.95f};
    quantum_decision_result_t result_large;
    prefrontal_quantum_accelerate_decision(bridge, utilities_large, 16, 0.6f, &result_large);

    // Speedup should be proportional to sqrt(N)
    // sqrt(16)/sqrt(4) = 2, so large should have higher speedup
    if (result_small.used_quantum && result_large.used_quantum) {
        EXPECT_GT(result_large.quantum_speedup, result_small.quantum_speedup);
    }
}

// ============================================================================
// INTEGRATION WORKFLOW TESTS
// ============================================================================

TEST_F(PrefrontalQuantumBridgeTest, CompleteDecisionPipeline) {
    // 1. Check resources
    uint32_t qubits;
    float coherence;
    ASSERT_TRUE(prefrontal_quantum_check_resources(bridge, &qubits, &coherence));

    // 2. Accelerate decision
    float utilities[] = {0.4f, 0.6f, 0.8f, 0.5f, 0.7f};
    quantum_decision_result_t dec_result;
    ASSERT_EQ(prefrontal_quantum_accelerate_decision(bridge, utilities, 5, 0.5f, &dec_result), 0);

    // 3. Estimate probability for selected option
    float context[] = {0.5f, 0.3f, 0.8f};
    float probability;
    ASSERT_EQ(prefrontal_quantum_estimate_probability(bridge, dec_result.best_candidate->option_id,
                                                       context, 3, &probability), 0);

    // 4. Verify stats updated
    prefrontal_quantum_stats_t stats;
    EXPECT_EQ(prefrontal_quantum_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.decisions_accelerated, 1u);
    EXPECT_GT(stats.total_coherence_used, 0.0f);
}

TEST_F(PrefrontalQuantumBridgeTest, CompletePlanningPipeline) {
    // 1. Define available actions
    uint32_t actions[] = {10, 20, 30, 40, 50};
    float values[] = {0.5f, 0.8f, 0.3f, 0.6f, 0.9f};

    // 2. Optimize plan
    quantum_planning_result_t plan_result;
    ASSERT_EQ(prefrontal_quantum_optimize_plan(bridge, actions, 5, NULL, values, 3, &plan_result), 0);
    ASSERT_NE(plan_result.best_plan, nullptr);

    // 3. Check plan quality
    EXPECT_GT(plan_result.optimization_quality, 0.0f);
    EXPECT_TRUE(plan_result.constraints_satisfied);

    // 4. Verify speedup achieved
    EXPECT_GT(plan_result.quantum_speedup, 1.0f);
}

TEST_F(PrefrontalQuantumBridgeTest, ConflictResolutionAndDecision) {
    // 1. Resolve goal conflicts
    float goal_values[] = {0.9f, 0.7f, 0.8f};
    float conflicts[] = {
        0.0f, 0.4f, 0.2f,
        0.4f, 0.0f, 0.3f,
        0.2f, 0.3f, 0.0f
    };

    quantum_conflict_result_t conflict_result;
    ASSERT_EQ(prefrontal_quantum_resolve_conflict(bridge, goal_values, conflicts, 3, &conflict_result), 0);

    // 2. Use priority ordering to inform decision
    float utilities[3];
    for (uint32_t i = 0; i < 3; i++) {
        utilities[i] = conflict_result.goal_weights[i];
    }

    quantum_decision_result_t dec_result;
    ASSERT_EQ(prefrontal_quantum_accelerate_decision(bridge, utilities, 3, 0.0f, &dec_result), 0);

    // Cleanup
    if (conflict_result.goal_weights) free(conflict_result.goal_weights);
    if (conflict_result.selected_priorities) free(conflict_result.selected_priorities);
}
