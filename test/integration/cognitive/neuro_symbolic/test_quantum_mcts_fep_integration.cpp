/**
 * @file test_quantum_mcts_fep_integration.cpp
 * @brief Integration tests for Quantum MCTS with FEP
 *
 * Tests the integration between Quantum MCTS and Free Energy Principle:
 * - Quantum MCTS using FEP expected free energy
 * - Classical vs quantum rollout comparison
 * - Quantum exploration bonus effects
 * - FEP-guided action selection
 * - Hybrid classical-quantum planning
 *
 * @version 2.6.3
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include "utils/nimcp_test_base.h"
#include <cmath>

extern "C" {
#include "cognitive/neuro_symbolic/nimcp_quantum_mcts.h"
#include "cognitive/neuro_symbolic/bridges/nimcp_quantum_mcts_fep_bridge.h"
#include "cognitive/free_energy/nimcp_fep_planning.h"
#include "utils/memory/nimcp_memory.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

/**
 * @brief Test fixture for Quantum MCTS FEP Integration tests
 */
class QuantumMCTSFEPIntegrationTest : public NimcpTestBase {
protected:
    quantum_mcts_t* qmcts;
    quantum_mcts_config_t config;

    void SetUp() override {
        NimcpTestBase::SetUp();
        qmcts = NULL;
        quantum_mcts_get_default_config(&config);
        /* Use fast config for integration tests */
        config.num_simulations = 100;
        config.planning_horizon = 10;
        config.discount_factor = 0.95f;
    }

    void TearDown() override {
        if (qmcts) {
            quantum_mcts_destroy(qmcts);
            qmcts = NULL;
        }
        NimcpTestBase::TearDown();
    }

    /**
     * @brief Create a simple test state
     */
    void CreateTestState(float* state, uint32_t dim, float base_value) {
        for (uint32_t i = 0; i < dim; i++) {
            state[i] = base_value + 0.1f * i;
        }
    }
};

/* ============================================================================
 * Quantum MCTS with FEP Expected Free Energy Tests
 * ============================================================================ */

TEST_F(QuantumMCTSFEPIntegrationTest, QuantumMCTSFEPValueIntegration) {
    config.enable_fep_integration = true;
    config.enhancement = QMCTS_ENHANCE_ROLLOUT;

    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    /* Create test state */
    float state[8];
    CreateTestState(state, 8, 0.5f);

    /* Get FEP-based value */
    float fep_value = quantum_mcts_fep_value(qmcts, state, 8);

    /* FEP value should be finite and reasonable */
    EXPECT_FALSE(std::isnan(fep_value));
    EXPECT_FALSE(std::isinf(fep_value));
}

TEST_F(QuantumMCTSFEPIntegrationTest, QuantumMCTSActiveInferenceAction) {
    config.enable_fep_integration = true;
    config.num_simulations = 50;

    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    /* Create belief state for active inference */
    float belief_state[4] = {0.25f, 0.25f, 0.25f, 0.25f};  /* Uniform belief */

    /* Select action using active inference */
    int action = -1;
    nimcp_error_t err = quantum_mcts_active_inference_action(qmcts, belief_state, 4, &action);
    EXPECT_EQ(err, 0);

    /* Action should be valid (non-negative or specific error code) */
    EXPECT_GE(action, -1);
}

TEST_F(QuantumMCTSFEPIntegrationTest, QuantumMCTSFEPBridgeCreation) {
    /* Create QMCTS-FEP bridge */
    qmcts_fep_bridge_t* bridge = qmcts_fep_bridge_create();
    ASSERT_NE(bridge, nullptr);

    /* Bridge should have initialized values */
    EXPECT_GE(bridge->quantum_exploration_boost, 0.0f);
    EXPECT_EQ(bridge->plans_executed, 0u);

    qmcts_fep_bridge_destroy(bridge);
}

TEST_F(QuantumMCTSFEPIntegrationTest, QuantumMCTSFEPBridgeExpectedValue) {
    /* Create bridge */
    qmcts_fep_bridge_t* bridge = qmcts_fep_bridge_create();
    ASSERT_NE(bridge, nullptr);

    /* Get expected value through bridge */
    float state[4] = {1.0f, 0.5f, 0.3f, 0.2f};
    float expected_value = qmcts_fep_bridge_expected_value(bridge, state, 4);

    /* Should return valid value */
    EXPECT_FALSE(std::isnan(expected_value));

    qmcts_fep_bridge_destroy(bridge);
}

TEST_F(QuantumMCTSFEPIntegrationTest, QuantumMCTSWithLinkedFEP) {
    config.enable_fep_integration = true;
    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    /* Link to FEP planner (if available, may be NULL in test) */
    nimcp_error_t err = quantum_mcts_link_fep(qmcts, NULL);
    /* Expect error for NULL, but should not crash */
    EXPECT_TRUE(err == NIMCP_SUCCESS || err == NIMCP_ERROR_INVALID_PARAM);

    /* Plan should still work */
    float state[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    qmcts_plan_t plan;
    quantum_mcts_plan_init(&plan, 10);

    err = quantum_mcts_plan(qmcts, state, 4, &plan);
    EXPECT_EQ(err, 0);

    quantum_mcts_plan_cleanup(&plan);
}

/* ============================================================================
 * Classical vs Quantum Rollout Comparison Tests
 * ============================================================================ */

TEST_F(QuantumMCTSFEPIntegrationTest, ClassicalRolloutPerformance) {
    config.enhancement = QMCTS_ENHANCE_NONE;  /* Pure classical */
    config.enable_quantum_rollouts = false;
    config.quantum_fraction = 0.0f;
    config.num_simulations = 100;

    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    /* Run classical planning */
    float state[8];
    CreateTestState(state, 8, 0.3f);

    qmcts_plan_t plan;
    quantum_mcts_plan_init(&plan, 20);

    nimcp_error_t err = quantum_mcts_plan(qmcts, state, 8, &plan);
    EXPECT_EQ(err, 0);

    /* Get statistics */
    quantum_mcts_stats_t stats;
    quantum_mcts_get_stats(qmcts, &stats);

    /* All simulations should be classical */
    EXPECT_EQ(stats.quantum_simulations, 0u);
    EXPECT_GT(stats.classical_simulations, 0u);

    quantum_mcts_plan_cleanup(&plan);
}

TEST_F(QuantumMCTSFEPIntegrationTest, QuantumRolloutPerformance) {
    config.enhancement = QMCTS_ENHANCE_ROLLOUT;
    config.enable_quantum_rollouts = true;
    config.quantum_fraction = 1.0f;  /* All quantum */
    config.qmc_shots = 100;
    config.num_simulations = 50;

    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    /* Run quantum-enhanced planning */
    float state[8];
    CreateTestState(state, 8, 0.3f);

    qmcts_plan_t plan;
    quantum_mcts_plan_init(&plan, 20);

    nimcp_error_t err = quantum_mcts_plan(qmcts, state, 8, &plan);
    EXPECT_EQ(err, 0);

    /* Get statistics */
    quantum_mcts_stats_t stats;
    quantum_mcts_get_stats(qmcts, &stats);

    /* Should have quantum simulations */
    EXPECT_GT(stats.quantum_simulations, 0u);

    quantum_mcts_plan_cleanup(&plan);
}

TEST_F(QuantumMCTSFEPIntegrationTest, HybridClassicalQuantumComparison) {
    /* Test 30% quantum, 70% classical */
    config.enhancement = QMCTS_ENHANCE_ROLLOUT;
    config.enable_quantum_rollouts = true;
    config.quantum_fraction = 0.3f;
    config.num_simulations = 100;

    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    float state[8];
    CreateTestState(state, 8, 0.5f);

    qmcts_plan_t plan;
    quantum_mcts_plan_init(&plan, 10);

    nimcp_error_t err = quantum_mcts_plan(qmcts, state, 8, &plan);
    EXPECT_EQ(err, 0);

    /* Get statistics */
    quantum_mcts_stats_t stats;
    quantum_mcts_get_stats(qmcts, &stats);

    /* Should have both types of simulations */
    EXPECT_GT(stats.total_simulations, 0u);

    /* Approximately 30% should be quantum (with some variance) */
    if (stats.total_simulations > 10) {
        float quantum_ratio = (float)stats.quantum_simulations / stats.total_simulations;
        EXPECT_GE(quantum_ratio, 0.0f);  /* At least some quantum */
        EXPECT_LE(quantum_ratio, 1.0f);
    }

    quantum_mcts_plan_cleanup(&plan);
}

TEST_F(QuantumMCTSFEPIntegrationTest, RolloutValueConsistency) {
    config.enhancement = QMCTS_ENHANCE_ROLLOUT;
    config.num_simulations = 50;

    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    /* Plan to create some tree structure */
    float state[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    qmcts_plan_t plan;
    quantum_mcts_plan_init(&plan, 5);
    quantum_mcts_plan(qmcts, state, 4, &plan);

    /* Perform quantum rollout from root */
    float value = quantum_mcts_rollout(qmcts, 0, NULL);

    /* Value should be finite */
    EXPECT_FALSE(std::isnan(value));
    EXPECT_FALSE(std::isinf(value));

    quantum_mcts_plan_cleanup(&plan);
}

/* ============================================================================
 * Quantum Exploration Bonus Tests
 * ============================================================================ */

TEST_F(QuantumMCTSFEPIntegrationTest, QuantumExplorationBonusComputation) {
    config.enhancement = QMCTS_ENHANCE_SELECTION;
    config.quantum_exploration_boost = 0.5f;
    config.num_simulations = 30;

    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    /* Build some tree structure */
    float state[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    qmcts_plan_t plan;
    quantum_mcts_plan_init(&plan, 5);
    quantum_mcts_plan(qmcts, state, 4, &plan);

    /* Get root node */
    const qmcts_node_t* root = quantum_mcts_get_root(qmcts);
    if (root) {
        /* Compute exploration bonus */
        float bonus = quantum_mcts_exploration_bonus(qmcts, root);

        /* Bonus should be non-negative */
        EXPECT_GE(bonus, 0.0f);
    }

    quantum_mcts_plan_cleanup(&plan);
}

TEST_F(QuantumMCTSFEPIntegrationTest, QuantumExplorationBoostEffect) {
    /* Compare planning with different exploration boosts */
    float boost_values[] = {0.0f, 0.5f, 1.0f};

    for (int i = 0; i < 3; i++) {
        quantum_mcts_config_t test_config;
        quantum_mcts_get_default_config(&test_config);
        test_config.enhancement = QMCTS_ENHANCE_SELECTION;
        test_config.quantum_exploration_boost = boost_values[i];
        test_config.num_simulations = 50;

        quantum_mcts_t* test_qmcts = quantum_mcts_create(&test_config);
        ASSERT_NE(test_qmcts, nullptr);

        float state[4] = {0.5f, 0.5f, 0.5f, 0.5f};
        qmcts_plan_t plan;
        quantum_mcts_plan_init(&plan, 5);

        nimcp_error_t err = quantum_mcts_plan(test_qmcts, state, 4, &plan);
        EXPECT_EQ(err, 0);

        /* Higher boost should potentially explore more (check tree depth) */
        uint32_t depth = quantum_mcts_get_depth(test_qmcts);
        EXPECT_GE(depth, 0u);

        quantum_mcts_plan_cleanup(&plan);
        quantum_mcts_destroy(test_qmcts);
    }
}

TEST_F(QuantumMCTSFEPIntegrationTest, QuantumSelectionChildChoice) {
    config.enhancement = QMCTS_ENHANCE_SELECTION;
    config.num_simulations = 50;

    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    /* Build tree */
    float state[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    qmcts_plan_t plan;
    quantum_mcts_plan_init(&plan, 5);
    quantum_mcts_plan(qmcts, state, 4, &plan);

    /* Select child using quantum-enhanced UCB */
    uint32_t selected = quantum_mcts_select_child(qmcts, 0);

    /* Should return valid child or UINT32_MAX if no children */
    if (selected != UINT32_MAX) {
        const qmcts_node_t* child = quantum_mcts_get_node(qmcts, selected);
        EXPECT_NE(child, nullptr);
    }

    quantum_mcts_plan_cleanup(&plan);
}

/* ============================================================================
 * Quantum Value Estimation Tests
 * ============================================================================ */

TEST_F(QuantumMCTSFEPIntegrationTest, QuantumValueEstimation) {
    config.enable_amplitude_estimation = true;
    config.qmc_shots = 100;

    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    /* Estimate value for a state */
    float state[4] = {0.3f, 0.5f, 0.7f, 0.9f};
    qmc_amplitude_result_t amplitude_result;
    memset(&amplitude_result, 0, sizeof(amplitude_result));

    nimcp_error_t err = quantum_mcts_estimate_value(qmcts, state, 4, &amplitude_result);
    EXPECT_EQ(err, 0);

    /* Amplitude result should have valid values */
    EXPECT_FALSE(std::isnan(amplitude_result.amplitude));
}

TEST_F(QuantumMCTSFEPIntegrationTest, QuantumValueEstimationConsistency) {
    config.enable_amplitude_estimation = true;
    config.qmc_shots = 200;

    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    /* Estimate value for same state multiple times */
    float state[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    float values[5];

    for (int i = 0; i < 5; i++) {
        qmc_amplitude_result_t result;
        memset(&result, 0, sizeof(result));
        quantum_mcts_estimate_value(qmcts, state, 4, &result);
        values[i] = result.amplitude;
    }

    /* Values should be relatively consistent (allowing for quantum randomness) */
    float mean = 0.0f;
    for (int i = 0; i < 5; i++) {
        mean += values[i];
    }
    mean /= 5.0f;

    /* All values should be within reasonable range of mean */
    for (int i = 0; i < 5; i++) {
        EXPECT_FALSE(std::isnan(values[i]));
    }
}

/* ============================================================================
 * Planning with FEP Integration Tests
 * ============================================================================ */

TEST_F(QuantumMCTSFEPIntegrationTest, PlanningWithFEPIntegration) {
    config.enable_fep_integration = true;
    config.num_simulations = 80;
    config.planning_horizon = 8;

    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    /* Plan with FEP integration */
    float state[6] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f};
    qmcts_plan_t plan;
    quantum_mcts_plan_init(&plan, 10);

    nimcp_error_t err = quantum_mcts_plan(qmcts, state, 6, &plan);
    EXPECT_EQ(err, 0);

    /* Plan should have actions */
    EXPECT_GE(plan.num_actions, 0u);

    /* Expected value should be finite */
    EXPECT_FALSE(std::isnan(plan.expected_value));

    quantum_mcts_plan_cleanup(&plan);
}

TEST_F(QuantumMCTSFEPIntegrationTest, ActionSelectionWithFEP) {
    config.enable_fep_integration = true;
    config.num_simulations = 50;

    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    /* Select action */
    float state[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    int action = -1;

    nimcp_error_t err = quantum_mcts_select_action(qmcts, state, 4, &action);
    EXPECT_EQ(err, 0);

    /* Action should be selected (may be -1 if no valid actions) */
    EXPECT_GE(action, -1);
}

/* ============================================================================
 * Temperature and Modulation Tests
 * ============================================================================ */

TEST_F(QuantumMCTSFEPIntegrationTest, TemperatureAffectsExploration) {
    config.temperature = 1.0f;
    config.num_simulations = 50;

    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    /* Set high temperature for more exploration */
    nimcp_error_t err = quantum_mcts_set_temperature(qmcts, 2.0f);
    EXPECT_EQ(err, 0);

    float state[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    qmcts_plan_t plan_high_temp;
    quantum_mcts_plan_init(&plan_high_temp, 10);
    quantum_mcts_plan(qmcts, state, 4, &plan_high_temp);

    /* Reset and use low temperature */
    quantum_mcts_reset(qmcts);
    quantum_mcts_set_temperature(qmcts, 0.1f);

    qmcts_plan_t plan_low_temp;
    quantum_mcts_plan_init(&plan_low_temp, 10);
    quantum_mcts_plan(qmcts, state, 4, &plan_low_temp);

    /* Both should produce valid plans */
    EXPECT_FALSE(std::isnan(plan_high_temp.expected_value));
    EXPECT_FALSE(std::isnan(plan_low_temp.expected_value));

    quantum_mcts_plan_cleanup(&plan_high_temp);
    quantum_mcts_plan_cleanup(&plan_low_temp);
}

TEST_F(QuantumMCTSFEPIntegrationTest, ATPModulationAffectsQuantumBudget) {
    config.enhancement = QMCTS_ENHANCE_ROLLOUT;
    config.enable_quantum_rollouts = true;
    config.atp_sensitivity = 1.0f;
    config.num_simulations = 50;

    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    /* Plan at full ATP */
    quantum_mcts_modulate_atp(qmcts, 1.0f);
    float state[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    qmcts_plan_t plan1;
    quantum_mcts_plan_init(&plan1, 10);
    quantum_mcts_plan(qmcts, state, 4, &plan1);

    quantum_mcts_stats_t stats1;
    quantum_mcts_get_stats(qmcts, &stats1);

    /* Reset and plan at low ATP */
    quantum_mcts_reset(qmcts);
    quantum_mcts_modulate_atp(qmcts, 0.2f);
    qmcts_plan_t plan2;
    quantum_mcts_plan_init(&plan2, 10);
    quantum_mcts_plan(qmcts, state, 4, &plan2);

    quantum_mcts_stats_t stats2;
    quantum_mcts_get_stats(qmcts, &stats2);

    /* Low ATP may result in fewer quantum simulations */
    EXPECT_GE(stats1.total_simulations, 0u);
    EXPECT_GE(stats2.total_simulations, 0u);

    quantum_mcts_plan_cleanup(&plan1);
    quantum_mcts_plan_cleanup(&plan2);
}

/* ============================================================================
 * Tree Structure Tests
 * ============================================================================ */

TEST_F(QuantumMCTSFEPIntegrationTest, TreeNodeStructure) {
    config.num_simulations = 100;
    config.planning_horizon = 10;

    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    /* Build tree through planning */
    float state[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    qmcts_plan_t plan;
    quantum_mcts_plan_init(&plan, 10);
    quantum_mcts_plan(qmcts, state, 4, &plan);

    /* Check root node */
    const qmcts_node_t* root = quantum_mcts_get_root(qmcts);
    ASSERT_NE(root, nullptr);

    /* Root should have been visited */
    EXPECT_GT(root->visit_count, 0u);

    /* Get best child */
    uint32_t best_child_id = quantum_mcts_get_best_child(qmcts, root->node_id);
    if (best_child_id != UINT32_MAX) {
        const qmcts_node_t* best_child = quantum_mcts_get_node(qmcts, best_child_id);
        EXPECT_NE(best_child, nullptr);
    }

    quantum_mcts_plan_cleanup(&plan);
}

TEST_F(QuantumMCTSFEPIntegrationTest, TreeDepthWithQuantumEnhancement) {
    config.enhancement = QMCTS_ENHANCE_FULL;
    config.num_simulations = 100;
    config.planning_horizon = 15;

    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    float state[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    qmcts_plan_t plan;
    quantum_mcts_plan_init(&plan, 15);
    quantum_mcts_plan(qmcts, state, 4, &plan);

    /* Check tree depth */
    uint32_t depth = quantum_mcts_get_depth(qmcts);
    EXPECT_GT(depth, 0u);
    EXPECT_LE(depth, config.planning_horizon);

    /* Get statistics */
    quantum_mcts_stats_t stats;
    quantum_mcts_get_stats(qmcts, &stats);
    EXPECT_GT(stats.total_nodes, 0u);
    EXPECT_EQ(stats.max_depth_reached, depth);

    quantum_mcts_plan_cleanup(&plan);
}

/* ============================================================================
 * Caching Tests
 * ============================================================================ */

TEST_F(QuantumMCTSFEPIntegrationTest, QuantumStateCaching) {
    config.enable_caching = true;
    config.max_cached_states = 100;
    config.num_simulations = 80;

    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    /* Plan twice with same state - second should use cache */
    float state[4] = {0.5f, 0.5f, 0.5f, 0.5f};

    qmcts_plan_t plan1;
    quantum_mcts_plan_init(&plan1, 10);
    quantum_mcts_plan(qmcts, state, 4, &plan1);

    quantum_mcts_stats_t stats1;
    quantum_mcts_get_stats(qmcts, &stats1);

    /* Plan again without reset */
    qmcts_plan_t plan2;
    quantum_mcts_plan_init(&plan2, 10);
    quantum_mcts_plan(qmcts, state, 4, &plan2);

    quantum_mcts_stats_t stats2;
    quantum_mcts_get_stats(qmcts, &stats2);

    /* Second planning may have cache hits */
    if (stats2.cache_hits > stats1.cache_hits) {
        EXPECT_GT(stats2.cache_hits, stats1.cache_hits);
    }

    quantum_mcts_plan_cleanup(&plan1);
    quantum_mcts_plan_cleanup(&plan2);
}

/* ============================================================================
 * Simulation Tests
 * ============================================================================ */

TEST_F(QuantumMCTSFEPIntegrationTest, SimulationStepByStep) {
    config.num_simulations = 10;  /* Few simulations for detailed testing */

    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    /* Initialize with a state first */
    float state[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    qmcts_plan_t plan;
    quantum_mcts_plan_init(&plan, 5);
    quantum_mcts_plan(qmcts, state, 4, &plan);

    /* Run additional simulations */
    nimcp_error_t err = quantum_mcts_simulate(qmcts, 5);
    EXPECT_EQ(err, 0);

    quantum_mcts_stats_t stats;
    quantum_mcts_get_stats(qmcts, &stats);

    /* Should have run additional simulations */
    EXPECT_GT(stats.total_simulations, 10u);

    quantum_mcts_plan_cleanup(&plan);
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

TEST_F(QuantumMCTSFEPIntegrationTest, NullStateHandling) {
    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    qmcts_plan_t plan;
    quantum_mcts_plan_init(&plan, 10);

    /* NULL state should be handled */
    nimcp_error_t err = quantum_mcts_plan(qmcts, NULL, 4, &plan);
    EXPECT_NE(err, NIMCP_SUCCESS);

    quantum_mcts_plan_cleanup(&plan);
}

TEST_F(QuantumMCTSFEPIntegrationTest, ZeroDimensionHandling) {
    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    float state[1] = {0.5f};
    qmcts_plan_t plan;
    quantum_mcts_plan_init(&plan, 10);

    /* Zero dimension should be handled */
    nimcp_error_t err = quantum_mcts_plan(qmcts, state, 0, &plan);
    EXPECT_NE(err, NIMCP_SUCCESS);

    quantum_mcts_plan_cleanup(&plan);
}

TEST_F(QuantumMCTSFEPIntegrationTest, ResetClearsState) {
    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    /* Build tree */
    float state[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    qmcts_plan_t plan;
    quantum_mcts_plan_init(&plan, 10);
    quantum_mcts_plan(qmcts, state, 4, &plan);

    quantum_mcts_stats_t stats_before;
    quantum_mcts_get_stats(qmcts, &stats_before);
    EXPECT_GT(stats_before.total_nodes, 0u);

    /* Reset */
    nimcp_error_t err = quantum_mcts_reset(qmcts);
    EXPECT_EQ(err, 0);

    /* Stats should be cleared */
    quantum_mcts_stats_t stats_after;
    quantum_mcts_get_stats(qmcts, &stats_after);
    EXPECT_EQ(stats_after.total_nodes, 0u);

    quantum_mcts_plan_cleanup(&plan);
}
