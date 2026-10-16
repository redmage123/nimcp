/**
 * @file test_credit_assignment_regression.cpp
 * @brief Regression tests for Shapley Value and Credit Assignment
 *
 * Tests verify:
 * - Shapley value efficiency axiom (credits sum to grand coalition value)
 * - Shapley value symmetry axiom
 * - Banzhaf index correctness
 * - Core membership checking
 * - Performance and numerical stability
 *
 * @version 1.0.0
 * @date 2025-12-27
 */

#include <gtest/gtest.h>
#include <chrono>
#include <cmath>
#include <random>
#include <vector>
#include <algorithm>
#include <limits>
#include <functional>

// Headers have their own extern "C" guards
#include "cognitive/game_theory/nimcp_credit_assignment.h"
#include "cognitive/game_theory/nimcp_game_theory.h"

//=============================================================================
// Test Characteristic Functions
//=============================================================================

/**
 * @brief Simple additive game: v(S) = sum of player values
 *
 * Each player has value i+1, so:
 * - v({0}) = 1
 * - v({1}) = 2
 * - v({0,1}) = 3
 * - etc.
 */
static float additive_game_value(uint32_t coalition, uint32_t num_players, void* user_data) {
    (void)user_data;
    float sum = 0.0f;
    for (uint32_t i = 0; i < num_players; i++) {
        if (coalition & (1u << i)) {
            sum += (float)(i + 1);
        }
    }
    return sum;
}

/**
 * @brief Superadditive game with synergies
 *
 * v(S) = sum of player values + synergy bonus for larger coalitions
 */
static float synergy_game_value(uint32_t coalition, uint32_t num_players, void* user_data) {
    (void)user_data;
    float sum = 0.0f;
    uint32_t size = 0;

    for (uint32_t i = 0; i < num_players; i++) {
        if (coalition & (1u << i)) {
            sum += (float)(i + 1);
            size++;
        }
    }

    // Synergy bonus: extra value for larger coalitions
    if (size >= 2) {
        sum += (float)(size * size);
    }

    return sum;
}

/**
 * @brief Voting game: majority wins
 *
 * v(S) = 1 if |S| > n/2, else 0
 */
static float voting_game_value(uint32_t coalition, uint32_t num_players, void* user_data) {
    (void)user_data;
    uint32_t size = 0;

    for (uint32_t i = 0; i < num_players; i++) {
        if (coalition & (1u << i)) {
            size++;
        }
    }

    return (size > num_players / 2) ? 1.0f : 0.0f;
}

/**
 * @brief Weighted voting game
 *
 * Player i has weight (i+1), quota is half of total weight
 */
static float weighted_voting_value(uint32_t coalition, uint32_t num_players, void* user_data) {
    (void)user_data;
    float weight_sum = 0.0f;
    float total_weight = 0.0f;

    for (uint32_t i = 0; i < num_players; i++) {
        float w = (float)(i + 1);
        total_weight += w;
        if (coalition & (1u << i)) {
            weight_sum += w;
        }
    }

    float quota = total_weight / 2.0f;
    return (weight_sum > quota) ? 1.0f : 0.0f;
}

/**
 * @brief Symmetric game: all players identical
 *
 * v(S) = |S|^2
 */
static float symmetric_game_value(uint32_t coalition, uint32_t num_players, void* user_data) {
    (void)user_data;
    uint32_t size = 0;

    for (uint32_t i = 0; i < num_players; i++) {
        if (coalition & (1u << i)) {
            size++;
        }
    }

    return (float)(size * size);
}

/**
 * @brief Null player game: one player contributes nothing
 *
 * Player 0 is a null player
 */
static float null_player_game_value(uint32_t coalition, uint32_t num_players, void* user_data) {
    (void)user_data;
    float sum = 0.0f;

    // Skip player 0 (null player)
    for (uint32_t i = 1; i < num_players; i++) {
        if (coalition & (1u << i)) {
            sum += (float)(i + 1);
        }
    }

    return sum;
}

//=============================================================================
// Test Fixture
//=============================================================================

class CreditAssignmentRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = nimcp_credit_default_config(4);
    }

    void TearDown() override {
        // Cleanup handled by individual tests
    }

    nimcp_credit_config_t config_;

    // Helper to create credit system
    nimcp_credit_system_t create_credit_system(uint32_t num_players = 4) {
        config_ = nimcp_credit_default_config(num_players);
        return nimcp_credit_create(&config_);
    }
};

//=============================================================================
// REG-CRE-001: Shapley Value Efficiency Axiom
//=============================================================================

TEST_F(CreditAssignmentRegressionTest, REG_CRE_001_ShapleyEfficiencyAdditive) {
    nimcp_credit_system_t system = create_credit_system(4);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_compute_shapley(
        system, additive_game_value, nullptr, &result
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Sum of credits should equal grand coalition value
    float credit_sum = 0.0f;
    for (uint32_t i = 0; i < 4; i++) {
        credit_sum += result.credits[i];
    }

    // Grand coalition value for additive game: 1+2+3+4 = 10
    EXPECT_NEAR(credit_sum, result.total_value, 0.001f);
    EXPECT_NEAR(credit_sum, 10.0f, 0.001f);

    // Efficiency error should be near zero
    EXPECT_LT(result.efficiency_error, 0.001f);

    nimcp_credit_destroy(system);
}

TEST_F(CreditAssignmentRegressionTest, REG_CRE_002_ShapleyEfficiencySynergy) {
    nimcp_credit_system_t system = create_credit_system(4);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_compute_shapley(
        system, synergy_game_value, nullptr, &result
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    float credit_sum = 0.0f;
    for (uint32_t i = 0; i < 4; i++) {
        credit_sum += result.credits[i];
    }

    // Grand coalition: 1+2+3+4 + 4^2 = 10 + 16 = 26
    EXPECT_NEAR(credit_sum, result.total_value, 0.001f);
    EXPECT_NEAR(credit_sum, 26.0f, 0.001f);

    nimcp_credit_destroy(system);
}

TEST_F(CreditAssignmentRegressionTest, REG_CRE_003_ShapleyEfficiencyVoting) {
    nimcp_credit_system_t system = create_credit_system(5);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_compute_shapley(
        system, voting_game_value, nullptr, &result
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    float credit_sum = 0.0f;
    for (uint32_t i = 0; i < 5; i++) {
        credit_sum += result.credits[i];
    }

    // Grand coalition value is 1 (majority achieved)
    EXPECT_NEAR(credit_sum, 1.0f, 0.001f);
    EXPECT_NEAR(credit_sum, result.total_value, 0.001f);

    nimcp_credit_destroy(system);
}

TEST_F(CreditAssignmentRegressionTest, REG_CRE_004_ShapleyEfficiencyManyScenarios) {
    std::mt19937 rng(42);

    for (int scenario = 0; scenario < 20; scenario++) {
        uint32_t num_players = 3 + (scenario % 5);  // 3 to 7 players
        nimcp_credit_system_t system = create_credit_system(num_players);
        ASSERT_NE(system, nullptr);

        nimcp_credit_result_t result;
        nimcp_error_t err = nimcp_credit_compute_shapley(
            system, synergy_game_value, nullptr, &result
        );
        ASSERT_EQ(err, NIMCP_SUCCESS);

        float credit_sum = 0.0f;
        for (uint32_t i = 0; i < num_players; i++) {
            credit_sum += result.credits[i];
        }

        EXPECT_NEAR(credit_sum, result.total_value, 0.01f)
            << "Scenario " << scenario << " failed efficiency axiom";

        nimcp_credit_destroy(system);
    }
}

//=============================================================================
// REG-CRE-010: Shapley Value Symmetry Axiom
//=============================================================================

TEST_F(CreditAssignmentRegressionTest, REG_CRE_010_ShapleySymmetryOnSymmetricGame) {
    nimcp_credit_system_t system = create_credit_system(4);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_compute_shapley(
        system, symmetric_game_value, nullptr, &result
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // All players should get equal credit in symmetric game
    float first_credit = result.credits[0];
    for (uint32_t i = 1; i < 4; i++) {
        EXPECT_NEAR(result.credits[i], first_credit, 0.001f)
            << "Player " << i << " has different credit in symmetric game";
    }

    // Symmetry error should be near zero
    EXPECT_LT(result.symmetry_error, 0.001f);

    nimcp_credit_destroy(system);
}

TEST_F(CreditAssignmentRegressionTest, REG_CRE_011_ShapleyVotingSymmetry) {
    // In simple majority voting, all players are symmetric
    nimcp_credit_system_t system = create_credit_system(5);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_compute_shapley(
        system, voting_game_value, nullptr, &result
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // All players should have equal power
    float expected = 1.0f / 5.0f;
    for (uint32_t i = 0; i < 5; i++) {
        EXPECT_NEAR(result.credits[i], expected, 0.01f)
            << "Player " << i << " has non-symmetric credit";
    }

    nimcp_credit_destroy(system);
}

//=============================================================================
// REG-CRE-020: Shapley Value Null Player Axiom
//=============================================================================

TEST_F(CreditAssignmentRegressionTest, REG_CRE_020_ShapleyNullPlayerZeroCredit) {
    nimcp_credit_system_t system = create_credit_system(4);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_compute_shapley(
        system, null_player_game_value, nullptr, &result
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Player 0 is null player, should get zero credit
    EXPECT_NEAR(result.credits[0], 0.0f, 0.001f)
        << "Null player should have zero credit";

    nimcp_credit_destroy(system);
}

//=============================================================================
// REG-CRE-030: Shapley Value for Known Games
//=============================================================================

TEST_F(CreditAssignmentRegressionTest, REG_CRE_030_ShapleyAdditiveGameKnown) {
    nimcp_credit_system_t system = create_credit_system(4);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_compute_shapley(
        system, additive_game_value, nullptr, &result
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // In additive game, Shapley value equals individual value
    EXPECT_NEAR(result.credits[0], 1.0f, 0.001f);
    EXPECT_NEAR(result.credits[1], 2.0f, 0.001f);
    EXPECT_NEAR(result.credits[2], 3.0f, 0.001f);
    EXPECT_NEAR(result.credits[3], 4.0f, 0.001f);

    nimcp_credit_destroy(system);
}

TEST_F(CreditAssignmentRegressionTest, REG_CRE_031_ShapleyWeightedVotingKnown) {
    // 3-player weighted voting: weights [1,2,3], quota > 3
    // Winning coalitions: {0,1,2}, {1,2}, {0,2}
    // Shapley values: Player 0 = 1/6, Player 1 = 1/6, Player 2 = 4/6
    // Note: Players 0 and 1 are equal because both are equally pivotal
    nimcp_credit_system_t system = create_credit_system(3);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_compute_shapley(
        system, weighted_voting_value, nullptr, &result
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Player 2 (highest weight) has highest Shapley value
    EXPECT_GT(result.credits[2], result.credits[1]);
    // Players 0 and 1 have equal Shapley values in this game
    EXPECT_NEAR(result.credits[0], result.credits[1], 0.001f);
    // Verify approximate values
    EXPECT_NEAR(result.credits[0], 1.0f/6.0f, 0.01f);
    EXPECT_NEAR(result.credits[1], 1.0f/6.0f, 0.01f);
    EXPECT_NEAR(result.credits[2], 4.0f/6.0f, 0.01f);

    nimcp_credit_destroy(system);
}

//=============================================================================
// REG-CRE-040: Monte Carlo Shapley Approximation
//=============================================================================

TEST_F(CreditAssignmentRegressionTest, REG_CRE_040_ApproximateShapleyConverges) {
    nimcp_credit_system_t system = create_credit_system(6);
    ASSERT_NE(system, nullptr);

    // Get exact Shapley first
    nimcp_credit_result_t exact;
    nimcp_error_t err = nimcp_credit_compute_shapley(
        system, additive_game_value, nullptr, &exact
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Approximate with many samples
    nimcp_credit_result_t approx;
    err = nimcp_credit_approximate_shapley(
        system, additive_game_value, nullptr, 10000, &approx
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Should be close to exact
    for (uint32_t i = 0; i < 6; i++) {
        EXPECT_NEAR(approx.credits[i], exact.credits[i], 0.1f)
            << "Player " << i << " approximation too far from exact";
    }

    nimcp_credit_destroy(system);
}

TEST_F(CreditAssignmentRegressionTest, REG_CRE_041_ApproximateShapleyEfficiency) {
    nimcp_credit_system_t system = create_credit_system(8);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_approximate_shapley(
        system, synergy_game_value, nullptr, 5000, &result
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Sum should approximately equal grand coalition value
    float credit_sum = 0.0f;
    for (uint32_t i = 0; i < 8; i++) {
        credit_sum += result.credits[i];
    }

    EXPECT_NEAR(credit_sum, result.total_value, 1.0f);

    nimcp_credit_destroy(system);
}

//=============================================================================
// REG-CRE-050: Banzhaf Index
//=============================================================================

TEST_F(CreditAssignmentRegressionTest, REG_CRE_050_BanzhafVotingGame) {
    nimcp_credit_system_t system = create_credit_system(5);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_compute_banzhaf(
        system, voting_game_value, nullptr, &result
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // In symmetric voting, all players have equal Banzhaf index
    float first = result.credits[0];
    for (uint32_t i = 1; i < 5; i++) {
        EXPECT_NEAR(result.credits[i], first, 0.01f);
    }

    nimcp_credit_destroy(system);
}

TEST_F(CreditAssignmentRegressionTest, REG_CRE_051_BanzhafWeightedVoting) {
    nimcp_credit_system_t system = create_credit_system(4);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_compute_banzhaf(
        system, weighted_voting_value, nullptr, &result
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Higher-weight players should have higher Banzhaf index
    EXPECT_GE(result.credits[3], result.credits[0]);

    nimcp_credit_destroy(system);
}

//=============================================================================
// REG-CRE-060: Core Membership
//=============================================================================

TEST_F(CreditAssignmentRegressionTest, REG_CRE_060_ShapleyInCoreAdditive) {
    nimcp_credit_system_t system = create_credit_system(4);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_compute_shapley(
        system, additive_game_value, nullptr, &result
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // For additive games, Shapley value is always in the core
    bool in_core = nimcp_credit_is_in_core(
        system, result.credits, additive_game_value, nullptr
    );
    EXPECT_TRUE(in_core);

    nimcp_credit_destroy(system);
}

TEST_F(CreditAssignmentRegressionTest, REG_CRE_061_CoreMembershipCheck) {
    nimcp_credit_system_t system = create_credit_system(3);
    ASSERT_NE(system, nullptr);

    // Test an allocation that should be in the core
    float good_alloc[3] = {1.0f, 2.0f, 3.0f};  // Equals individual values
    bool in_core = nimcp_credit_is_in_core(
        system, good_alloc, additive_game_value, nullptr
    );
    EXPECT_TRUE(in_core);

    // Test an allocation that violates core (someone gets less than solo value)
    float bad_alloc[3] = {0.0f, 2.0f, 4.0f};  // Player 0 gets less than v({0})=1
    in_core = nimcp_credit_is_in_core(
        system, bad_alloc, additive_game_value, nullptr
    );
    EXPECT_FALSE(in_core);

    nimcp_credit_destroy(system);
}

//=============================================================================
// REG-CRE-070: Equal Split Baseline
//=============================================================================

TEST_F(CreditAssignmentRegressionTest, REG_CRE_070_EqualSplitCorrectness) {
    nimcp_credit_system_t system = create_credit_system(4);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_compute_equal_split(
        system, synergy_game_value, nullptr, &result
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // All players should get equal share
    float expected = result.total_value / 4.0f;
    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_NEAR(result.credits[i], expected, 0.001f);
    }

    nimcp_credit_destroy(system);
}

//=============================================================================
// REG-CRE-080: Performance Tests
//=============================================================================

TEST_F(CreditAssignmentRegressionTest, REG_CRE_080_ShapleyComputationTime) {
    // Shapley is O(2^n), test with n=10 (1024 coalitions)
    nimcp_credit_system_t system = create_credit_system(10);
    ASSERT_NE(system, nullptr);

    auto start = std::chrono::high_resolution_clock::now();

    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_compute_shapley(
        system, additive_game_value, nullptr, &result
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in under 1 second for 10 players
    EXPECT_LT(duration.count(), 1000)
        << "Shapley for 10 players took " << duration.count() << "ms";

    nimcp_credit_destroy(system);
}

TEST_F(CreditAssignmentRegressionTest, REG_CRE_081_ApproximateShapleyScalability) {
    // Approximate Shapley should scale to larger n
    nimcp_credit_system_t system = create_credit_system(20);
    ASSERT_NE(system, nullptr);

    auto start = std::chrono::high_resolution_clock::now();

    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_approximate_shapley(
        system, additive_game_value, nullptr, 1000, &result
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete quickly even for 20 players with Monte Carlo
    EXPECT_LT(duration.count(), 500)
        << "Approximate Shapley for 20 players took " << duration.count() << "ms";

    nimcp_credit_destroy(system);
}

TEST_F(CreditAssignmentRegressionTest, REG_CRE_082_SinglePlayerShapleyTime) {
    nimcp_credit_system_t system = create_credit_system(12);
    ASSERT_NE(system, nullptr);

    auto start = std::chrono::high_resolution_clock::now();

    float credit;
    for (uint32_t i = 0; i < 12; i++) {
        nimcp_credit_compute_shapley_single(
            system, i, additive_game_value, nullptr, &credit
        );
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Computing individual values should be efficient
    EXPECT_LT(duration.count(), 2000);

    nimcp_credit_destroy(system);
}

//=============================================================================
// REG-CRE-090: Numerical Stability
//=============================================================================

static float tiny_value_game(uint32_t coalition, uint32_t num_players, void* user_data) {
    (void)user_data;
    float sum = 0.0f;
    for (uint32_t i = 0; i < num_players; i++) {
        if (coalition & (1u << i)) {
            sum += 1e-10f * (float)(i + 1);
        }
    }
    return sum;
}

static float huge_value_game(uint32_t coalition, uint32_t num_players, void* user_data) {
    (void)user_data;
    float sum = 0.0f;
    for (uint32_t i = 0; i < num_players; i++) {
        if (coalition & (1u << i)) {
            sum += 1e10f * (float)(i + 1);
        }
    }
    return sum;
}

TEST_F(CreditAssignmentRegressionTest, REG_CRE_090_VerySmallValues) {
    nimcp_credit_system_t system = create_credit_system(4);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_compute_shapley(
        system, tiny_value_game, nullptr, &result
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_FALSE(std::isnan(result.credits[i]));
        EXPECT_FALSE(std::isinf(result.credits[i]));
    }

    // Efficiency should still hold
    float sum = 0.0f;
    for (uint32_t i = 0; i < 4; i++) {
        sum += result.credits[i];
    }
    EXPECT_NEAR(sum, result.total_value, result.total_value * 0.01f);

    nimcp_credit_destroy(system);
}

TEST_F(CreditAssignmentRegressionTest, REG_CRE_091_VeryLargeValues) {
    nimcp_credit_system_t system = create_credit_system(4);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_compute_shapley(
        system, huge_value_game, nullptr, &result
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_FALSE(std::isnan(result.credits[i]));
        EXPECT_FALSE(std::isinf(result.credits[i]));
    }

    nimcp_credit_destroy(system);
}

TEST_F(CreditAssignmentRegressionTest, REG_CRE_092_ZeroCoalitionValues) {
    // Game where all coalitions have zero value
    auto zero_game = [](uint32_t coalition, uint32_t num_players, void* user_data) -> float {
        (void)coalition;
        (void)num_players;
        (void)user_data;
        return 0.0f;
    };

    nimcp_credit_system_t system = create_credit_system(4);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_compute_shapley(
        system, zero_game, nullptr, &result
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // All credits should be zero
    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(result.credits[i], 0.0f);
    }

    nimcp_credit_destroy(system);
}

//=============================================================================
// REG-CRE-100: Axiom Verification
//=============================================================================

TEST_F(CreditAssignmentRegressionTest, REG_CRE_100_VerifyAxiomsAdditive) {
    nimcp_credit_system_t system = create_credit_system(4);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_credit_compute_shapley(system, additive_game_value, nullptr, &result);

    float efficiency_error, symmetry_error;
    bool axioms_ok = nimcp_credit_verify_axioms(
        &result, additive_game_value, nullptr, &efficiency_error, &symmetry_error
    );

    EXPECT_TRUE(axioms_ok);
    EXPECT_LT(efficiency_error, 0.001f);

    nimcp_credit_destroy(system);
}

TEST_F(CreditAssignmentRegressionTest, REG_CRE_101_VerifyAxiomsSymmetric) {
    nimcp_credit_system_t system = create_credit_system(4);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_credit_compute_shapley(system, symmetric_game_value, nullptr, &result);

    float efficiency_error, symmetry_error;
    bool axioms_ok = nimcp_credit_verify_axioms(
        &result, symmetric_game_value, nullptr, &efficiency_error, &symmetry_error
    );

    EXPECT_TRUE(axioms_ok);
    EXPECT_LT(symmetry_error, 0.001f);

    nimcp_credit_destroy(system);
}

//=============================================================================
// REG-CRE-110: Null Safety
//=============================================================================

TEST_F(CreditAssignmentRegressionTest, REG_CRE_110_NullHandling) {
    // Create with null config
    nimcp_credit_system_t system = nimcp_credit_create(nullptr);
    if (system != nullptr) {
        nimcp_credit_destroy(system);
    }

    // Compute with null system
    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_compute_shapley(
        nullptr, additive_game_value, nullptr, &result
    );
    EXPECT_NE(err, NIMCP_SUCCESS);

    // Compute with null value function
    system = create_credit_system(4);
    ASSERT_NE(system, nullptr);
    err = nimcp_credit_compute_shapley(system, nullptr, nullptr, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);

    // Compute with null result
    err = nimcp_credit_compute_shapley(system, additive_game_value, nullptr, nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS);

    nimcp_credit_destroy(system);

    // Destroy null should not crash
    nimcp_credit_destroy(nullptr);
}

//=============================================================================
// REG-CRE-120: Query Functions
//=============================================================================

TEST_F(CreditAssignmentRegressionTest, REG_CRE_120_GetNumPlayers) {
    nimcp_credit_system_t system = create_credit_system(7);
    ASSERT_NE(system, nullptr);

    EXPECT_EQ(nimcp_credit_get_num_players(system), 7u);

    nimcp_credit_destroy(system);
}

TEST_F(CreditAssignmentRegressionTest, REG_CRE_121_MethodNameConsistency) {
    for (int i = 0; i < NIMCP_CREDIT_COUNT; i++) {
        const char* name = nimcp_credit_method_name((nimcp_credit_method_t)i);
        ASSERT_NE(name, nullptr) << "Method " << i << " has null name";
        EXPECT_GT(strlen(name), 0u) << "Method " << i << " has empty name";
    }
}

//=============================================================================
// REG-CRE-130: Repeated Operations Stability
//=============================================================================

TEST_F(CreditAssignmentRegressionTest, REG_CRE_130_RepeatedCreateDestroy) {
    for (int i = 0; i < 1000; i++) {
        nimcp_credit_system_t system = create_credit_system(4);
        ASSERT_NE(system, nullptr) << "Failed at iteration " << i;
        nimcp_credit_destroy(system);
    }
}

TEST_F(CreditAssignmentRegressionTest, REG_CRE_131_RepeatedCompute) {
    nimcp_credit_system_t system = create_credit_system(5);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t first;
    nimcp_credit_compute_shapley(system, additive_game_value, nullptr, &first);

    // Repeated computations should give identical results
    for (int i = 0; i < 100; i++) {
        nimcp_credit_result_t result;
        nimcp_error_t err = nimcp_credit_compute_shapley(
            system, additive_game_value, nullptr, &result
        );
        ASSERT_EQ(err, NIMCP_SUCCESS);

        for (uint32_t j = 0; j < 5; j++) {
            EXPECT_FLOAT_EQ(result.credits[j], first.credits[j])
                << "Inconsistent at iteration " << i << ", player " << j;
        }
    }

    nimcp_credit_destroy(system);
}

TEST_F(CreditAssignmentRegressionTest, REG_CRE_132_CoalitionsEvaluatedCount) {
    nimcp_credit_system_t system = create_credit_system(4);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_credit_compute_shapley(system, additive_game_value, nullptr, &result);

    // For 4 players, should evaluate 2^4 = 16 coalitions
    EXPECT_EQ(result.coalitions_evaluated, 16u);

    nimcp_credit_destroy(system);
}

