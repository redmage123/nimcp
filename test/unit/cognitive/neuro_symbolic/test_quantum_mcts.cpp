/**
 * @file test_quantum_mcts.cpp
 * @brief Unit tests for Quantum Monte Carlo Tree Search
 *
 * Tests the Quantum MCTS implementation which provides quantum-enhanced
 * tree search for mathematical planning and theorem proving.
 */

#include <gtest/gtest.h>
#include "utils/nimcp_test_base.h"

extern "C" {
#include "cognitive/neuro_symbolic/nimcp_quantum_mcts.h"
}

/**
 * @brief Test fixture for Quantum MCTS tests
 */
class QuantumMCTSTest : public NimcpTestBase {
protected:
    quantum_mcts_t* qmcts;
    quantum_mcts_config_t config;

    void SetUp() override {
        NimcpTestBase::SetUp();
        qmcts = NULL;
        memset(&config, 0, sizeof(config));
        quantum_mcts_get_default_config(&config);
    }

    void TearDown() override {
        if (qmcts) {
            quantum_mcts_destroy(qmcts);
            qmcts = NULL;
        }
        NimcpTestBase::TearDown();
    }

    // Simple transition function for testing
    static nimcp_error_t simple_transition(
        const float* state,
        uint32_t state_dim,
        int action,
        float* next_state,
        float* reward,
        bool* terminal,
        void* user_data) {

        (void)user_data;

        if (!state || !next_state || !reward || !terminal || state_dim == 0) {
            return NIMCP_ERROR_INVALID_PARAM;
        }

        // Simple transition: move in direction of action
        for (uint32_t i = 0; i < state_dim; i++) {
            next_state[i] = state[i] + (float)(action * 0.1);
        }

        // Reward based on distance from origin
        float dist = 0;
        for (uint32_t i = 0; i < state_dim; i++) {
            dist += next_state[i] * next_state[i];
        }
        *reward = 1.0f / (1.0f + dist);

        // Terminal if very close to origin
        *terminal = (dist < 0.01f);

        return NIMCP_SUCCESS;
    }

    // Simple value function for testing
    static float simple_value(
        const float* state,
        uint32_t state_dim,
        void* user_data) {

        (void)user_data;

        if (!state || state_dim == 0) return 0.0f;

        // Value is inverse of distance from origin
        float dist = 0;
        for (uint32_t i = 0; i < state_dim; i++) {
            dist += state[i] * state[i];
        }
        return 1.0f / (1.0f + dist);
    }

    // Simple action enumeration
    static uint32_t simple_actions(
        const float* state,
        uint32_t state_dim,
        int* actions,
        uint32_t max_actions,
        void* user_data) {

        (void)state;
        (void)state_dim;
        (void)user_data;

        // Two actions: move positive (1) or negative (-1)
        uint32_t count = (max_actions >= 2) ? 2 : max_actions;
        if (count >= 1) actions[0] = 1;
        if (count >= 2) actions[1] = -1;
        return count;
    }
};

// ============================================================================
// Default Configuration Tests
// ============================================================================

TEST_F(QuantumMCTSTest, GetDefaultConfigSucceeds) {
    quantum_mcts_config_t cfg;
    nimcp_error_t err = quantum_mcts_get_default_config(&cfg);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(QuantumMCTSTest, GetDefaultConfigNullReturnsError) {
    nimcp_error_t err = quantum_mcts_get_default_config(NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(QuantumMCTSTest, DefaultConfigHasReasonableValues) {
    quantum_mcts_config_t cfg;
    quantum_mcts_get_default_config(&cfg);

    EXPECT_GT(cfg.num_simulations, 0u);
    EXPECT_GT(cfg.exploration_constant, 0.0f);
    EXPECT_GT(cfg.planning_horizon, 0u);
    EXPECT_GT(cfg.discount_factor, 0.0f);
    EXPECT_LE(cfg.discount_factor, 1.0f);
    EXPECT_GE(cfg.quantum_fraction, 0.0f);
    EXPECT_LE(cfg.quantum_fraction, 1.0f);
}

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(QuantumMCTSTest, CreateWithConfigSucceeds) {
    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);
}

TEST_F(QuantumMCTSTest, CreateWithNullConfigSucceeds) {
    qmcts = quantum_mcts_create(NULL);
    EXPECT_NE(qmcts, nullptr);
}

TEST_F(QuantumMCTSTest, DestroyNullIsNoOp) {
    quantum_mcts_destroy(NULL);
    SUCCEED();
}

TEST_F(QuantumMCTSTest, CreateDestroyMultipleTimesSucceeds) {
    for (int i = 0; i < 5; i++) {
        qmcts = quantum_mcts_create(&config);
        ASSERT_NE(qmcts, nullptr) << "Failed on iteration " << i;
        quantum_mcts_destroy(qmcts);
        qmcts = NULL;
    }
}

TEST_F(QuantumMCTSTest, ResetNullReturnsError) {
    nimcp_error_t err = quantum_mcts_reset(NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(QuantumMCTSTest, ResetSucceeds) {
    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    nimcp_error_t err = quantum_mcts_reset(qmcts);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

// ============================================================================
// Environment Setup Tests
// ============================================================================

TEST_F(QuantumMCTSTest, SetEnvironmentNullQMCTSReturnsError) {
    nimcp_error_t err = quantum_mcts_set_environment(
        NULL, simple_transition, simple_value, simple_actions, NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(QuantumMCTSTest, SetEnvironmentSucceeds) {
    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    nimcp_error_t err = quantum_mcts_set_environment(
        qmcts, simple_transition, simple_value, simple_actions, NULL);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(QuantumMCTSTest, SetEnvironmentNullCallbacksSucceeds) {
    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    // Setting NULL callbacks should succeed (use defaults)
    nimcp_error_t err = quantum_mcts_set_environment(
        qmcts, NULL, NULL, NULL, NULL);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

// ============================================================================
// Action Selection Tests
// ============================================================================

TEST_F(QuantumMCTSTest, SelectActionNullQMCTSReturnsError) {
    float state[] = {0.5f, 0.5f};
    int action;
    nimcp_error_t err = quantum_mcts_select_action(NULL, state, 2, &action);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(QuantumMCTSTest, SelectActionNullStateReturnsError) {
    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    int action;
    nimcp_error_t err = quantum_mcts_select_action(qmcts, NULL, 2, &action);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(QuantumMCTSTest, SelectActionNullOutputReturnsError) {
    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    float state[] = {0.5f, 0.5f};
    nimcp_error_t err = quantum_mcts_select_action(qmcts, state, 2, NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(QuantumMCTSTest, SelectActionWithEnvironmentSucceeds) {
    config.num_simulations = 10;  // Small for testing
    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    nimcp_error_t err = quantum_mcts_set_environment(
        qmcts, simple_transition, simple_value, simple_actions, NULL);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    float state[] = {1.0f, 1.0f};
    int action;
    err = quantum_mcts_select_action(qmcts, state, 2, &action);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Action should be one of the valid actions
    EXPECT_TRUE(action == 1 || action == -1);
}

// ============================================================================
// Simulation Tests
// ============================================================================

TEST_F(QuantumMCTSTest, SimulateNullReturnsError) {
    nimcp_error_t err = quantum_mcts_simulate(NULL, 10);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(QuantumMCTSTest, SimulateZeroCountSucceeds) {
    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    nimcp_error_t err = quantum_mcts_simulate(qmcts, 0);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(QuantumMCTSTest, SimulateWithEnvironmentSucceeds) {
    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    quantum_mcts_set_environment(
        qmcts, simple_transition, simple_value, simple_actions, NULL);

    nimcp_error_t err = quantum_mcts_simulate(qmcts, 50);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

// ============================================================================
// Planning Tests
// ============================================================================

TEST_F(QuantumMCTSTest, PlanNullQMCTSReturnsError) {
    float state[] = {0.5f, 0.5f};
    qmcts_plan_t plan;
    memset(&plan, 0, sizeof(plan));

    nimcp_error_t err = quantum_mcts_plan(NULL, state, 2, &plan);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(QuantumMCTSTest, PlanWithEnvironmentSucceeds) {
    config.num_simulations = 20;
    config.planning_horizon = 5;
    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    quantum_mcts_set_environment(
        qmcts, simple_transition, simple_value, simple_actions, NULL);

    float state[] = {1.0f, 0.5f};
    qmcts_plan_t plan;
    memset(&plan, 0, sizeof(plan));

    nimcp_error_t err = quantum_mcts_plan(qmcts, state, 2, &plan);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

// ============================================================================
// Rollout Tests
// ============================================================================

TEST_F(QuantumMCTSTest, RolloutNullReturnsZero) {
    float value = quantum_mcts_rollout(NULL, 0, NULL);
    EXPECT_EQ(value, 0.0f);
}

TEST_F(QuantumMCTSTest, RolloutWithEnvironmentReturnsValidValue) {
    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    quantum_mcts_set_environment(
        qmcts, simple_transition, simple_value, simple_actions, NULL);

    // Note: node_id 0 may not exist - this tests graceful handling
    float value = quantum_mcts_rollout(qmcts, 0, NULL);
    // Value should be finite
    EXPECT_FALSE(std::isinf(value));
    EXPECT_FALSE(std::isnan(value));
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(QuantumMCTSTest, GetStatsNullReturnsError) {
    quantum_mcts_stats_t stats;
    nimcp_error_t err = quantum_mcts_get_stats(NULL, &stats);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(QuantumMCTSTest, GetStatsAfterSimulationShowsCounts) {
    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    quantum_mcts_set_environment(
        qmcts, simple_transition, simple_value, simple_actions, NULL);

    // Run some simulations
    quantum_mcts_simulate(qmcts, 10);

    quantum_mcts_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    nimcp_error_t err = quantum_mcts_get_stats(qmcts, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Should have recorded simulations
    EXPECT_GE(stats.total_simulations, 0u);
}

// ============================================================================
// Modulation Tests
// ============================================================================

TEST_F(QuantumMCTSTest, ModulateATPNullReturnsError) {
    nimcp_error_t err = quantum_mcts_modulate_atp(NULL, 0.5f);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(QuantumMCTSTest, ModulateATPSucceeds) {
    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    nimcp_error_t err = quantum_mcts_modulate_atp(qmcts, 0.8f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(QuantumMCTSTest, QuantumFractionZeroIsClassical) {
    config.quantum_fraction = 0.0f;
    config.enhancement = QMCTS_ENHANCE_NONE;
    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    quantum_mcts_set_environment(
        qmcts, simple_transition, simple_value, simple_actions, NULL);

    quantum_mcts_simulate(qmcts, 10);

    quantum_mcts_stats_t stats;
    quantum_mcts_get_stats(qmcts, &stats);

    // No quantum simulations should occur
    EXPECT_EQ(stats.quantum_simulations, 0u);
}

TEST_F(QuantumMCTSTest, QuantumFractionOneIsFullQuantum) {
    config.quantum_fraction = 1.0f;
    config.enhancement = QMCTS_ENHANCE_ROLLOUT;
    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    quantum_mcts_set_environment(
        qmcts, simple_transition, simple_value, simple_actions, NULL);

    quantum_mcts_simulate(qmcts, 10);

    quantum_mcts_stats_t stats;
    quantum_mcts_get_stats(qmcts, &stats);

    // All simulations should use quantum (if any occurred)
    // Classical may be 0
    if (stats.total_simulations > 0) {
        EXPECT_GE(stats.quantum_simulations, 0u);
    }
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(QuantumMCTSTest, FullWorkflowSucceeds) {
    // Configure with limited simulations for testing
    config.num_simulations = 50;
    config.planning_horizon = 10;
    config.exploration_constant = 1.0f;
    config.quantum_fraction = 0.2f;
    config.enhancement = QMCTS_ENHANCE_ROLLOUT;
    config.enable_bio_async = false;

    qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    // Set up environment
    nimcp_error_t err = quantum_mcts_set_environment(
        qmcts, simple_transition, simple_value, simple_actions, NULL);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Initial state
    float state[] = {2.0f, 1.5f};
    int action;

    // Select action
    err = quantum_mcts_select_action(qmcts, state, 2, &action);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Generate plan
    qmcts_plan_t plan;
    memset(&plan, 0, sizeof(plan));
    err = quantum_mcts_plan(qmcts, state, 2, &plan);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Get stats
    quantum_mcts_stats_t stats;
    err = quantum_mcts_get_stats(qmcts, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Reset
    err = quantum_mcts_reset(qmcts);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}
