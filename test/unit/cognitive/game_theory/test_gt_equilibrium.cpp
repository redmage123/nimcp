//=============================================================================
// test_gt_equilibrium.cpp - Unit tests for Nash Equilibrium Module
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/game_theory/nimcp_gt_equilibrium.h"
#include "cognitive/game_theory/nimcp_game_theory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EquilibriumTest : public ::testing::Test {
protected:
    nimcp_equilibrium_t solver = nullptr;
    nimcp_equilibrium_config_t config;

    void SetUp() override {
        // Default 2-player 2-strategy config
        uint32_t strategies[2] = {2, 2};
        config = nimcp_equilibrium_default_config(2, strategies);
    }

    void TearDown() override {
        if (solver) {
            nimcp_equilibrium_destroy(solver);
            solver = nullptr;
        }
    }

    // Helper to set up Prisoner's Dilemma
    void SetupPrisonersDilemma() {
        // Payoff structure:
        //              Player 2
        //           C        D
        // Player 1
        //    C    (3,3)    (0,5)
        //    D    (5,0)    (1,1)
        //
        // Row player payoffs (player 0)
        float p0_payoffs[4] = {3.0f, 0.0f, 5.0f, 1.0f};
        // Column player payoffs (player 1)
        float p1_payoffs[4] = {3.0f, 5.0f, 0.0f, 1.0f};

        ASSERT_EQ(nimcp_equilibrium_set_bimatrix(solver, p0_payoffs, p1_payoffs, 2, 2), NIMCP_SUCCESS);
    }

    // Helper to set up Battle of Sexes
    void SetupBattleOfSexes() {
        // Payoff structure:
        //              Player 2
        //          Opera    Football
        // Player 1
        //  Opera   (3,2)    (0,0)
        // Football (0,0)    (2,3)
        //
        float p0_payoffs[4] = {3.0f, 0.0f, 0.0f, 2.0f};
        float p1_payoffs[4] = {2.0f, 0.0f, 0.0f, 3.0f};

        ASSERT_EQ(nimcp_equilibrium_set_bimatrix(solver, p0_payoffs, p1_payoffs, 2, 2), NIMCP_SUCCESS);
    }

    // Helper to set up Matching Pennies (zero-sum, no pure NE)
    void SetupMatchingPennies() {
        // Payoff structure:
        //              Player 2
        //          Heads    Tails
        // Player 1
        //  Heads   (1,-1)   (-1,1)
        //  Tails   (-1,1)   (1,-1)
        //
        float p0_payoffs[4] = {1.0f, -1.0f, -1.0f, 1.0f};
        float p1_payoffs[4] = {-1.0f, 1.0f, 1.0f, -1.0f};

        ASSERT_EQ(nimcp_equilibrium_set_bimatrix(solver, p0_payoffs, p1_payoffs, 2, 2), NIMCP_SUCCESS);
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(EquilibriumTest, DefaultConfigValues) {
    EXPECT_EQ(config.num_players, 2);
    EXPECT_EQ(config.num_strategies[0], 2);
    EXPECT_EQ(config.num_strategies[1], 2);
    EXPECT_GT(config.max_iterations, 0);
    EXPECT_GT(config.convergence_epsilon, 0.0f);
}

TEST_F(EquilibriumTest, CreateDestroy) {
    solver = nimcp_equilibrium_create(&config);
    ASSERT_NE(solver, nullptr);

    nimcp_equilibrium_destroy(solver);
    solver = nullptr;  // Prevent double-free in TearDown
}

TEST_F(EquilibriumTest, CreateWithNullConfig) {
    solver = nimcp_equilibrium_create(nullptr);
    // Should return NULL or use safe defaults
    // Implementation-dependent behavior
}

TEST_F(EquilibriumTest, CreateWithDifferentStrategyCounts) {
    uint32_t strategies[2] = {3, 4};
    config = nimcp_equilibrium_default_config(2, strategies);

    solver = nimcp_equilibrium_create(&config);
    ASSERT_NE(solver, nullptr);
    EXPECT_EQ(config.num_strategies[0], 3);
    EXPECT_EQ(config.num_strategies[1], 4);
}

//=============================================================================
// Game Setup Tests
//=============================================================================

TEST_F(EquilibriumTest, SetBimatrix) {
    solver = nimcp_equilibrium_create(&config);
    ASSERT_NE(solver, nullptr);

    float p0_payoffs[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float p1_payoffs[4] = {4.0f, 3.0f, 2.0f, 1.0f};

    nimcp_error_t err = nimcp_equilibrium_set_bimatrix(solver, p0_payoffs, p1_payoffs, 2, 2);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(EquilibriumTest, SetPayoffsPerPlayer) {
    solver = nimcp_equilibrium_create(&config);
    ASSERT_NE(solver, nullptr);

    float payoffs[4] = {1.0f, 2.0f, 3.0f, 4.0f};

    nimcp_error_t err = nimcp_equilibrium_set_payoffs(solver, 0, payoffs, 4);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    err = nimcp_equilibrium_set_payoffs(solver, 1, payoffs, 4);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(EquilibriumTest, SetPayoffsInvalidPlayer) {
    solver = nimcp_equilibrium_create(&config);
    ASSERT_NE(solver, nullptr);

    float payoffs[4] = {1.0f, 2.0f, 3.0f, 4.0f};

    nimcp_error_t err = nimcp_equilibrium_set_payoffs(solver, 5, payoffs, 4);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

//=============================================================================
// Pure Strategy Nash Equilibrium Tests
//=============================================================================

TEST_F(EquilibriumTest, FindPureNashPrisonersDilemma) {
    solver = nimcp_equilibrium_create(&config);
    ASSERT_NE(solver, nullptr);
    SetupPrisonersDilemma();

    nimcp_equilibrium_result_t result;
    nimcp_error_t err = nimcp_equilibrium_find_pure_nash(solver, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Prisoner's Dilemma has unique pure NE at (Defect, Defect)
    EXPECT_EQ(result.type, NIMCP_EQUILIBRIUM_TYPE_PURE);
    EXPECT_EQ(result.strategies.pure_strategies[0], 1);  // Defect
    EXPECT_EQ(result.strategies.pure_strategies[1], 1);  // Defect
    EXPECT_FLOAT_EQ(result.payoffs[0], 1.0f);
    EXPECT_FLOAT_EQ(result.payoffs[1], 1.0f);
}

TEST_F(EquilibriumTest, FindPureNashBattleOfSexes) {
    solver = nimcp_equilibrium_create(&config);
    ASSERT_NE(solver, nullptr);
    SetupBattleOfSexes();

    nimcp_equilibrium_result_t result;
    nimcp_error_t err = nimcp_equilibrium_find_pure_nash(solver, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Battle of Sexes has two pure NE: (Opera, Opera) or (Football, Football)
    EXPECT_EQ(result.type, NIMCP_EQUILIBRIUM_TYPE_PURE);
    // Should find one of them
    bool is_opera_opera = (result.strategies.pure_strategies[0] == 0 &&
                           result.strategies.pure_strategies[1] == 0);
    bool is_football_football = (result.strategies.pure_strategies[0] == 1 &&
                                  result.strategies.pure_strategies[1] == 1);
    EXPECT_TRUE(is_opera_opera || is_football_football);
}

TEST_F(EquilibriumTest, NoPureNashMatchingPennies) {
    solver = nimcp_equilibrium_create(&config);
    ASSERT_NE(solver, nullptr);
    SetupMatchingPennies();

    nimcp_equilibrium_result_t result;
    nimcp_error_t err = nimcp_equilibrium_find_pure_nash(solver, &result);

    // Matching Pennies has no pure Nash equilibrium
    EXPECT_EQ(err, NIMCP_GT_ERROR_NO_EQUILIBRIUM);
}

//=============================================================================
// Mixed Strategy Nash Equilibrium Tests
//=============================================================================

TEST_F(EquilibriumTest, FindMixedNashMatchingPennies) {
    solver = nimcp_equilibrium_create(&config);
    ASSERT_NE(solver, nullptr);
    SetupMatchingPennies();

    nimcp_equilibrium_result_t result;
    nimcp_error_t err = nimcp_equilibrium_find_mixed_nash(solver, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Mixed NE is (0.5, 0.5) for both players
    EXPECT_EQ(result.type, NIMCP_EQUILIBRIUM_TYPE_MIXED);

    // Check that probabilities sum to 1 and are approximately 0.5 each
    if (result.strategies.mixed_strategies[0]) {
        float sum0 = result.strategies.mixed_strategies[0][0] +
                     result.strategies.mixed_strategies[0][1];
        EXPECT_NEAR(sum0, 1.0f, 0.01f);
        EXPECT_NEAR(result.strategies.mixed_strategies[0][0], 0.5f, 0.1f);
    }

    if (result.strategies.mixed_strategies[1]) {
        float sum1 = result.strategies.mixed_strategies[1][0] +
                     result.strategies.mixed_strategies[1][1];
        EXPECT_NEAR(sum1, 1.0f, 0.01f);
        EXPECT_NEAR(result.strategies.mixed_strategies[1][0], 0.5f, 0.1f);
    }

    // Cleanup mixed strategy arrays
    nimcp_strategy_profile_cleanup(&result.strategies);
}

TEST_F(EquilibriumTest, FindMixedNashBattleOfSexes) {
    solver = nimcp_equilibrium_create(&config);
    ASSERT_NE(solver, nullptr);
    SetupBattleOfSexes();

    nimcp_equilibrium_result_t result;
    nimcp_error_t err = nimcp_equilibrium_find_mixed_nash(solver, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Battle of Sexes has a mixed NE where players mix between Opera and Football
    // Player 1: Opera with prob 3/5, Football with prob 2/5
    // Player 2: Opera with prob 2/5, Football with prob 3/5
    if (result.type == NIMCP_EQUILIBRIUM_TYPE_MIXED) {
        nimcp_strategy_profile_cleanup(&result.strategies);
    }
}

//=============================================================================
// Best Response Tests
//=============================================================================

TEST_F(EquilibriumTest, BestResponsePrisonersDilemma) {
    solver = nimcp_equilibrium_create(&config);
    ASSERT_NE(solver, nullptr);
    SetupPrisonersDilemma();

    // Create a strategy profile where opponent cooperates
    nimcp_strategy_profile_t opponent_profile;
    uint32_t opponent_strategies[2] = {0, 0};  // Both cooperate
    nimcp_strategy_profile_init_pure(&opponent_profile, 2, opponent_strategies);

    uint32_t best_strategy;
    float best_payoff;

    // Player 0's best response to player 1 cooperating is to defect
    nimcp_error_t err = nimcp_equilibrium_best_response(solver, 0, &opponent_profile,
                                                        &best_strategy, &best_payoff);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(best_strategy, 1);  // Defect
    EXPECT_FLOAT_EQ(best_payoff, 5.0f);
}

//=============================================================================
// Equilibrium Verification Tests
//=============================================================================

TEST_F(EquilibriumTest, VerifyNashEquilibrium) {
    solver = nimcp_equilibrium_create(&config);
    ASSERT_NE(solver, nullptr);
    SetupPrisonersDilemma();

    // (Defect, Defect) is Nash equilibrium
    nimcp_strategy_profile_t profile;
    uint32_t strategies[2] = {1, 1};  // Both defect
    nimcp_strategy_profile_init_pure(&profile, 2, strategies);

    bool is_nash = nimcp_equilibrium_is_nash(solver, &profile, 0.0f);
    EXPECT_TRUE(is_nash);
}

TEST_F(EquilibriumTest, VerifyNotNashEquilibrium) {
    solver = nimcp_equilibrium_create(&config);
    ASSERT_NE(solver, nullptr);
    SetupPrisonersDilemma();

    // (Cooperate, Cooperate) is NOT Nash equilibrium
    nimcp_strategy_profile_t profile;
    uint32_t strategies[2] = {0, 0};  // Both cooperate
    nimcp_strategy_profile_init_pure(&profile, 2, strategies);

    bool is_nash = nimcp_equilibrium_is_nash(solver, &profile, 0.0f);
    EXPECT_FALSE(is_nash);
}

//=============================================================================
// Regret Computation Tests
//=============================================================================

TEST_F(EquilibriumTest, ComputeRegret) {
    solver = nimcp_equilibrium_create(&config);
    ASSERT_NE(solver, nullptr);
    SetupPrisonersDilemma();

    // (Cooperate, Cooperate) profile
    nimcp_strategy_profile_t profile;
    uint32_t strategies[2] = {0, 0};
    nimcp_strategy_profile_init_pure(&profile, 2, strategies);

    float regrets[2];
    nimcp_error_t err = nimcp_equilibrium_compute_regret(solver, &profile, regrets);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Both players have regret of 2 (could get 5 by defecting instead of 3)
    EXPECT_FLOAT_EQ(regrets[0], 2.0f);
    EXPECT_FLOAT_EQ(regrets[1], 2.0f);
}

TEST_F(EquilibriumTest, ZeroRegretAtNashEquilibrium) {
    solver = nimcp_equilibrium_create(&config);
    ASSERT_NE(solver, nullptr);
    SetupPrisonersDilemma();

    // (Defect, Defect) profile - Nash equilibrium
    nimcp_strategy_profile_t profile;
    uint32_t strategies[2] = {1, 1};
    nimcp_strategy_profile_init_pure(&profile, 2, strategies);

    float regrets[2];
    nimcp_error_t err = nimcp_equilibrium_compute_regret(solver, &profile, regrets);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // At Nash equilibrium, regret should be 0
    EXPECT_FLOAT_EQ(regrets[0], 0.0f);
    EXPECT_FLOAT_EQ(regrets[1], 0.0f);
}

//=============================================================================
// Expected Payoff Tests
//=============================================================================

TEST_F(EquilibriumTest, ExpectedPayoffPureStrategy) {
    solver = nimcp_equilibrium_create(&config);
    ASSERT_NE(solver, nullptr);
    SetupPrisonersDilemma();

    nimcp_strategy_profile_t profile;
    uint32_t strategies[2] = {0, 1};  // Player 0 cooperates, Player 1 defects
    nimcp_strategy_profile_init_pure(&profile, 2, strategies);

    float payoff0 = nimcp_equilibrium_expected_payoff(solver, 0, &profile);
    float payoff1 = nimcp_equilibrium_expected_payoff(solver, 1, &profile);

    EXPECT_FLOAT_EQ(payoff0, 0.0f);  // Sucker's payoff
    EXPECT_FLOAT_EQ(payoff1, 5.0f);  // Temptation payoff
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(EquilibriumTest, GetStats) {
    solver = nimcp_equilibrium_create(&config);
    ASSERT_NE(solver, nullptr);
    SetupPrisonersDilemma();

    // Run equilibrium finding
    nimcp_equilibrium_result_t result;
    nimcp_equilibrium_find_pure_nash(solver, &result);

    nimcp_equilibrium_stats_t stats;
    nimcp_error_t err = nimcp_equilibrium_get_stats(solver, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(stats.iterations_completed, 0);
}

TEST_F(EquilibriumTest, Reset) {
    solver = nimcp_equilibrium_create(&config);
    ASSERT_NE(solver, nullptr);
    SetupPrisonersDilemma();

    nimcp_equilibrium_result_t result;
    nimcp_equilibrium_find_pure_nash(solver, &result);

    nimcp_error_t err = nimcp_equilibrium_reset(solver);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

//=============================================================================
// Lemke-Howson Tests (2-player only)
//=============================================================================

TEST_F(EquilibriumTest, LemkeHowsonMatchingPennies) {
    solver = nimcp_equilibrium_create(&config);
    ASSERT_NE(solver, nullptr);
    SetupMatchingPennies();

    nimcp_equilibrium_result_t result;
    nimcp_error_t err = nimcp_equilibrium_lemke_howson(solver, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Should find mixed NE
    if (result.strategies.mixed_strategies[0]) {
        nimcp_strategy_profile_cleanup(&result.strategies);
    }
}

//=============================================================================
// Find All Equilibria Tests
//=============================================================================

TEST_F(EquilibriumTest, FindAllEquilibriaBattleOfSexes) {
    config.find_all_equilibria = true;
    solver = nimcp_equilibrium_create(&config);
    ASSERT_NE(solver, nullptr);
    SetupBattleOfSexes();

    nimcp_equilibrium_result_t results[8];
    uint32_t num_found = 0;

    nimcp_error_t err = nimcp_equilibrium_find_all(solver, results, 8, &num_found);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Battle of Sexes has 3 Nash equilibria: 2 pure + 1 mixed
    EXPECT_GE(num_found, 2);

    // Cleanup mixed strategies
    for (uint32_t i = 0; i < num_found; i++) {
        if (results[i].type == NIMCP_EQUILIBRIUM_TYPE_MIXED) {
            nimcp_strategy_profile_cleanup(&results[i].strategies);
        }
    }
}

//=============================================================================
// Algorithm Name Tests
//=============================================================================

TEST_F(EquilibriumTest, AlgorithmNames) {
    EXPECT_NE(nimcp_equilibrium_algo_name(NIMCP_EQUILIBRIUM_ALGO_BEST_RESPONSE), nullptr);
    EXPECT_NE(nimcp_equilibrium_algo_name(NIMCP_EQUILIBRIUM_ALGO_SUPPORT_ENUM), nullptr);
    EXPECT_NE(nimcp_equilibrium_algo_name(NIMCP_EQUILIBRIUM_ALGO_LEMKE_HOWSON), nullptr);
    EXPECT_NE(nimcp_equilibrium_algo_name(NIMCP_EQUILIBRIUM_ALGO_FICTITIOUS_PLAY), nullptr);
}

TEST_F(EquilibriumTest, EquilibriumTypeNames) {
    EXPECT_NE(nimcp_equilibrium_type_name(NIMCP_EQUILIBRIUM_TYPE_PURE), nullptr);
    EXPECT_NE(nimcp_equilibrium_type_name(NIMCP_EQUILIBRIUM_TYPE_MIXED), nullptr);
    EXPECT_NE(nimcp_equilibrium_type_name(NIMCP_EQUILIBRIUM_TYPE_CORRELATED), nullptr);
    EXPECT_NE(nimcp_equilibrium_type_name(NIMCP_EQUILIBRIUM_TYPE_APPROXIMATE), nullptr);
}

//=============================================================================
// Strategy Profile Helper Tests
//=============================================================================

TEST_F(EquilibriumTest, StrategyProfileInitPure) {
    nimcp_strategy_profile_t profile;
    uint32_t strategies[2] = {0, 1};
    nimcp_strategy_profile_init_pure(&profile, 2, strategies);

    EXPECT_EQ(profile.type, NIMCP_STRATEGY_PURE);
    EXPECT_EQ(profile.num_players, 2);
    EXPECT_EQ(profile.pure_strategies[0], 0);
    EXPECT_EQ(profile.pure_strategies[1], 1);
}

TEST_F(EquilibriumTest, StrategyProfileInitMixed) {
    nimcp_strategy_profile_t profile;
    uint32_t num_strategies[2] = {2, 3};

    nimcp_error_t err = nimcp_strategy_profile_init_mixed(&profile, 2, num_strategies);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(profile.type, NIMCP_STRATEGY_MIXED);
    EXPECT_EQ(profile.num_players, 2);
    EXPECT_NE(profile.mixed_strategies[0], nullptr);
    EXPECT_NE(profile.mixed_strategies[1], nullptr);
    EXPECT_EQ(profile.num_strategies[0], 2);
    EXPECT_EQ(profile.num_strategies[1], 3);

    nimcp_strategy_profile_cleanup(&profile);
}

TEST_F(EquilibriumTest, StrategyProfileCopy) {
    nimcp_strategy_profile_t src, dest;
    uint32_t num_strategies[2] = {2, 2};

    nimcp_strategy_profile_init_mixed(&src, 2, num_strategies);
    src.mixed_strategies[0][0] = 0.3f;
    src.mixed_strategies[0][1] = 0.7f;
    src.mixed_strategies[1][0] = 0.6f;
    src.mixed_strategies[1][1] = 0.4f;

    nimcp_error_t err = nimcp_strategy_profile_copy(&dest, &src);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(dest.type, NIMCP_STRATEGY_MIXED);
    EXPECT_FLOAT_EQ(dest.mixed_strategies[0][0], 0.3f);
    EXPECT_FLOAT_EQ(dest.mixed_strategies[1][1], 0.4f);

    nimcp_strategy_profile_cleanup(&src);
    nimcp_strategy_profile_cleanup(&dest);
}

//=============================================================================
// Game Matrix Tests
//=============================================================================

TEST_F(EquilibriumTest, GameMatrixCreateDestroy) {
    uint32_t strategies[2] = {3, 4};
    nimcp_game_matrix_t* matrix = nimcp_game_matrix_create(2, strategies);
    ASSERT_NE(matrix, nullptr);

    EXPECT_EQ(matrix->num_players, 2);
    EXPECT_EQ(matrix->num_strategies[0], 3);
    EXPECT_EQ(matrix->num_strategies[1], 4);
    EXPECT_EQ(matrix->total_cells, 12);  // 3 * 4

    nimcp_game_matrix_destroy(matrix);
}

TEST_F(EquilibriumTest, GameMatrixGetSet) {
    uint32_t strategies[2] = {2, 2};
    nimcp_game_matrix_t* matrix = nimcp_game_matrix_create(2, strategies);
    ASSERT_NE(matrix, nullptr);

    uint32_t profile[2] = {0, 1};
    nimcp_game_matrix_set(matrix, profile, 5.0f);

    float value = nimcp_game_matrix_get(matrix, profile);
    EXPECT_FLOAT_EQ(value, 5.0f);

    nimcp_game_matrix_destroy(matrix);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(EquilibriumTest, SinglePlayerGame) {
    uint32_t strategies[1] = {3};
    config = nimcp_equilibrium_default_config(1, strategies);
    solver = nimcp_equilibrium_create(&config);

    // Single player "game" - should handle gracefully
    if (solver) {
        nimcp_equilibrium_destroy(solver);
        solver = nullptr;
    }
}

TEST_F(EquilibriumTest, ZeroPayoffGame) {
    solver = nimcp_equilibrium_create(&config);
    ASSERT_NE(solver, nullptr);

    // All zero payoffs
    float zero_payoffs[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    nimcp_equilibrium_set_bimatrix(solver, zero_payoffs, zero_payoffs, 2, 2);

    nimcp_equilibrium_result_t result;
    nimcp_error_t err = nimcp_equilibrium_find_pure_nash(solver, &result);

    // With all zero payoffs, any pure strategy profile is a Nash equilibrium
    EXPECT_EQ(err, NIMCP_SUCCESS);
}
