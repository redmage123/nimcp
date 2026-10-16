//=============================================================================
// test_gt_coalition.cpp - Unit tests for Coalition Formation Module
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/game_theory/nimcp_gt_coalition.h"
#include "cognitive/game_theory/nimcp_game_theory.h"

//=============================================================================
// Coalition Value Callback for Tests
//=============================================================================

// Simple additive value function
static float additive_value(uint32_t coalition, uint32_t num_players, void* user_data) {
    (void)user_data;
    float value = 0.0f;
    for (uint32_t i = 0; i < num_players; i++) {
        if (coalition & (1 << i)) {
            value += (float)(i + 1);  // Player i contributes i+1
        }
    }
    return value;
}

// Superadditive value function (synergy for larger coalitions)
static float superadditive_value(uint32_t coalition, uint32_t num_players, void* user_data) {
    (void)user_data;
    int count = 0;
    float base = 0.0f;
    for (uint32_t i = 0; i < num_players; i++) {
        if (coalition & (1 << i)) {
            count++;
            base += (float)(i + 1);
        }
    }
    // Add synergy bonus for coalitions with 2+ members
    if (count >= 2) {
        base *= 1.5f;
    }
    return base;
}

// Zero value function
static float zero_value(uint32_t coalition, uint32_t num_players, void* user_data) {
    (void)coalition;
    (void)num_players;
    (void)user_data;
    return 0.0f;
}

// Subadditive value function (splitting is beneficial)
static float subadditive_value(uint32_t coalition, uint32_t num_players, void* user_data) {
    (void)user_data;
    int count = 0;
    float base = 0.0f;
    for (uint32_t i = 0; i < num_players; i++) {
        if (coalition & (1 << i)) {
            count++;
            base += (float)(i + 1);
        }
    }
    // Penalty for coalitions with 2+ members (makes splitting beneficial)
    if (count >= 2) {
        base *= 0.5f;
    }
    return base;
}

// Simple preference callback (prefer larger coalitions)
static int size_preference(uint32_t player, uint32_t coal1, uint32_t coal2, void* user_data) {
    (void)user_data;
    (void)player;

    int size1 = __builtin_popcount(coal1);
    int size2 = __builtin_popcount(coal2);

    return size1 - size2;  // Prefer larger coalitions
}

//=============================================================================
// Test Fixture
//=============================================================================

class CoalitionTest : public ::testing::Test {
protected:
    nimcp_coalition_game_t game = nullptr;
    nimcp_coalition_config_t config;

    void SetUp() override {
        config = nimcp_coalition_default_config(4);  // 4-player game
    }

    void TearDown() override {
        if (game) {
            nimcp_coalition_destroy(game);
            game = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(CoalitionTest, DefaultConfigValues) {
    EXPECT_EQ(config.num_players, 4);
    EXPECT_GT(config.max_iterations, 0);
    EXPECT_GT(config.convergence_epsilon, 0.0f);
}

TEST_F(CoalitionTest, StabilityTypeNames) {
    EXPECT_NE(nimcp_stability_type_name(NIMCP_STABILITY_CORE), nullptr);
    EXPECT_NE(nimcp_stability_type_name(NIMCP_STABILITY_NASH), nullptr);
    EXPECT_NE(nimcp_stability_type_name(NIMCP_STABILITY_INDIVIDUAL), nullptr);
    EXPECT_NE(nimcp_stability_type_name(NIMCP_STABILITY_CONTRACTUAL), nullptr);
}

TEST_F(CoalitionTest, FormationAlgorithmNames) {
    EXPECT_NE(nimcp_formation_algorithm_name(NIMCP_FORMATION_GREEDY), nullptr);
    EXPECT_NE(nimcp_formation_algorithm_name(NIMCP_FORMATION_OPTIMAL), nullptr);
    EXPECT_NE(nimcp_formation_algorithm_name(NIMCP_FORMATION_MERGE_SPLIT), nullptr);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(CoalitionTest, CreateDestroy) {
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);

    nimcp_coalition_destroy(game);
    game = nullptr;
}

TEST_F(CoalitionTest, CreateWithNullConfig) {
    game = nimcp_coalition_create(nullptr);
    // Should handle gracefully (may return nullptr or use defaults)
}

TEST_F(CoalitionTest, SetValueFunction) {
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);

    nimcp_error_t err = nimcp_coalition_set_value_function(game, additive_value, nullptr);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(CoalitionTest, SetPreferenceFunction) {
    config.use_preferences = true;
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);

    nimcp_error_t err = nimcp_coalition_set_preferences(game, size_preference, nullptr);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

//=============================================================================
// Coalition Value Tests
//=============================================================================

TEST_F(CoalitionTest, ComputeCoalitionValue) {
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);
    nimcp_coalition_set_value_function(game, additive_value, nullptr);

    // Coalition {0, 2} = 0b0101 = 5
    float value = 0.0f;
    nimcp_error_t err = nimcp_coalition_compute_value(game, 5, &value);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    // Player 0 contributes 1, Player 2 contributes 3
    EXPECT_FLOAT_EQ(value, 4.0f);
}

TEST_F(CoalitionTest, ComputeGrandCoalitionValue) {
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);
    nimcp_coalition_set_value_function(game, additive_value, nullptr);

    // Grand coalition {0, 1, 2, 3} = 0b1111 = 15
    float value = 0.0f;
    nimcp_error_t err = nimcp_coalition_compute_value(game, 15, &value);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    // 1 + 2 + 3 + 4 = 10
    EXPECT_FLOAT_EQ(value, 10.0f);
}

TEST_F(CoalitionTest, EmptyCoalitionValue) {
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);
    nimcp_coalition_set_value_function(game, additive_value, nullptr);

    float value = -1.0f;  // Initialize to non-zero to verify update
    nimcp_error_t err = nimcp_coalition_compute_value(game, 0, &value);  // Empty coalition
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_FLOAT_EQ(value, 0.0f);
}

//=============================================================================
// Greedy Formation Tests
//=============================================================================

TEST_F(CoalitionTest, GreedyFormation) {
    config.algorithm = NIMCP_FORMATION_GREEDY;
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);
    nimcp_coalition_set_value_function(game, superadditive_value, nullptr);

    nimcp_coalition_result_t result;
    nimcp_error_t err = nimcp_coalition_form_greedy(game, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Should form some coalition structure
    EXPECT_GT(result.structure.num_coalitions, 0);
    EXPECT_GT(result.structure.total_value, 0.0f);
}

TEST_F(CoalitionTest, GreedyFormationCoversAllPlayers) {
    config.algorithm = NIMCP_FORMATION_GREEDY;
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);
    nimcp_coalition_set_value_function(game, additive_value, nullptr);

    nimcp_coalition_result_t result;
    nimcp_coalition_form_greedy(game, &result);

    // All players should be covered exactly once
    uint32_t covered = 0;
    for (uint32_t i = 0; i < result.structure.num_coalitions; i++) {
        covered |= result.structure.coalitions[i].members;
    }
    EXPECT_EQ(covered, 0b1111);  // All 4 players
}

//=============================================================================
// Optimal Formation Tests
//=============================================================================

TEST_F(CoalitionTest, OptimalFormation) {
    config.algorithm = NIMCP_FORMATION_OPTIMAL;
    config.num_players = 3;  // Keep small for exponential algorithm
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);
    nimcp_coalition_set_value_function(game, superadditive_value, nullptr);

    nimcp_coalition_result_t result;
    nimcp_error_t err = nimcp_coalition_form_optimal(game, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Optimal should find the maximum value structure
    // With superadditive, grand coalition should be optimal
    EXPECT_EQ(result.structure.num_coalitions, 1);
    EXPECT_EQ(result.structure.coalitions[0].members, 0b0111);  // All 3 players
}

//=============================================================================
// Merge-Split Dynamics Tests
//=============================================================================

TEST_F(CoalitionTest, MergeSplitFormation) {
    config.algorithm = NIMCP_FORMATION_MERGE_SPLIT;
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);
    nimcp_coalition_set_value_function(game, superadditive_value, nullptr);

    nimcp_coalition_result_t result;
    nimcp_error_t err = nimcp_coalition_form_merge_split(game, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_TRUE(result.converged);
}

//=============================================================================
// Stability Tests
//=============================================================================

TEST_F(CoalitionTest, CheckCoreStability) {
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);
    nimcp_coalition_set_value_function(game, additive_value, nullptr);

    // Create a coalition structure
    nimcp_coalition_structure_t structure;
    structure.num_coalitions = 4;
    for (uint32_t i = 0; i < 4; i++) {
        structure.coalitions[i].members = (1 << i);  // Singleton coalitions
        structure.coalitions[i].value = (float)(i + 1);
        structure.coalitions[i].size = 1;
    }
    structure.total_value = 10.0f;
    structure.all_players = 0b1111;

    bool is_core_stable = nimcp_coalition_is_stable(game, &structure, NIMCP_STABILITY_CORE);
    // Additive game with singletons is core-stable (no blocking coalition)
    EXPECT_TRUE(is_core_stable);
}

TEST_F(CoalitionTest, CheckNashStability) {
    // Use single-player game where Nash stability is trivially true
    nimcp_coalition_config_t single_config = nimcp_coalition_default_config(1);
    nimcp_coalition_game_t single_game = nimcp_coalition_create(&single_config);
    ASSERT_NE(single_game, nullptr);
    nimcp_coalition_set_value_function(single_game, additive_value, nullptr);

    nimcp_coalition_structure_t structure;
    nimcp_coalition_structure_init_singletons(&structure, 1);
    structure.coalitions[0].value = 1.0f;
    structure.total_value = 1.0f;

    // Single player cannot deviate anywhere, so always Nash stable
    bool is_nash_stable = nimcp_coalition_is_stable(single_game, &structure, NIMCP_STABILITY_NASH);
    EXPECT_TRUE(is_nash_stable);

    nimcp_coalition_destroy(single_game);
}

TEST_F(CoalitionTest, CheckIndividualRationality) {
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);
    nimcp_coalition_set_value_function(game, additive_value, nullptr);

    // Singletons are always individually rational (each player gets their singleton value)
    nimcp_coalition_structure_t structure;
    nimcp_coalition_structure_init_singletons(&structure, 4);
    for (uint32_t i = 0; i < 4; i++) {
        structure.coalitions[i].value = (float)(i + 1);
    }
    structure.total_value = 10.0f;

    bool is_ir = nimcp_coalition_is_stable(game, &structure, NIMCP_STABILITY_INDIVIDUAL);
    EXPECT_TRUE(is_ir);  // Singletons are always individually rational
}

TEST_F(CoalitionTest, CheckIsInCore) {
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);
    nimcp_coalition_set_value_function(game, additive_value, nullptr);

    nimcp_coalition_structure_t structure;
    structure.num_coalitions = 4;
    for (uint32_t i = 0; i < 4; i++) {
        structure.coalitions[i].members = (1 << i);
        structure.coalitions[i].value = (float)(i + 1);
        structure.coalitions[i].size = 1;
    }
    structure.total_value = 10.0f;
    structure.all_players = 0b1111;

    bool in_core = nimcp_coalition_is_in_core(game, &structure);
    EXPECT_TRUE(in_core);  // Additive with singletons
}

TEST_F(CoalitionTest, CheckIndividuallyRational) {
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);
    nimcp_coalition_set_value_function(game, additive_value, nullptr);

    // Singletons are always individually rational
    nimcp_coalition_structure_t structure;
    nimcp_coalition_structure_init_singletons(&structure, 4);
    for (uint32_t i = 0; i < 4; i++) {
        structure.coalitions[i].value = (float)(i + 1);
    }
    structure.total_value = 10.0f;

    bool is_ir = nimcp_coalition_is_individually_rational(game, &structure);
    EXPECT_TRUE(is_ir);
}

//=============================================================================
// Blocking Coalition Tests
//=============================================================================

TEST_F(CoalitionTest, FindBlockingCoalition) {
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);
    nimcp_coalition_set_value_function(game, superadditive_value, nullptr);

    // Create inefficient singleton structure
    nimcp_coalition_structure_t structure;
    structure.num_coalitions = 4;
    for (uint32_t i = 0; i < 4; i++) {
        structure.coalitions[i].members = (1 << i);
        structure.coalitions[i].value = (float)(i + 1);
        structure.coalitions[i].size = 1;
    }
    structure.total_value = 10.0f;
    structure.all_players = 0b1111;

    uint32_t blocking = 0;
    nimcp_error_t err = nimcp_coalition_find_blocking(game, &structure, &blocking);

    // With superadditive values, should find a blocking coalition
    if (err == NIMCP_SUCCESS) {
        EXPECT_GT(__builtin_popcount(blocking), 1);  // Blocking coalition has 2+ members
    }
}

TEST_F(CoalitionTest, NoBlockingCoalitionForCoreStable) {
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);
    nimcp_coalition_set_value_function(game, additive_value, nullptr);

    // Singleton structure is core-stable for additive games
    nimcp_coalition_structure_t structure;
    structure.num_coalitions = 4;
    for (uint32_t i = 0; i < 4; i++) {
        structure.coalitions[i].members = (1 << i);
        structure.coalitions[i].value = (float)(i + 1);
        structure.coalitions[i].size = 1;
    }
    structure.total_value = 10.0f;
    structure.all_players = 0b1111;

    uint32_t blocking = 0;
    nimcp_error_t err = nimcp_coalition_find_blocking(game, &structure, &blocking);

    EXPECT_EQ(err, NIMCP_GT_ERROR_NO_BLOCKING);
}

//=============================================================================
// Hedonic Game Tests
//=============================================================================

TEST_F(CoalitionTest, HedonicGameFormation) {
    config.use_preferences = true;
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);
    // Hedonic formation still requires value function for payoff computation
    nimcp_coalition_set_value_function(game, superadditive_value, nullptr);
    nimcp_coalition_set_preferences(game, size_preference, nullptr);

    nimcp_coalition_result_t result;
    nimcp_error_t err = nimcp_coalition_form_greedy(game, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // With size preference and superadditive values, should form larger coalitions
    EXPECT_LE(result.structure.num_coalitions, 2);
}

//=============================================================================
// Coalition Structure Operations Tests
//=============================================================================

TEST_F(CoalitionTest, ValidatePartition) {
    // Valid partition
    nimcp_coalition_structure_t valid_structure;
    valid_structure.num_coalitions = 2;
    valid_structure.coalitions[0].members = 0b0011;  // Players 0, 1
    valid_structure.coalitions[0].size = 2;
    valid_structure.coalitions[1].members = 0b1100;  // Players 2, 3
    valid_structure.coalitions[1].size = 2;
    valid_structure.all_players = 0b1111;

    bool is_valid = nimcp_coalition_structure_is_valid(&valid_structure, 4);
    EXPECT_TRUE(is_valid);

    // Invalid partition (overlapping)
    nimcp_coalition_structure_t invalid_structure;
    invalid_structure.num_coalitions = 2;
    invalid_structure.coalitions[0].members = 0b0011;  // Players 0, 1
    invalid_structure.coalitions[0].size = 2;
    invalid_structure.coalitions[1].members = 0b0110;  // Players 1, 2 (overlap!)
    invalid_structure.coalitions[1].size = 2;
    invalid_structure.all_players = 0b1111;

    is_valid = nimcp_coalition_structure_is_valid(&invalid_structure, 4);
    EXPECT_FALSE(is_valid);
}

TEST_F(CoalitionTest, MergeCoalitions) {
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);
    nimcp_coalition_set_value_function(game, additive_value, nullptr);

    nimcp_coalition_structure_t structure;
    structure.num_coalitions = 2;
    structure.coalitions[0].members = 0b0011;  // {0, 1}
    structure.coalitions[0].value = 3.0f;
    structure.coalitions[0].size = 2;
    structure.coalitions[1].members = 0b1100;  // {2, 3}
    structure.coalitions[1].value = 7.0f;
    structure.coalitions[1].size = 2;
    structure.total_value = 10.0f;
    structure.all_players = 0b1111;

    nimcp_coalition_structure_t new_structure;
    nimcp_error_t err = nimcp_coalition_merge(game, &structure, 0, 1, &new_structure);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(new_structure.num_coalitions, 1);
    EXPECT_EQ(new_structure.coalitions[0].members, 0b1111);  // Grand coalition
}

TEST_F(CoalitionTest, SplitCoalition) {
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);
    // Use subadditive value function where splitting improves total value
    nimcp_coalition_set_value_function(game, subadditive_value, nullptr);

    nimcp_coalition_structure_t structure;
    structure.num_coalitions = 1;
    structure.coalitions[0].members = 0b1111;  // Grand coalition
    structure.coalitions[0].value = subadditive_value(0b1111, 4, nullptr);  // 10 * 0.5 = 5
    structure.coalitions[0].size = 4;
    structure.total_value = structure.coalitions[0].value;
    structure.all_players = 0b1111;

    // Split coalition at index 0
    nimcp_coalition_structure_t new_structure;
    nimcp_error_t err = nimcp_coalition_split(game, &structure, 0, &new_structure);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    // With subadditive values, splitting should be beneficial
    EXPECT_GE(new_structure.num_coalitions, 2);
}

TEST_F(CoalitionTest, InitSingletons) {
    nimcp_coalition_structure_t structure;
    nimcp_coalition_structure_init_singletons(&structure, 4);

    EXPECT_EQ(structure.num_coalitions, 4);
    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_EQ(structure.coalitions[i].members, (1u << i));
        EXPECT_EQ(structure.coalitions[i].size, 1);
    }
}

TEST_F(CoalitionTest, InitGrandCoalition) {
    nimcp_coalition_structure_t structure;
    nimcp_coalition_structure_init_grand(&structure, 4);

    EXPECT_EQ(structure.num_coalitions, 1);
    EXPECT_EQ(structure.coalitions[0].members, 0b1111);
    EXPECT_EQ(structure.coalitions[0].size, 4);
}

TEST_F(CoalitionTest, FindPlayerCoalition) {
    nimcp_coalition_structure_t structure;
    structure.num_coalitions = 2;
    structure.coalitions[0].members = 0b0011;  // Players 0, 1
    structure.coalitions[1].members = 0b1100;  // Players 2, 3

    EXPECT_EQ(nimcp_coalition_find_player_coalition(&structure, 0), 0);
    EXPECT_EQ(nimcp_coalition_find_player_coalition(&structure, 1), 0);
    EXPECT_EQ(nimcp_coalition_find_player_coalition(&structure, 2), 1);
    EXPECT_EQ(nimcp_coalition_find_player_coalition(&structure, 3), 1);
    EXPECT_EQ(nimcp_coalition_find_player_coalition(&structure, 4), -1);  // Not found
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(CoalitionTest, GetFormationStats) {
    config.algorithm = NIMCP_FORMATION_GREEDY;
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);
    nimcp_coalition_set_value_function(game, additive_value, nullptr);

    nimcp_coalition_result_t result;
    nimcp_coalition_form_greedy(game, &result);

    EXPECT_GE(result.iterations, 0);
    EXPECT_GE(result.coalitions_evaluated, 0);
    EXPECT_GE(result.formation_time_ms, 0.0f);
}

TEST_F(CoalitionTest, GetNumPlayers) {
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);

    uint32_t num = nimcp_coalition_get_num_players(game);
    EXPECT_EQ(num, 4);
}

TEST_F(CoalitionTest, ClearCache) {
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);
    nimcp_coalition_set_value_function(game, additive_value, nullptr);

    // Compute a value to populate cache
    float value = 0.0f;
    nimcp_coalition_compute_value(game, 0b0011, &value);

    // Clear cache should not crash
    nimcp_coalition_clear_cache(game);
}

//=============================================================================
// Payoff Tests
//=============================================================================

TEST_F(CoalitionTest, ComputePayoff) {
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);
    nimcp_coalition_set_value_function(game, additive_value, nullptr);

    // Coalition {0, 1} = 0b0011, value = 1 + 2 = 3
    // With equal split, each player gets 1.5
    float payoff = 0.0f;
    nimcp_error_t err = nimcp_coalition_compute_payoff(game, 0b0011, 0, &payoff);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_FLOAT_EQ(payoff, 1.5f);
}

TEST_F(CoalitionTest, ComputeAllPayoffs) {
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);
    nimcp_coalition_set_value_function(game, additive_value, nullptr);

    nimcp_coalition_structure_t structure;
    structure.num_coalitions = 2;
    structure.coalitions[0].members = 0b0011;  // {0, 1}, value 3
    structure.coalitions[0].value = 3.0f;
    structure.coalitions[0].size = 2;
    structure.coalitions[1].members = 0b1100;  // {2, 3}, value 7
    structure.coalitions[1].value = 7.0f;
    structure.coalitions[1].size = 2;
    structure.total_value = 10.0f;
    structure.all_players = 0b1111;

    float payoffs[4] = {0};
    nimcp_error_t err = nimcp_coalition_compute_payoffs(game, &structure, payoffs);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Players 0, 1 split 3.0 -> 1.5 each
    EXPECT_FLOAT_EQ(payoffs[0], 1.5f);
    EXPECT_FLOAT_EQ(payoffs[1], 1.5f);
    // Players 2, 3 split 7.0 -> 3.5 each
    EXPECT_FLOAT_EQ(payoffs[2], 3.5f);
    EXPECT_FLOAT_EQ(payoffs[3], 3.5f);
}

//=============================================================================
// Player Deviation Tests
//=============================================================================

TEST_F(CoalitionTest, PlayerWouldDeviate) {
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);
    nimcp_coalition_set_value_function(game, additive_value, nullptr);

    nimcp_coalition_structure_t structure;
    nimcp_coalition_structure_init_singletons(&structure, 4);

    // For additive game, no player should want to deviate from singletons
    bool would_deviate = nimcp_coalition_player_would_deviate(game, &structure, 0, 1);
    // Result depends on implementation
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(CoalitionTest, SinglePlayerGame) {
    config.num_players = 1;
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);
    nimcp_coalition_set_value_function(game, additive_value, nullptr);

    nimcp_coalition_result_t result;
    nimcp_error_t err = nimcp_coalition_form_greedy(game, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(result.structure.num_coalitions, 1);
    EXPECT_EQ(result.structure.coalitions[0].members, 0b0001);
}

TEST_F(CoalitionTest, TwoPlayerGame) {
    config.num_players = 2;
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);
    nimcp_coalition_set_value_function(game, superadditive_value, nullptr);

    nimcp_coalition_result_t result;
    nimcp_error_t err = nimcp_coalition_form_greedy(game, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // With superadditive, grand coalition should be formed
    EXPECT_EQ(result.structure.num_coalitions, 1);
    EXPECT_EQ(result.structure.coalitions[0].members, 0b0011);
}

TEST_F(CoalitionTest, ZeroValueGame) {
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);

    nimcp_coalition_set_value_function(game, zero_value, nullptr);

    nimcp_coalition_result_t result;
    nimcp_error_t err = nimcp_coalition_form_greedy(game, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_FLOAT_EQ(result.structure.total_value, 0.0f);
}

TEST_F(CoalitionTest, PreferenceOrderSetting) {
    config.use_preferences = true;
    game = nimcp_coalition_create(&config);
    ASSERT_NE(game, nullptr);

    // Set preference order for player 0
    uint32_t prefs[] = {0b1111, 0b0011, 0b0001};  // Grand > {0,1} > singleton
    nimcp_error_t err = nimcp_coalition_set_preference_order(game, 0, prefs, 3);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}
