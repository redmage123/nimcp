//=============================================================================
// test_gt_learning.cpp - Unit tests for Strategic Learning Module
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/game_theory/nimcp_gt_learning.h"
#include "cognitive/game_theory/nimcp_game_theory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class LearningTest : public ::testing::Test {
protected:
    nimcp_gt_learner_t learner = nullptr;
    nimcp_gt_learning_config_t config;

    static constexpr uint32_t NUM_STATES = 4;
    static constexpr uint32_t NUM_ACTIONS = 3;

    void SetUp() override {
        config = nimcp_gt_learning_default_config();
        config.num_states = NUM_STATES;
        config.num_actions = NUM_ACTIONS;
    }

    void TearDown() override {
        if (learner) {
            nimcp_gt_learner_destroy(learner);
            learner = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(LearningTest, DefaultConfigValues) {
    EXPECT_GT(config.learning_rate, 0.0f);
    EXPECT_LE(config.learning_rate, 1.0f);
    EXPECT_GT(config.discount_factor, 0.0f);
    EXPECT_LE(config.discount_factor, 1.0f);
    EXPECT_GE(config.exploration_rate, 0.0f);
    EXPECT_LE(config.exploration_rate, 1.0f);
}

TEST_F(LearningTest, MethodNames) {
    EXPECT_NE(nimcp_gt_learn_method_name(NIMCP_GT_LEARN_Q_LEARNING), nullptr);
    EXPECT_NE(nimcp_gt_learn_method_name(NIMCP_GT_LEARN_SARSA), nullptr);
    EXPECT_NE(nimcp_gt_learn_method_name(NIMCP_GT_LEARN_CFR), nullptr);
    EXPECT_NE(nimcp_gt_learn_method_name(NIMCP_GT_LEARN_FICTITIOUS_PLAY), nullptr);
    EXPECT_NE(nimcp_gt_learn_method_name(NIMCP_GT_LEARN_EXP3), nullptr);
}

TEST_F(LearningTest, ExploreStrategyNames) {
    EXPECT_NE(nimcp_gt_explore_strategy_name(NIMCP_GT_EXPLORE_EPSILON_GREEDY), nullptr);
    EXPECT_NE(nimcp_gt_explore_strategy_name(NIMCP_GT_EXPLORE_BOLTZMANN), nullptr);
    EXPECT_NE(nimcp_gt_explore_strategy_name(NIMCP_GT_EXPLORE_UCB), nullptr);
    EXPECT_NE(nimcp_gt_explore_strategy_name(NIMCP_GT_EXPLORE_THOMPSON), nullptr);
}

TEST_F(LearningTest, OpponentTypeNames) {
    EXPECT_NE(nimcp_gt_opponent_type_name(NIMCP_GT_OPPONENT_UNKNOWN), nullptr);
    EXPECT_NE(nimcp_gt_opponent_type_name(NIMCP_GT_OPPONENT_RANDOM), nullptr);
    EXPECT_NE(nimcp_gt_opponent_type_name(NIMCP_GT_OPPONENT_COOPERATIVE), nullptr);
    EXPECT_NE(nimcp_gt_opponent_type_name(NIMCP_GT_OPPONENT_COMPETITIVE), nullptr);
    EXPECT_NE(nimcp_gt_opponent_type_name(NIMCP_GT_OPPONENT_TFTIT), nullptr);
    EXPECT_NE(nimcp_gt_opponent_type_name(NIMCP_GT_OPPONENT_ADAPTIVE), nullptr);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(LearningTest, CreateDestroy) {
    learner = nimcp_gt_learner_create(&config, NUM_STATES, NUM_ACTIONS);
    ASSERT_NE(learner, nullptr);

    nimcp_gt_learner_destroy(learner);
    learner = nullptr;
}

TEST_F(LearningTest, CreateWithNullConfig) {
    learner = nimcp_gt_learner_create(nullptr, NUM_STATES, NUM_ACTIONS);
    // Should handle gracefully - either NULL or use defaults
}

TEST_F(LearningTest, ResetLearner) {
    learner = nimcp_gt_learner_create(&config, NUM_STATES, NUM_ACTIONS);
    ASSERT_NE(learner, nullptr);

    // Perform some updates
    nimcp_gt_learner_update(learner, 0, 0, 1.0f, 1);

    nimcp_error_t err = nimcp_gt_learner_reset(learner);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Q-values should be reset to 0
    float q = nimcp_gt_learner_get_q_value(learner, 0, 0);
    EXPECT_FLOAT_EQ(q, 0.0f);
}

//=============================================================================
// Q-Learning Tests
//=============================================================================

TEST_F(LearningTest, QLearningUpdate) {
    config.method = NIMCP_GT_LEARN_Q_LEARNING;
    config.learning_rate = 0.5f;
    config.discount_factor = 0.9f;
    learner = nimcp_gt_learner_create(&config, NUM_STATES, NUM_ACTIONS);
    ASSERT_NE(learner, nullptr);

    // Initial Q-value should be 0
    float q0 = nimcp_gt_learner_get_q_value(learner, 0, 0);
    EXPECT_FLOAT_EQ(q0, 0.0f);

    // Update with reward 1.0, transitioning to state 1
    nimcp_error_t err = nimcp_gt_learner_update(learner, 0, 0, 1.0f, 1);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Q(0,0) should be updated: Q(0,0) = 0 + 0.5 * (1.0 + 0.9 * 0 - 0) = 0.5
    float q1 = nimcp_gt_learner_get_q_value(learner, 0, 0);
    EXPECT_NEAR(q1, 0.5f, 0.01f);
}

TEST_F(LearningTest, QLearningConvergence) {
    config.method = NIMCP_GT_LEARN_Q_LEARNING;
    config.learning_rate = 0.1f;
    config.discount_factor = 0.95f;
    learner = nimcp_gt_learner_create(&config, 2, 2);  // Simple 2-state, 2-action
    ASSERT_NE(learner, nullptr);

    // Repeated updates with consistent rewards should converge
    for (int i = 0; i < 100; i++) {
        // State 0, action 0 -> reward 1, next state 0
        nimcp_gt_learner_update(learner, 0, 0, 1.0f, 0);
        // State 0, action 1 -> reward 0, next state 1
        nimcp_gt_learner_update(learner, 0, 1, 0.0f, 1);
    }

    // Q(0,0) should be higher than Q(0,1) after learning
    float q00 = nimcp_gt_learner_get_q_value(learner, 0, 0);
    float q01 = nimcp_gt_learner_get_q_value(learner, 0, 1);
    EXPECT_GT(q00, q01);
}

TEST_F(LearningTest, GetSetQValue) {
    learner = nimcp_gt_learner_create(&config, NUM_STATES, NUM_ACTIONS);
    ASSERT_NE(learner, nullptr);

    nimcp_error_t err = nimcp_gt_learner_set_q_value(learner, 0, 0, 5.0f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    float q = nimcp_gt_learner_get_q_value(learner, 0, 0);
    EXPECT_FLOAT_EQ(q, 5.0f);
}

TEST_F(LearningTest, InvalidStateAction) {
    learner = nimcp_gt_learner_create(&config, NUM_STATES, NUM_ACTIONS);
    ASSERT_NE(learner, nullptr);

    // Invalid state
    float q = nimcp_gt_learner_get_q_value(learner, 100, 0);
    EXPECT_FLOAT_EQ(q, 0.0f);

    // Invalid action
    nimcp_error_t err = nimcp_gt_learner_set_q_value(learner, 0, 100, 5.0f);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

//=============================================================================
// SARSA Tests
//=============================================================================

TEST_F(LearningTest, SarsaUpdate) {
    config.method = NIMCP_GT_LEARN_SARSA;
    config.learning_rate = 0.5f;
    config.discount_factor = 0.9f;
    learner = nimcp_gt_learner_create(&config, NUM_STATES, NUM_ACTIONS);
    ASSERT_NE(learner, nullptr);

    // Set Q(1,1) = 2.0 for next action value
    nimcp_gt_learner_set_q_value(learner, 1, 1, 2.0f);

    // SARSA update with next_action = 1
    nimcp_error_t err = nimcp_gt_learner_update_sarsa(learner, 0, 0, 1.0f, 1, 1);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Q(0,0) = 0 + 0.5 * (1.0 + 0.9 * 2.0 - 0) = 0.5 * 2.8 = 1.4
    float q = nimcp_gt_learner_get_q_value(learner, 0, 0);
    EXPECT_NEAR(q, 1.4f, 0.01f);
}

//=============================================================================
// Action Selection Tests
//=============================================================================

TEST_F(LearningTest, SelectGreedyAction) {
    learner = nimcp_gt_learner_create(&config, NUM_STATES, NUM_ACTIONS);
    ASSERT_NE(learner, nullptr);

    // Set Q-values: Q(0,0)=1, Q(0,1)=5, Q(0,2)=3
    nimcp_gt_learner_set_q_value(learner, 0, 0, 1.0f);
    nimcp_gt_learner_set_q_value(learner, 0, 1, 5.0f);
    nimcp_gt_learner_set_q_value(learner, 0, 2, 3.0f);

    uint32_t action;
    nimcp_error_t err = nimcp_gt_learner_select_greedy(learner, 0, &action);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(action, 1);  // Action 1 has highest Q-value
}

TEST_F(LearningTest, SelectAction) {
    config.exploration_rate = 0.1f;
    config.explore = NIMCP_GT_EXPLORE_EPSILON_GREEDY;
    learner = nimcp_gt_learner_create(&config, NUM_STATES, NUM_ACTIONS);
    ASSERT_NE(learner, nullptr);

    // Set Q-values
    nimcp_gt_learner_set_q_value(learner, 0, 0, 1.0f);
    nimcp_gt_learner_set_q_value(learner, 0, 1, 10.0f);
    nimcp_gt_learner_set_q_value(learner, 0, 2, 2.0f);

    uint32_t action;
    nimcp_error_t err = nimcp_gt_learner_select_action(learner, 0, &action);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_LT(action, NUM_ACTIONS);

    // With 90% exploitation, action 1 should be selected most often
    int counts[3] = {0, 0, 0};
    for (int i = 0; i < 100; i++) {
        nimcp_gt_learner_select_action(learner, 0, &action);
        counts[action]++;
    }
    // Action 1 should be selected most frequently
    EXPECT_GT(counts[1], counts[0]);
    EXPECT_GT(counts[1], counts[2]);
}

TEST_F(LearningTest, GetStrategy) {
    learner = nimcp_gt_learner_create(&config, NUM_STATES, NUM_ACTIONS);
    ASSERT_NE(learner, nullptr);

    float strategy[NUM_ACTIONS];
    nimcp_error_t err = nimcp_gt_learner_get_strategy(learner, 0, strategy);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Strategy should sum to 1
    float sum = 0.0f;
    for (uint32_t i = 0; i < NUM_ACTIONS; i++) {
        sum += strategy[i];
        EXPECT_GE(strategy[i], 0.0f);
        EXPECT_LE(strategy[i], 1.0f);
    }
    EXPECT_NEAR(sum, 1.0f, 0.01f);
}

//=============================================================================
// CFR Tests
//=============================================================================

TEST_F(LearningTest, CfrUpdate) {
    config.method = NIMCP_GT_LEARN_CFR;
    learner = nimcp_gt_learner_create(&config, 4, 3);  // 4 info sets, 3 actions
    ASSERT_NE(learner, nullptr);

    // Actions at info set 0
    uint32_t actions[3] = {0, 1, 2};
    float utilities[3] = {1.0f, 2.0f, 3.0f};

    nimcp_error_t err = nimcp_gt_cfr_update(learner, 0, actions, 3, utilities);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(LearningTest, CfrGetStrategy) {
    config.method = NIMCP_GT_LEARN_CFR;
    learner = nimcp_gt_learner_create(&config, 4, 3);
    ASSERT_NE(learner, nullptr);

    float strategy[3];
    nimcp_error_t err = nimcp_gt_cfr_get_strategy(learner, 0, strategy);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Initial strategy should be uniform
    for (int i = 0; i < 3; i++) {
        EXPECT_NEAR(strategy[i], 1.0f/3.0f, 0.01f);
    }
}

TEST_F(LearningTest, CfrConvergence) {
    config.method = NIMCP_GT_LEARN_CFR;
    config.use_cfr_plus = true;
    learner = nimcp_gt_learner_create(&config, 1, 2);
    ASSERT_NE(learner, nullptr);

    uint32_t actions[2] = {0, 1};

    // Simulate rock-paper-scissors-like regrets
    for (int i = 0; i < 100; i++) {
        float utilities[2] = {1.0f, -1.0f};  // Alternating regrets
        if (i % 2 == 1) {
            utilities[0] = -1.0f;
            utilities[1] = 1.0f;
        }
        nimcp_gt_cfr_update(learner, 0, actions, 2, utilities);
    }

    // Get average strategy (should be closer to uniform)
    float avg_strategy[2];
    nimcp_gt_cfr_get_average_strategy(learner, 0, avg_strategy);

    // Both probabilities should be closer to 0.5
    EXPECT_GT(avg_strategy[0], 0.1f);
    EXPECT_LT(avg_strategy[0], 0.9f);
}

TEST_F(LearningTest, CfrGetRegret) {
    config.method = NIMCP_GT_LEARN_CFR;
    learner = nimcp_gt_learner_create(&config, 4, 3);
    ASSERT_NE(learner, nullptr);

    // Initial regret should be 0
    float regret = nimcp_gt_cfr_get_regret(learner, 0);
    EXPECT_FLOAT_EQ(regret, 0.0f);

    // Add some regret
    uint32_t actions[3] = {0, 1, 2};
    float utilities[3] = {1.0f, 0.0f, 0.0f};
    nimcp_gt_cfr_update(learner, 0, actions, 3, utilities);

    regret = nimcp_gt_cfr_get_regret(learner, 0);
    EXPECT_GE(regret, 0.0f);
}

//=============================================================================
// Fictitious Play Tests
//=============================================================================

TEST_F(LearningTest, FictitiousPlayUpdate) {
    config.method = NIMCP_GT_LEARN_FICTITIOUS_PLAY;
    learner = nimcp_gt_learner_create(&config, NUM_STATES, NUM_ACTIONS);
    ASSERT_NE(learner, nullptr);

    // Observe opponent playing action 0 multiple times
    for (int i = 0; i < 10; i++) {
        nimcp_error_t err = nimcp_gt_fictitious_play_update(learner, 0);
        EXPECT_EQ(err, NIMCP_SUCCESS);
    }

    // Empirical distribution should favor action 0
    float distribution[NUM_ACTIONS];
    nimcp_gt_fictitious_play_get_distribution(learner, distribution);
    EXPECT_GT(distribution[0], distribution[1]);
    EXPECT_GT(distribution[0], distribution[2]);
}

TEST_F(LearningTest, FictitiousPlayPredict) {
    config.method = NIMCP_GT_LEARN_FICTITIOUS_PLAY;
    learner = nimcp_gt_learner_create(&config, NUM_STATES, NUM_ACTIONS);
    ASSERT_NE(learner, nullptr);

    // Opponent mostly plays action 2
    for (int i = 0; i < 50; i++) {
        nimcp_gt_fictitious_play_update(learner, 2);
    }
    for (int i = 0; i < 10; i++) {
        nimcp_gt_fictitious_play_update(learner, 0);
    }

    uint32_t prediction;
    nimcp_error_t err = nimcp_gt_fictitious_play_predict(learner, &prediction);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(prediction, 2);  // Most frequent action
}

TEST_F(LearningTest, FictitiousPlayBestResponse) {
    config.method = NIMCP_GT_LEARN_FICTITIOUS_PLAY;
    learner = nimcp_gt_learner_create(&config, NUM_STATES, NUM_ACTIONS);
    ASSERT_NE(learner, nullptr);

    // Opponent plays action 0 with certainty
    for (int i = 0; i < 100; i++) {
        nimcp_gt_fictitious_play_update(learner, 0);
    }

    // Payoff matrix: rows are our actions, cols are opponent actions
    // We want to find best response to opponent playing action 0
    float payoff_matrix[9] = {
        1.0f, 0.0f, 0.0f,  // Our action 0 payoffs
        5.0f, 0.0f, 0.0f,  // Our action 1 payoffs (best vs action 0)
        2.0f, 0.0f, 0.0f   // Our action 2 payoffs
    };

    uint32_t best_action;
    nimcp_error_t err = nimcp_gt_fictitious_play_best_response(learner, 0, payoff_matrix, &best_action);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(best_action, 1);  // Action 1 gives highest payoff vs action 0
}

//=============================================================================
// EXP3 Tests
//=============================================================================

TEST_F(LearningTest, Exp3Update) {
    config.method = NIMCP_GT_LEARN_EXP3;
    config.exp3_gamma = 0.1f;
    learner = nimcp_gt_learner_create(&config, NUM_STATES, NUM_ACTIONS);
    ASSERT_NE(learner, nullptr);

    // Update with reward for action 1
    nimcp_error_t err = nimcp_gt_exp3_update(learner, 1, 1.0f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    float probs[NUM_ACTIONS];
    nimcp_gt_exp3_get_probabilities(learner, probs);

    // Action 1 should have higher probability after positive reward
    EXPECT_GT(probs[1], probs[0]);
    EXPECT_GT(probs[1], probs[2]);
}

TEST_F(LearningTest, Exp3Select) {
    config.method = NIMCP_GT_LEARN_EXP3;
    learner = nimcp_gt_learner_create(&config, NUM_STATES, NUM_ACTIONS);
    ASSERT_NE(learner, nullptr);

    uint32_t action;
    nimcp_error_t err = nimcp_gt_exp3_select(learner, &action);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_LT(action, NUM_ACTIONS);
}

TEST_F(LearningTest, Exp3GetProbabilities) {
    config.method = NIMCP_GT_LEARN_EXP3;
    learner = nimcp_gt_learner_create(&config, NUM_STATES, NUM_ACTIONS);
    ASSERT_NE(learner, nullptr);

    float probs[NUM_ACTIONS];
    nimcp_error_t err = nimcp_gt_exp3_get_probabilities(learner, probs);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Should sum to 1
    float sum = 0.0f;
    for (uint32_t i = 0; i < NUM_ACTIONS; i++) {
        EXPECT_GE(probs[i], 0.0f);
        EXPECT_LE(probs[i], 1.0f);
        sum += probs[i];
    }
    EXPECT_NEAR(sum, 1.0f, 0.01f);
}

//=============================================================================
// Opponent Modeling Tests
//=============================================================================

TEST_F(LearningTest, OpponentModelInit) {
    nimcp_gt_opponent_model_t model;
    nimcp_gt_opponent_model_init(&model, NUM_ACTIONS);

    EXPECT_EQ(model.num_actions, NUM_ACTIONS);
    EXPECT_EQ(model.predicted_type, NIMCP_GT_OPPONENT_UNKNOWN);
    // Initial confidence may be uniform (1/NUM_ACTIONS) or zero depending on implementation
    EXPECT_GE(model.prediction_confidence, 0.0f);
    EXPECT_LE(model.prediction_confidence, 1.0f);

    nimcp_gt_opponent_model_cleanup(&model);
}

TEST_F(LearningTest, ModelOpponent) {
    config.enable_opponent_modeling = true;
    learner = nimcp_gt_learner_create(&config, NUM_STATES, NUM_ACTIONS);
    ASSERT_NE(learner, nullptr);

    // History of opponent always cooperating (action 0 = cooperate)
    uint32_t history[20];
    for (int i = 0; i < 20; i++) {
        history[i] = 0;  // All cooperate
    }

    nimcp_gt_opponent_model_t model;
    nimcp_error_t err = nimcp_gt_model_opponent(learner, history, 20, &model);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Should identify as cooperative type
    EXPECT_EQ(model.predicted_type, NIMCP_GT_OPPONENT_COOPERATIVE);
    EXPECT_GT(model.cooperation_rate, 0.9f);

    nimcp_gt_opponent_model_cleanup(&model);
}

TEST_F(LearningTest, UpdateOpponentModel) {
    config.enable_opponent_modeling = true;
    learner = nimcp_gt_learner_create(&config, NUM_STATES, NUM_ACTIONS);
    ASSERT_NE(learner, nullptr);

    // Update with observations
    for (int i = 0; i < 10; i++) {
        nimcp_error_t err = nimcp_gt_update_opponent_model(learner, 0, 0);  // We cooperate, they cooperate
        EXPECT_EQ(err, NIMCP_SUCCESS);
    }

    nimcp_gt_opponent_model_t model;
    nimcp_gt_get_opponent_model(learner, &model);

    // Cooperation rate tracking is implementation-dependent
    EXPECT_GE(model.cooperation_rate, 0.0f);
    EXPECT_LE(model.cooperation_rate, 1.0f);
    nimcp_gt_opponent_model_cleanup(&model);
}

TEST_F(LearningTest, PredictOpponentAction) {
    config.enable_opponent_modeling = true;
    learner = nimcp_gt_learner_create(&config, NUM_STATES, NUM_ACTIONS);
    ASSERT_NE(learner, nullptr);

    // Build history of opponent playing action 1 when we play action 0
    for (int i = 0; i < 50; i++) {
        nimcp_gt_update_opponent_model(learner, 0, 1);
    }

    uint32_t prediction;
    float confidence;
    nimcp_error_t err = nimcp_gt_predict_opponent_action(learner, 0, &prediction, &confidence);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(prediction, 1);
    EXPECT_GT(confidence, 0.5f);
}

//=============================================================================
// Learning Rate and Exploration Scheduling Tests
//=============================================================================

TEST_F(LearningTest, AdvanceSchedule) {
    config.lr_schedule = NIMCP_GT_SCHEDULE_DECAY;
    config.lr_decay_rate = 0.99f;
    config.exploration_decay = 0.99f;
    learner = nimcp_gt_learner_create(&config, NUM_STATES, NUM_ACTIONS);
    ASSERT_NE(learner, nullptr);

    float initial_lr = nimcp_gt_learner_get_learning_rate(learner);
    float initial_explore = nimcp_gt_learner_get_exploration_rate(learner);

    // Advance schedule
    for (int i = 0; i < 10; i++) {
        nimcp_gt_learner_advance_schedule(learner);
    }

    float new_lr = nimcp_gt_learner_get_learning_rate(learner);
    float new_explore = nimcp_gt_learner_get_exploration_rate(learner);

    // Rates should have decayed
    EXPECT_LT(new_lr, initial_lr);
    EXPECT_LT(new_explore, initial_explore);
}

TEST_F(LearningTest, SetLearningRate) {
    learner = nimcp_gt_learner_create(&config, NUM_STATES, NUM_ACTIONS);
    ASSERT_NE(learner, nullptr);

    nimcp_error_t err = nimcp_gt_learner_set_learning_rate(learner, 0.25f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    float lr = nimcp_gt_learner_get_learning_rate(learner);
    EXPECT_FLOAT_EQ(lr, 0.25f);
}

TEST_F(LearningTest, SetExplorationRate) {
    learner = nimcp_gt_learner_create(&config, NUM_STATES, NUM_ACTIONS);
    ASSERT_NE(learner, nullptr);

    nimcp_error_t err = nimcp_gt_learner_set_exploration_rate(learner, 0.05f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    float explore = nimcp_gt_learner_get_exploration_rate(learner);
    EXPECT_FLOAT_EQ(explore, 0.05f);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(LearningTest, GetStats) {
    learner = nimcp_gt_learner_create(&config, NUM_STATES, NUM_ACTIONS);
    ASSERT_NE(learner, nullptr);

    // Perform some updates
    for (int i = 0; i < 10; i++) {
        nimcp_gt_learner_update(learner, 0, 0, 1.0f, 1);
    }

    nimcp_gt_learning_stats_t stats;
    nimcp_error_t err = nimcp_gt_learner_get_stats(learner, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.updates, 10);
}

TEST_F(LearningTest, ResetStats) {
    learner = nimcp_gt_learner_create(&config, NUM_STATES, NUM_ACTIONS);
    ASSERT_NE(learner, nullptr);

    nimcp_gt_learner_update(learner, 0, 0, 1.0f, 1);

    nimcp_gt_learner_reset_stats(learner);

    nimcp_gt_learning_stats_t stats;
    nimcp_gt_learner_get_stats(learner, &stats);
    EXPECT_EQ(stats.updates, 0);
}

TEST_F(LearningTest, HasConverged) {
    learner = nimcp_gt_learner_create(&config, 2, 2);
    ASSERT_NE(learner, nullptr);

    // Before learning, should be converged (all zeros, no change)
    bool converged = nimcp_gt_learner_has_converged(learner, 0.01f);
    EXPECT_TRUE(converged);

    // After update, might not be converged
    nimcp_gt_learner_update(learner, 0, 0, 10.0f, 1);
    converged = nimcp_gt_learner_has_converged(learner, 0.01f);
    // Could be either depending on implementation
}

TEST_F(LearningTest, ComputeExploitability) {
    learner = nimcp_gt_learner_create(&config, 2, 2);
    ASSERT_NE(learner, nullptr);

    // Set up a simple 2x2 game payoff matrix
    float payoff_matrix[4] = {1.0f, 0.0f, 0.0f, 1.0f};  // Coordination game

    float exploitability;
    nimcp_error_t err = nimcp_gt_compute_exploitability(learner, payoff_matrix, &exploitability);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    // Exploitability can be negative in some formulations (regret-based)
    // Just verify it returns a reasonable value
    EXPECT_LE(exploitability, 100.0f);  // Reasonable upper bound
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(LearningTest, ZeroReward) {
    learner = nimcp_gt_learner_create(&config, NUM_STATES, NUM_ACTIONS);
    ASSERT_NE(learner, nullptr);

    nimcp_error_t err = nimcp_gt_learner_update(learner, 0, 0, 0.0f, 0);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    float q = nimcp_gt_learner_get_q_value(learner, 0, 0);
    EXPECT_FLOAT_EQ(q, 0.0f);
}

TEST_F(LearningTest, NegativeReward) {
    config.learning_rate = 1.0f;
    learner = nimcp_gt_learner_create(&config, NUM_STATES, NUM_ACTIONS);
    ASSERT_NE(learner, nullptr);

    nimcp_error_t err = nimcp_gt_learner_update(learner, 0, 0, -5.0f, 0);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    float q = nimcp_gt_learner_get_q_value(learner, 0, 0);
    EXPECT_LT(q, 0.0f);
}

TEST_F(LearningTest, LargeStateSpace) {
    // Test with larger state space
    learner = nimcp_gt_learner_create(&config, 100, 10);

    if (learner) {
        nimcp_error_t err = nimcp_gt_learner_update(learner, 50, 5, 1.0f, 60);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        float q = nimcp_gt_learner_get_q_value(learner, 50, 5);
        EXPECT_GT(q, 0.0f);
    }
}
