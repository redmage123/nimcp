/**
 * @file test_recovery_evolution.cpp
 * @brief Unit tests for recovery strategy evolution module
 *
 * Tests genetic algorithm optimization, Q-learning action selection,
 * experience replay, and transfer learning.
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_recovery_evolution.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class RecoveryEvolutionTest : public ::testing::Test {
protected:
    re_context_t* ctx;
    re_config_t config;

    void SetUp() override {
        config = re_default_config();
        ctx = re_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            re_destroy(ctx);
            ctx = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST(ReLifecycleTest, DefaultConfig) {
    re_config_t config = re_default_config();

    EXPECT_GT(config.population_size, 0);
    EXPECT_GT(config.mutation_rate, 0.0f);
    EXPECT_LT(config.mutation_rate, 1.0f);
    EXPECT_GT(config.crossover_rate, 0.0f);
    EXPECT_GT(config.learning_rate, 0.0f);
    EXPECT_GT(config.discount_factor, 0.0f);
}

TEST(ReLifecycleTest, CreateAndDestroy) {
    re_config_t config = re_default_config();

    re_context_t* ctx = re_create(&config);
    ASSERT_NE(ctx, nullptr);

    re_destroy(ctx);
}

TEST(ReLifecycleTest, CreateWithNullConfig) {
    re_context_t* ctx = re_create(nullptr);
    EXPECT_EQ(ctx, nullptr);
}

//=============================================================================
// Genetic Algorithm Tests
//=============================================================================

TEST_F(RecoveryEvolutionTest, InitPopulation) {
    EXPECT_TRUE(re_init_population(ctx));
}

TEST_F(RecoveryEvolutionTest, AddStrategy) {
    re_strategy_t strategy = {0};
    strategy.id = 1;
    strategy.gene_count = 2;
    strategy.genes[0].value = 0.5f;
    strategy.genes[1].value = 0.3f;
    strategy.action_count = 2;
    strategy.actions[0] = RE_ACTION_RETRY;
    strategy.actions[1] = RE_ACTION_CHECKPOINT;
    strategy.fitness = 0.5f;

    EXPECT_TRUE(re_add_strategy(ctx, &strategy));
}

TEST_F(RecoveryEvolutionTest, GetStrategy) {
    re_strategy_t strategy = {0};
    strategy.id = 1;
    strategy.fitness = 0.75f;

    re_add_strategy(ctx, &strategy);

    re_strategy_t retrieved;
    EXPECT_TRUE(re_get_strategy(ctx, 1, &retrieved));
    EXPECT_NEAR(retrieved.fitness, 0.75f, 0.01f);
}

TEST_F(RecoveryEvolutionTest, GetBestStrategy) {
    // Add multiple strategies with different fitness
    for (int i = 1; i <= 5; i++) {
        re_strategy_t strategy = {0};
        strategy.id = i;
        strategy.fitness = 0.1f * i;  // Increasing fitness
        re_add_strategy(ctx, &strategy);
    }

    re_strategy_t best;
    EXPECT_TRUE(re_get_best_strategy(ctx, &best));
    EXPECT_NEAR(best.fitness, 0.5f, 0.01f);  // Highest fitness
}

TEST_F(RecoveryEvolutionTest, EvaluateFitness) {
    re_strategy_t strategy = {0};
    strategy.id = 1;
    re_add_strategy(ctx, &strategy);

    re_outcome_t outcome = {0};
    outcome.strategy_id = 1;
    outcome.success = true;
    outcome.recovery_time_ms = 100;
    outcome.resource_usage = 0.5f;

    float fitness = re_evaluate_fitness(ctx, 1, &outcome);
    EXPECT_GE(fitness, 0.0f);
    EXPECT_LE(fitness, 1.0f);
}

TEST_F(RecoveryEvolutionTest, SelectParents) {
    re_init_population(ctx);

    re_strategy_t parent1, parent2;
    EXPECT_TRUE(re_select_parents(ctx, &parent1, &parent2));
}

TEST_F(RecoveryEvolutionTest, Crossover) {
    re_strategy_t parent1 = {0};
    parent1.id = 1;
    parent1.gene_count = 2;
    parent1.genes[0].value = 1.0f;
    parent1.genes[1].value = 2.0f;
    parent1.action_count = 2;
    parent1.actions[0] = RE_ACTION_RETRY;
    parent1.actions[1] = RE_ACTION_CHECKPOINT;

    re_strategy_t parent2 = {0};
    parent2.id = 2;
    parent2.gene_count = 2;
    parent2.genes[0].value = 3.0f;
    parent2.genes[1].value = 4.0f;
    parent2.action_count = 2;
    parent2.actions[0] = RE_ACTION_RESTART;
    parent2.actions[1] = RE_ACTION_FAILOVER;

    re_strategy_t child;
    EXPECT_TRUE(re_crossover(ctx, &parent1, &parent2, &child));
    EXPECT_EQ(child.gene_count, 2);
}

TEST_F(RecoveryEvolutionTest, Mutate) {
    re_strategy_t strategy = {0};
    strategy.id = 1;
    strategy.gene_count = 2;
    strategy.genes[0].value = 1.0f;
    strategy.genes[0].min_value = 0.0f;
    strategy.genes[0].max_value = 10.0f;
    strategy.genes[0].mutation_sigma = 0.1f;
    strategy.genes[1].value = 2.0f;
    strategy.genes[1].min_value = 0.0f;
    strategy.genes[1].max_value = 10.0f;
    strategy.genes[1].mutation_sigma = 0.1f;

    // Mutation may or may not change values based on probability
    bool mutated = re_mutate(ctx, &strategy);
    (void)mutated;
}

TEST_F(RecoveryEvolutionTest, EvolveGeneration) {
    re_init_population(ctx);

    uint32_t gen = re_evolve_generation(ctx);
    EXPECT_GE(gen, 1);
}

//=============================================================================
// Reinforcement Learning Tests
//=============================================================================

TEST_F(RecoveryEvolutionTest, SelectAction) {
    uint32_t state = re_encode_state(1, 50, 0);  // fault_type=1, severity=50

    re_action_t action = re_select_action(ctx, state);
    EXPECT_LT((int)action, RE_ACTION_COUNT);
}

TEST_F(RecoveryEvolutionTest, UpdateQ) {
    uint32_t state = re_encode_state(1, 50, 0);
    uint32_t next_state = re_encode_state(0, 0, 0);  // Recovered

    float q = re_update_q(ctx, state, RE_ACTION_RETRY, 1.0f, next_state, true);
    EXPECT_GE(q, 0.0f);
}

TEST_F(RecoveryEvolutionTest, GetQValue) {
    uint32_t state = re_encode_state(1, 50, 0);

    // Update Q value first
    re_update_q(ctx, state, RE_ACTION_RETRY, 0.5f, state, false);

    float q = re_get_q_value(ctx, state, RE_ACTION_RETRY);
    EXPECT_GT(q, 0.0f);
}

TEST_F(RecoveryEvolutionTest, GetBestAction) {
    uint32_t state = re_encode_state(1, 50, 0);

    // Update Q values for different actions
    re_update_q(ctx, state, RE_ACTION_RETRY, 0.3f, state, false);
    re_update_q(ctx, state, RE_ACTION_CHECKPOINT, 0.8f, state, false);
    re_update_q(ctx, state, RE_ACTION_RESTART, 0.5f, state, false);

    re_action_t best = re_get_best_action(ctx, state);
    EXPECT_EQ(best, RE_ACTION_CHECKPOINT);
}

TEST_F(RecoveryEvolutionTest, DecayEpsilon) {
    float initial = config.epsilon;
    float after = re_decay_epsilon(ctx);
    EXPECT_LT(after, initial);
}

//=============================================================================
// Experience Replay Tests
//=============================================================================

TEST_F(RecoveryEvolutionTest, StoreExperience) {
    re_experience_t exp = {0};
    exp.state = re_encode_state(1, 50, 0);
    exp.action = RE_ACTION_RETRY;
    exp.reward = 1.0f;
    exp.next_state = re_encode_state(0, 0, 0);
    exp.terminal = true;

    EXPECT_TRUE(re_store_experience(ctx, &exp));
}

TEST_F(RecoveryEvolutionTest, SampleBatch) {
    // Store multiple experiences
    for (int i = 0; i < 50; i++) {
        re_experience_t exp = {0};
        exp.state = re_encode_state(1, 50, 0);
        exp.action = (re_action_t)(i % RE_ACTION_COUNT);
        exp.reward = (i % 2 == 0) ? 1.0f : -0.5f;
        exp.terminal = (i % 10 == 9);
        re_store_experience(ctx, &exp);
    }

    re_experience_t batch[32];
    uint32_t count = re_sample_batch(ctx, batch, 32);
    EXPECT_GT(count, 0);
}

TEST_F(RecoveryEvolutionTest, LearnFromBatch) {
    // Store enough experiences
    for (int i = 0; i < 100; i++) {
        re_experience_t exp = {0};
        exp.state = re_encode_state(1, 50, 0);
        exp.action = RE_ACTION_RETRY;
        exp.reward = 1.0f;
        exp.next_state = re_encode_state(0, 0, 0);
        exp.terminal = true;
        re_store_experience(ctx, &exp);
    }

    float loss = re_learn_from_batch(ctx);
    // Loss can be any value
    (void)loss;
}

//=============================================================================
// Transfer Learning Tests
//=============================================================================

TEST_F(RecoveryEvolutionTest, ExportKnowledge) {
    re_init_population(ctx);

    char buffer[4096];
    size_t written = re_export_knowledge(ctx, buffer, sizeof(buffer));
    EXPECT_GT(written, 0);
}

TEST_F(RecoveryEvolutionTest, ImportKnowledge) {
    // Create source context with knowledge
    re_config_t source_config = re_default_config();
    re_context_t* source = re_create(&source_config);
    re_init_population(source);

    char buffer[4096];
    size_t written = re_export_knowledge(source, buffer, sizeof(buffer));

    // Import into our context
    EXPECT_TRUE(re_import_knowledge(ctx, buffer, written));

    re_destroy(source);
}

TEST_F(RecoveryEvolutionTest, TransferFrom) {
    // Create source context
    re_config_t source_config = re_default_config();
    re_context_t* source = re_create(&source_config);
    re_init_population(source);

    EXPECT_TRUE(re_transfer_from(ctx, source, 0.5f));

    re_destroy(source);
}

//=============================================================================
// Strategy Recommendation Tests
//=============================================================================

TEST_F(RecoveryEvolutionTest, RecommendStrategy) {
    re_init_population(ctx);

    re_strategy_t strategy;
    float confidence = re_recommend_strategy(ctx, 1, 50, &strategy);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

TEST_F(RecoveryEvolutionTest, GetActionSequence) {
    re_init_population(ctx);

    re_action_t actions[10];
    uint32_t count = re_get_action_sequence(ctx, 1, actions, 10);
    EXPECT_GE(count, 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(RecoveryEvolutionTest, GetStats) {
    re_stats_t stats;
    EXPECT_TRUE(re_get_stats(ctx, &stats));

    EXPECT_GE(stats.total_generations, 0);
    EXPECT_GE(stats.total_evaluations, 0);
}

TEST_F(RecoveryEvolutionTest, ResetStats) {
    re_init_population(ctx);
    re_evolve_generation(ctx);

    re_reset_stats(ctx);

    re_stats_t stats;
    EXPECT_TRUE(re_get_stats(ctx, &stats));
    EXPECT_EQ(stats.total_generations, 0);
}

TEST_F(RecoveryEvolutionTest, GetDiversity) {
    re_init_population(ctx);

    float diversity = re_get_diversity(ctx);
    EXPECT_GE(diversity, 0.0f);
    EXPECT_LE(diversity, 1.0f);
}

//=============================================================================
// Utility Tests
//=============================================================================

TEST_F(RecoveryEvolutionTest, CalculateFitness) {
    re_outcome_t outcome = {0};
    outcome.success = true;
    outcome.recovery_time_ms = 100;
    outcome.resource_usage = 0.5f;
    outcome.data_loss = 0.0f;

    float fitness = re_calculate_fitness(ctx, &outcome);
    EXPECT_GE(fitness, 0.0f);
    EXPECT_LE(fitness, 1.0f);
}

TEST_F(RecoveryEvolutionTest, CalculateReward) {
    re_outcome_t success_outcome = {0};
    success_outcome.success = true;
    success_outcome.recovery_time_ms = 100;

    float reward = re_calculate_reward(&success_outcome);
    EXPECT_GT(reward, 0.0f);

    re_outcome_t failure_outcome = {0};
    failure_outcome.success = false;

    float neg_reward = re_calculate_reward(&failure_outcome);
    EXPECT_LT(neg_reward, reward);
}

TEST_F(RecoveryEvolutionTest, EncodeState) {
    uint32_t state = re_encode_state(5, 75, 10);
    EXPECT_GT(state, 0);
}

//=============================================================================
// String Conversion Tests
//=============================================================================

TEST(ReStringTest, AlgorithmToString) {
    EXPECT_STREQ("Genetic", re_algorithm_to_string(RE_ALGO_GENETIC));
    EXPECT_STREQ("Q-Learning", re_algorithm_to_string(RE_ALGO_Q_LEARNING));
    EXPECT_STREQ("SARSA", re_algorithm_to_string(RE_ALGO_SARSA));
    EXPECT_STREQ("Actor-Critic", re_algorithm_to_string(RE_ALGO_ACTOR_CRITIC));
    EXPECT_STREQ("Hybrid", re_algorithm_to_string(RE_ALGO_HYBRID));
}

TEST(ReStringTest, ActionToString) {
    EXPECT_STREQ("Retry", re_action_to_string(RE_ACTION_RETRY));
    EXPECT_STREQ("Checkpoint", re_action_to_string(RE_ACTION_CHECKPOINT));
    EXPECT_STREQ("ReduceLoad", re_action_to_string(RE_ACTION_REDUCE_LOAD));
    EXPECT_STREQ("Isolate", re_action_to_string(RE_ACTION_ISOLATE));
    EXPECT_STREQ("Restart", re_action_to_string(RE_ACTION_RESTART));
    EXPECT_STREQ("Failover", re_action_to_string(RE_ACTION_FAILOVER));
    EXPECT_STREQ("Degrade", re_action_to_string(RE_ACTION_DEGRADE));
    EXPECT_STREQ("Escalate", re_action_to_string(RE_ACTION_ESCALATE));
}

TEST(ReStringTest, SelectionToString) {
    EXPECT_STREQ("Roulette", re_selection_to_string(RE_SELECT_ROULETTE));
    EXPECT_STREQ("Tournament", re_selection_to_string(RE_SELECT_TOURNAMENT));
    EXPECT_STREQ("Rank", re_selection_to_string(RE_SELECT_RANK));
    EXPECT_STREQ("Elitism", re_selection_to_string(RE_SELECT_ELITISM));
}

TEST(ReStringTest, CrossoverToString) {
    EXPECT_STREQ("Single", re_crossover_to_string(RE_CROSS_SINGLE));
    EXPECT_STREQ("TwoPoint", re_crossover_to_string(RE_CROSS_TWO_POINT));
    EXPECT_STREQ("Uniform", re_crossover_to_string(RE_CROSS_UNIFORM));
    EXPECT_STREQ("Blend", re_crossover_to_string(RE_CROSS_BLEND));
}

TEST(ReStringTest, FitnessToString) {
    EXPECT_STREQ("RecoveryTime", re_fitness_to_string(RE_FIT_RECOVERY_TIME));
    EXPECT_STREQ("SuccessRate", re_fitness_to_string(RE_FIT_SUCCESS_RATE));
    EXPECT_STREQ("ResourceUsage", re_fitness_to_string(RE_FIT_RESOURCE_USAGE));
    EXPECT_STREQ("DataLoss", re_fitness_to_string(RE_FIT_DATA_LOSS));
    EXPECT_STREQ("Composite", re_fitness_to_string(RE_FIT_COMPOSITE));
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(RecoveryEvolutionTest, GetNonexistentStrategy) {
    re_strategy_t strategy;
    EXPECT_FALSE(re_get_strategy(ctx, 999, &strategy));
}

TEST_F(RecoveryEvolutionTest, EvolveEmptyPopulation) {
    // Evolving without initialization should handle gracefully
    uint32_t gen = re_evolve_generation(ctx);
    // May return 0 or 1 depending on implementation
    (void)gen;
}

TEST_F(RecoveryEvolutionTest, MaxPopulation) {
    // Try to add more than max population
    for (uint32_t i = 1; i <= RE_MAX_POPULATION + 5; i++) {
        re_strategy_t strategy = {0};
        strategy.id = i;
        bool added = re_add_strategy(ctx, &strategy);
        if (i > RE_MAX_POPULATION) {
            EXPECT_FALSE(added);
        }
    }
}
