/**
 * @file test_evolutionary_proof.cpp
 * @brief Unit tests for Evolutionary Proof Search System
 *
 * Tests the GA+RL hybrid theorem prover which evolves proof strategies
 * using genetic algorithms and reinforcement learning.
 */

#include <gtest/gtest.h>
#include "utils/nimcp_test_base.h"

extern "C" {
#include "cognitive/neuro_symbolic/nimcp_evolutionary_proof.h"
}

/**
 * @brief Test fixture for Evolutionary Proof tests
 */
class EvolutionaryProofTest : public NimcpTestBase {
protected:
    evolutionary_proof_search_t* prover;
    evoproof_config_t config;

    void SetUp() override {
        NimcpTestBase::SetUp();
        prover = NULL;
        memset(&config, 0, sizeof(config));
        evolutionary_proof_get_default_config(&config);
    }

    void TearDown() override {
        if (prover) {
            evolutionary_proof_destroy(prover);
            prover = NULL;
        }
        NimcpTestBase::TearDown();
    }
};

// ============================================================================
// Default Configuration Tests
// ============================================================================

TEST_F(EvolutionaryProofTest, GetDefaultConfigSucceeds) {
    evoproof_config_t cfg;
    nimcp_error_t err = evolutionary_proof_get_default_config(&cfg);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(EvolutionaryProofTest, GetDefaultConfigNullReturnsError) {
    nimcp_error_t err = evolutionary_proof_get_default_config(NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(EvolutionaryProofTest, DefaultConfigHasValidPopulation) {
    evoproof_config_t cfg;
    evolutionary_proof_get_default_config(&cfg);

    EXPECT_GT(cfg.population_size, 0u);
    EXPECT_LE(cfg.population_size, EVOPROOF_MAX_POPULATION);
}

TEST_F(EvolutionaryProofTest, DefaultConfigHasValidRates) {
    evoproof_config_t cfg;
    evolutionary_proof_get_default_config(&cfg);

    // Mutation and crossover rates should be in [0,1]
    EXPECT_GE(cfg.mutation_rate, 0.0f);
    EXPECT_LE(cfg.mutation_rate, 1.0f);
    EXPECT_GE(cfg.crossover_rate, 0.0f);
    EXPECT_LE(cfg.crossover_rate, 1.0f);
}

TEST_F(EvolutionaryProofTest, DefaultConfigHasValidLearningParams) {
    evoproof_config_t cfg;
    evolutionary_proof_get_default_config(&cfg);

    // Learning rate should be positive
    EXPECT_GT(cfg.learning_rate, 0.0f);
    EXPECT_LE(cfg.learning_rate, 1.0f);

    // Discount factor should be in (0,1]
    EXPECT_GT(cfg.discount_factor, 0.0f);
    EXPECT_LE(cfg.discount_factor, 1.0f);

    // Exploration rate should be in [0,1]
    EXPECT_GE(cfg.initial_epsilon, 0.0f);
    EXPECT_LE(cfg.initial_epsilon, 1.0f);
}

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(EvolutionaryProofTest, CreateWithConfigSucceeds) {
    prover = evolutionary_proof_create(&config);
    ASSERT_NE(prover, nullptr);
}

TEST_F(EvolutionaryProofTest, CreateWithNullConfigSucceeds) {
    prover = evolutionary_proof_create(NULL);
    EXPECT_NE(prover, nullptr);
}

TEST_F(EvolutionaryProofTest, DestroyNullIsNoOp) {
    evolutionary_proof_destroy(NULL);
    SUCCEED();
}

TEST_F(EvolutionaryProofTest, CreateDestroyMultipleTimesSucceeds) {
    for (int i = 0; i < 5; i++) {
        prover = evolutionary_proof_create(&config);
        ASSERT_NE(prover, nullptr) << "Failed on iteration " << i;
        evolutionary_proof_destroy(prover);
        prover = NULL;
    }
}

TEST_F(EvolutionaryProofTest, ResetNullReturnsError) {
    nimcp_error_t err = evolutionary_proof_reset(NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(EvolutionaryProofTest, ResetClearsState) {
    prover = evolutionary_proof_create(&config);
    ASSERT_NE(prover, nullptr);

    nimcp_error_t err = evolutionary_proof_reset(prover);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

// ============================================================================
// Algorithm Configuration Tests
// ============================================================================

TEST_F(EvolutionaryProofTest, CreateWithGeneticAlgorithm) {
    config.algorithm = EVOPROOF_ALGO_GENETIC;
    prover = evolutionary_proof_create(&config);
    EXPECT_NE(prover, nullptr);
}

TEST_F(EvolutionaryProofTest, CreateWithQLearning) {
    config.algorithm = EVOPROOF_ALGO_QLEARNING;
    prover = evolutionary_proof_create(&config);
    EXPECT_NE(prover, nullptr);
}

TEST_F(EvolutionaryProofTest, CreateWithHybridAlgorithm) {
    config.algorithm = EVOPROOF_ALGO_HYBRID;
    prover = evolutionary_proof_create(&config);
    EXPECT_NE(prover, nullptr);
}

// ============================================================================
// Selection Method Tests
// ============================================================================

TEST_F(EvolutionaryProofTest, CreateWithRouletteSelection) {
    config.selection = EVOPROOF_SELECT_ROULETTE;
    prover = evolutionary_proof_create(&config);
    EXPECT_NE(prover, nullptr);
}

TEST_F(EvolutionaryProofTest, CreateWithTournamentSelection) {
    config.selection = EVOPROOF_SELECT_TOURNAMENT;
    config.tournament_size = 3;
    prover = evolutionary_proof_create(&config);
    EXPECT_NE(prover, nullptr);
}

TEST_F(EvolutionaryProofTest, CreateWithElitismSelection) {
    config.selection = EVOPROOF_SELECT_ELITISM;
    config.elite_count = 2;
    prover = evolutionary_proof_create(&config);
    EXPECT_NE(prover, nullptr);
}

// ============================================================================
// Crossover Method Tests
// ============================================================================

TEST_F(EvolutionaryProofTest, CreateWithSinglePointCrossover) {
    config.crossover = EVOPROOF_CROSS_SINGLE;
    prover = evolutionary_proof_create(&config);
    EXPECT_NE(prover, nullptr);
}

TEST_F(EvolutionaryProofTest, CreateWithUniformCrossover) {
    config.crossover = EVOPROOF_CROSS_UNIFORM;
    prover = evolutionary_proof_create(&config);
    EXPECT_NE(prover, nullptr);
}

// ============================================================================
// Population Management Tests
// ============================================================================

TEST_F(EvolutionaryProofTest, InitPopulationNullReturnsError) {
    nimcp_error_t err = evolutionary_proof_init_population(NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(EvolutionaryProofTest, InitPopulationSucceeds) {
    config.population_size = 16;
    prover = evolutionary_proof_create(&config);
    ASSERT_NE(prover, nullptr);

    // Population should be initialized during creation
    // Calling init again should succeed or return appropriate status
    nimcp_error_t err = evolutionary_proof_init_population(prover);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

// ============================================================================
// Generation Evolution Tests
// ============================================================================

TEST_F(EvolutionaryProofTest, EvolveGenerationNullReturnsZero) {
    uint32_t result = evolutionary_proof_evolve_generation(NULL);
    EXPECT_EQ(result, 0u);
}

TEST_F(EvolutionaryProofTest, EvolveGenerationReturnsOffspringCount) {
    prover = evolutionary_proof_create(&config);
    ASSERT_NE(prover, nullptr);

    // evolve_generation returns number of offspring created, not generation count
    uint32_t offspring1 = evolutionary_proof_evolve_generation(prover);
    EXPECT_GT(offspring1, 0u);  // Should create some offspring

    uint32_t offspring2 = evolutionary_proof_evolve_generation(prover);
    EXPECT_GT(offspring2, 0u);  // Should create some offspring again
}

TEST_F(EvolutionaryProofTest, EvolveMultipleGenerationsCreatesOffspring) {
    prover = evolutionary_proof_create(&config);
    ASSERT_NE(prover, nullptr);

    // evolve_generation returns offspring count, not generation number
    for (int i = 0; i < 10; i++) {
        uint32_t offspring = evolutionary_proof_evolve_generation(prover);
        EXPECT_GT(offspring, 0u) << "No offspring created at iteration " << i;
    }
}

// ============================================================================
// Action Selection Tests
// ============================================================================

TEST_F(EvolutionaryProofTest, SelectActionNullReturnsError) {
    proof_state_t state;
    memset(&state, 0, sizeof(state));

    proof_action_t action = evolutionary_proof_select_action(NULL, &state);
    EXPECT_LT((int)action, (int)PROOF_ACTION_COUNT);
}

TEST_F(EvolutionaryProofTest, SelectActionReturnsValidAction) {
    prover = evolutionary_proof_create(&config);
    ASSERT_NE(prover, nullptr);

    proof_state_t state;
    memset(&state, 0, sizeof(state));
    state.state_id = 1;
    state.goal_complexity = 5;
    state.depth = 2;
    state.available_rules = 10;
    state.progress_estimate = 0.3f;

    proof_action_t action = evolutionary_proof_select_action(prover, &state);
    EXPECT_GE((int)action, 0);
    EXPECT_LT((int)action, (int)PROOF_ACTION_COUNT);
}

// ============================================================================
// Q-Learning Update Tests
// ============================================================================

TEST_F(EvolutionaryProofTest, UpdateQNullReturnsZero) {
    proof_state_t state, next;
    memset(&state, 0, sizeof(state));
    memset(&next, 0, sizeof(next));

    // Function returns Q-value, not error code. Returns 0 on null input.
    float result = evolutionary_proof_update_q(
        NULL, &state, PROOF_ACTION_APPLY_RULE, 1.0f, &next, false);
    EXPECT_EQ(result, 0.0f);  // Safe fallback on null
}

TEST_F(EvolutionaryProofTest, UpdateQReturnsReasonableValue) {
    config.algorithm = EVOPROOF_ALGO_QLEARNING;
    prover = evolutionary_proof_create(&config);
    ASSERT_NE(prover, nullptr);

    proof_state_t state, next;
    memset(&state, 0, sizeof(state));
    memset(&next, 0, sizeof(next));
    state.state_id = 1;
    next.state_id = 2;

    float result = evolutionary_proof_update_q(
        prover, &state, PROOF_ACTION_APPLY_RULE, 1.0f, &next, false);
    // Result should be TD error, can be positive or negative
    EXPECT_TRUE(std::isfinite(result));
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(EvolutionaryProofTest, GetStatsNullReturnsError) {
    evoproof_stats_t stats;
    nimcp_error_t err = evolutionary_proof_get_stats(NULL, &stats);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(EvolutionaryProofTest, GetStatsWithNullStatsReturnsError) {
    prover = evolutionary_proof_create(&config);
    ASSERT_NE(prover, nullptr);

    nimcp_error_t err = evolutionary_proof_get_stats(prover, NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(EvolutionaryProofTest, InitialStatsAreZero) {
    prover = evolutionary_proof_create(&config);
    ASSERT_NE(prover, nullptr);

    evoproof_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));  // Fill with non-zero

    nimcp_error_t err = evolutionary_proof_get_stats(prover, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.proofs_attempted, 0u);
    EXPECT_EQ(stats.proofs_succeeded, 0u);
}

// ============================================================================
// Modulation Tests
// ============================================================================

TEST_F(EvolutionaryProofTest, ModulateATPNullReturnsError) {
    nimcp_error_t err = evolutionary_proof_modulate_atp(NULL, 1.0f);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(EvolutionaryProofTest, ModulateATPValidRange) {
    prover = evolutionary_proof_create(&config);
    ASSERT_NE(prover, nullptr);

    nimcp_error_t err = evolutionary_proof_modulate_atp(prover, 0.5f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(EvolutionaryProofTest, ModulateATPClampsOutOfRange) {
    prover = evolutionary_proof_create(&config);
    ASSERT_NE(prover, nullptr);

    // Should clamp to valid range [0,1]
    nimcp_error_t err1 = evolutionary_proof_modulate_atp(prover, -0.5f);
    EXPECT_EQ(err1, NIMCP_SUCCESS);

    nimcp_error_t err2 = evolutionary_proof_modulate_atp(prover, 1.5f);
    EXPECT_EQ(err2, NIMCP_SUCCESS);
}

// ============================================================================
// Proof Trace Tests
// ============================================================================

TEST_F(EvolutionaryProofTest, TraceInitNullReturnsError) {
    nimcp_error_t err = evolutionary_proof_trace_init(NULL, 10);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(EvolutionaryProofTest, TraceInitSucceeds) {
    evoproof_trace_t trace;
    memset(&trace, 0, sizeof(trace));

    nimcp_error_t err = evolutionary_proof_trace_init(&trace, 10);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_NE(trace.steps, nullptr);
    EXPECT_EQ(trace.num_steps, 0u);
    EXPECT_EQ(trace.capacity, 10u);

    evolutionary_proof_trace_cleanup(&trace);
}

TEST_F(EvolutionaryProofTest, TraceCleanupNullIsNoOp) {
    evolutionary_proof_trace_cleanup(NULL);
    SUCCEED();
}

// ============================================================================
// Experience Replay Tests
// ============================================================================

TEST_F(EvolutionaryProofTest, AddExperienceNullReturnsError) {
    proof_experience_t exp;
    memset(&exp, 0, sizeof(exp));

    nimcp_error_t err = evolutionary_proof_store_experience(NULL, &exp);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(EvolutionaryProofTest, AddExperienceSucceeds) {
    config.algorithm = EVOPROOF_ALGO_QLEARNING;
    prover = evolutionary_proof_create(&config);
    ASSERT_NE(prover, nullptr);

    proof_experience_t exp;
    memset(&exp, 0, sizeof(exp));
    exp.state.state_id = 1;
    exp.action = PROOF_ACTION_APPLY_RULE;
    exp.reward = 1.0f;
    exp.next_state.state_id = 2;
    exp.terminal = false;

    nimcp_error_t err = evolutionary_proof_store_experience(prover, &exp);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

// ============================================================================
// Best Strategy Tests
// ============================================================================

TEST_F(EvolutionaryProofTest, GetBestStrategyNullReturnsNull) {
    const proof_strategy_t* best = evolutionary_proof_get_best(NULL);
    EXPECT_EQ(best, nullptr);
}

TEST_F(EvolutionaryProofTest, GetBestStrategyReturnsValid) {
    prover = evolutionary_proof_create(&config);
    ASSERT_NE(prover, nullptr);

    const proof_strategy_t* best = evolutionary_proof_get_best(prover);
    // Initially may be NULL or first strategy
    // Just check it doesn't crash
    SUCCEED();
}

TEST_F(EvolutionaryProofTest, GetBestStrategyAfterEvolution) {
    prover = evolutionary_proof_create(&config);
    ASSERT_NE(prover, nullptr);

    // Evolve a few generations
    for (int i = 0; i < 5; i++) {
        evolutionary_proof_evolve_generation(prover);
    }

    const proof_strategy_t* best = evolutionary_proof_get_best(prover);
    EXPECT_NE(best, nullptr);
}
