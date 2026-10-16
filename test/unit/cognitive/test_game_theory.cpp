/**
 * @file test_game_theory.cpp
 * @brief Unit tests for Game Theory cognitive module
 *
 * Tests game theory primitives including auctions, bargaining,
 * mechanism design, fairness computation, and Pareto optimality.
 */

#include <gtest/gtest.h>
#include "utils/nimcp_test_base.h"

// Headers have their own extern "C" guards
#include "cognitive/game_theory/nimcp_game_theory.h"

/**
 * @brief Test fixture for Game Theory module tests
 */
class GameTheoryTest : public NimcpTestBase {
protected:
    nimcp_gt_system_t gt_system;
    nimcp_gt_config_t config;

    void SetUp() override {
        NimcpTestBase::SetUp();
        gt_system = NULL;
        config = nimcp_gt_default_config();
    }

    void TearDown() override {
        if (gt_system) {
            nimcp_gt_destroy(gt_system);
            gt_system = NULL;
        }
        NimcpTestBase::TearDown();
    }
};

// ============================================================================
// Default Configuration Tests
// ============================================================================

TEST_F(GameTheoryTest, DefaultConfigReturnsValidValues) {
    nimcp_gt_config_t cfg = nimcp_gt_default_config();

    // Verify reasonable defaults
    EXPECT_GT(cfg.max_players, 0u);
    EXPECT_GT(cfg.max_iterations, 0u);
    EXPECT_GT(cfg.convergence_epsilon, 0.0f);
    EXPECT_LT(cfg.convergence_epsilon, 1.0f);  // Should be a small value
}

TEST_F(GameTheoryTest, DefaultConfigMaxPlayersIsReasonable) {
    nimcp_gt_config_t cfg = nimcp_gt_default_config();

    // Max players should be at least 2 for meaningful games
    EXPECT_GE(cfg.max_players, 2u);
    // But not unreasonably large
    EXPECT_LE(cfg.max_players, 1000u);
}

// ============================================================================
// System Lifecycle Tests
// ============================================================================

TEST_F(GameTheoryTest, CreateWithDefaultConfigSucceeds) {
    gt_system = nimcp_gt_create(&config);
    ASSERT_NE(gt_system, nullptr);
}

TEST_F(GameTheoryTest, CreateWithNullConfigUsesDefaults) {
    // Implementation uses default config when NULL is passed
    gt_system = nimcp_gt_create(nullptr);
    EXPECT_NE(gt_system, nullptr);
}

TEST_F(GameTheoryTest, DestroyNullSystemIsNoOp) {
    // Should not crash
    nimcp_gt_destroy(NULL);
    SUCCEED();
}

TEST_F(GameTheoryTest, CreateDestroyMultipleTimesSucceeds) {
    for (int i = 0; i < 5; i++) {
        gt_system = nimcp_gt_create(&config);
        ASSERT_NE(gt_system, nullptr) << "Failed on iteration " << i;
        nimcp_gt_destroy(gt_system);
        gt_system = NULL;
    }
}

TEST_F(GameTheoryTest, CreateWithCustomConfigSucceeds) {
    config.max_players = 10;
    config.max_iterations = 500;
    config.convergence_epsilon = 0.001f;

    gt_system = nimcp_gt_create(&config);
    ASSERT_NE(gt_system, nullptr);
}

// ============================================================================
// Player Management Tests
// ============================================================================

TEST_F(GameTheoryTest, PlayerInitSucceeds) {
    nimcp_player_t player;
    memset(&player, 0, sizeof(player));
    nimcp_player_init(&player, 0, "test_player", 4);

    // Verify initialization
    EXPECT_EQ(player.id, 0u);
    EXPECT_EQ(player.num_actions, 4u);

    // Cleanup
    nimcp_player_cleanup(&player);
}

TEST_F(GameTheoryTest, PlayerInitWithNullIsNoOp) {
    // Should not crash with NULL
    nimcp_player_init(nullptr, 0, "test", 4);
    SUCCEED();
}

TEST_F(GameTheoryTest, PlayerCleanupNullIsNoOp) {
    // Should not crash
    nimcp_player_cleanup(nullptr);
    SUCCEED();
}

TEST_F(GameTheoryTest, PlayerInitSetsId) {
    nimcp_player_t player;
    memset(&player, 0, sizeof(player));
    nimcp_player_init(&player, 42, "player_42", 2);
    EXPECT_EQ(player.id, 42u);

    nimcp_player_cleanup(&player);
}

TEST_F(GameTheoryTest, MultiplePlayersHaveDistinctIds) {
    nimcp_player_t players[4];
    const char* names[] = {"p0", "p1", "p2", "p3"};

    for (int i = 0; i < 4; i++) {
        memset(&players[i], 0, sizeof(nimcp_player_t));
        nimcp_player_init(&players[i], (nimcp_player_id_t)i, names[i], 2);
        EXPECT_EQ(players[i].id, (nimcp_player_id_t)i);
    }

    // Cleanup
    for (int i = 0; i < 4; i++) {
        nimcp_player_cleanup(&players[i]);
    }
}

// ============================================================================
// Fairness Index Tests
// ============================================================================

TEST_F(GameTheoryTest, FairnessIndexEqualPayoffsReturnsOne) {
    float allocations[] = {1.0f, 1.0f, 1.0f, 1.0f};
    float fairness = nimcp_compute_fairness_index(allocations, 4);

    // Equal allocations should give maximum fairness (1.0)
    EXPECT_FLOAT_EQ(fairness, 1.0f);
}

TEST_F(GameTheoryTest, FairnessIndexUnequalPayoffsLessThanOne) {
    float allocations[] = {10.0f, 0.0f, 0.0f, 0.0f};  // Very unequal
    float fairness = nimcp_compute_fairness_index(allocations, 4);

    // Unequal allocations should give fairness < 1.0
    EXPECT_LT(fairness, 1.0f);
    EXPECT_GE(fairness, 0.0f);
}

TEST_F(GameTheoryTest, FairnessIndexSinglePlayerReturnsOne) {
    float allocations[] = {5.0f};
    float fairness = nimcp_compute_fairness_index(allocations, 1);

    // Single player trivially has fair distribution
    EXPECT_FLOAT_EQ(fairness, 1.0f);
}

TEST_F(GameTheoryTest, FairnessIndexNullPayoffsReturnsZero) {
    float fairness = nimcp_compute_fairness_index(nullptr, 4);
    EXPECT_FLOAT_EQ(fairness, 0.0f);
}

TEST_F(GameTheoryTest, FairnessIndexZeroPlayersReturnsZero) {
    float allocations[] = {1.0f, 2.0f};
    float fairness = nimcp_compute_fairness_index(allocations, 0);
    EXPECT_FLOAT_EQ(fairness, 0.0f);
}

TEST_F(GameTheoryTest, FairnessIndexProportionalPayoffs) {
    // Jain's fairness index for proportional allocations
    float allocations[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float fairness = nimcp_compute_fairness_index(allocations, 4);

    // Should be between 0 and 1
    EXPECT_GT(fairness, 0.0f);
    EXPECT_LE(fairness, 1.0f);
}

// ============================================================================
// Pareto Optimality Tests
// ============================================================================

TEST_F(GameTheoryTest, IsParetoOptimalReturnsValidResult) {
    // Current utilities for 2 players
    float utilities[] = {5.0f, 5.0f};

    // Feasible alternatives (flattened: alt1, alt2)
    float feasible[] = {4.0f, 4.0f,   // Alternative 1: worse for both
                        3.0f, 6.0f};  // Alternative 2: worse for p0, better for p1

    bool is_optimal = nimcp_is_pareto_optimal(utilities, 2, feasible, 2);

    // Outcome (5,5) is Pareto optimal relative to (4,4) and (3,6)
    // because no alternative makes everyone better off
    EXPECT_TRUE(is_optimal);
}

TEST_F(GameTheoryTest, IsParetoOptimalWithNullUtilitiesReturnsFalse) {
    float feasible[] = {1.0f, 1.0f};
    bool is_optimal = nimcp_is_pareto_optimal(nullptr, 2, feasible, 1);
    EXPECT_FALSE(is_optimal);
}

TEST_F(GameTheoryTest, IsParetoOptimalWithNullAlternativesReturnsFalse) {
    float utilities[] = {5.0f, 5.0f};

    // Implementation returns false when alternatives is NULL (defensive)
    bool is_optimal = nimcp_is_pareto_optimal(utilities, 2, nullptr, 0);
    EXPECT_FALSE(is_optimal);
}

TEST_F(GameTheoryTest, IsParetoOptimalDominatedOutcome) {
    float utilities[] = {2.0f, 2.0f};

    // Alternative that strictly dominates
    float feasible[] = {3.0f, 3.0f};  // Better for both players

    bool is_optimal = nimcp_is_pareto_optimal(utilities, 2, feasible, 1);

    // Outcome is dominated by alternative, so not Pareto optimal
    EXPECT_FALSE(is_optimal);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(GameTheoryTest, GetStatsReturnsValidStats) {
    gt_system = nimcp_gt_create(&config);
    ASSERT_NE(gt_system, nullptr);

    nimcp_game_stats_t stats;
    int result = nimcp_gt_get_stats(gt_system, &stats);

    EXPECT_EQ(result, 0);
    // Initial stats should be zeroed
    EXPECT_EQ(stats.games_played, 0u);
    EXPECT_EQ(stats.equilibria_found, 0u);
}

TEST_F(GameTheoryTest, GetStatsNullSystemReturnsError) {
    nimcp_game_stats_t stats;
    int result = nimcp_gt_get_stats(NULL, &stats);
    EXPECT_NE(result, 0);
}

TEST_F(GameTheoryTest, GetStatsNullStatsReturnsError) {
    gt_system = nimcp_gt_create(&config);
    ASSERT_NE(gt_system, nullptr);

    int result = nimcp_gt_get_stats(gt_system, nullptr);
    EXPECT_NE(result, 0);
}

// ============================================================================
// Game Type Enum Tests
// ============================================================================

TEST_F(GameTheoryTest, GameTypeEnumsAreDefined) {
    // Verify game types are accessible
    EXPECT_NE(NIMCP_GAME_AUCTION, NIMCP_GAME_BARGAINING);
    EXPECT_NE(NIMCP_GAME_BARGAINING, NIMCP_GAME_COOPERATIVE);
}

TEST_F(GameTheoryTest, StrategyTypeEnumsAreDefined) {
    // Verify strategy types are accessible
    EXPECT_NE(NIMCP_STRATEGY_DOMINANT, NIMCP_STRATEGY_PURE);
    EXPECT_NE(NIMCP_STRATEGY_PURE, NIMCP_STRATEGY_MIXED);
}

TEST_F(GameTheoryTest, SolutionConceptEnumsAreDefined) {
    // Verify solution concepts are accessible
    EXPECT_NE(NIMCP_SOLUTION_NASH, NIMCP_SOLUTION_PARETO_OPTIMAL);
    EXPECT_NE(NIMCP_SOLUTION_PARETO_OPTIMAL, NIMCP_SOLUTION_CORRELATED);
}

// ============================================================================
// Game Outcome Tests
// ============================================================================

TEST_F(GameTheoryTest, GameOutcomeInitSucceeds) {
    nimcp_game_outcome_t outcome;
    nimcp_game_outcome_init(&outcome);

    // Verify initialized to sensible defaults
    EXPECT_EQ(outcome.num_winners, 0u);
    EXPECT_FLOAT_EQ(outcome.social_welfare, 0.0f);
}

TEST_F(GameTheoryTest, GameOutcomeInitNullIsNoOp) {
    // Should not crash
    nimcp_game_outcome_init(nullptr);
    SUCCEED();
}

// ============================================================================
// Edge Cases and Boundary Tests
// ============================================================================

TEST_F(GameTheoryTest, FairnessIndexWithAllZeroPayoffs) {
    float allocations[] = {0.0f, 0.0f, 0.0f, 0.0f};
    float fairness = nimcp_compute_fairness_index(allocations, 4);

    // All zero allocations - edge case, implementation dependent
    // Should not crash, result should be valid
    EXPECT_GE(fairness, 0.0f);
    EXPECT_LE(fairness, 1.0f);
}

TEST_F(GameTheoryTest, FairnessIndexWithNegativePayoffs) {
    float allocations[] = {-1.0f, -1.0f, -1.0f, -1.0f};
    float fairness = nimcp_compute_fairness_index(allocations, 4);

    // Equal negative allocations should still be "fair"
    // Implementation may handle this differently
    EXPECT_GE(fairness, 0.0f);
}

TEST_F(GameTheoryTest, LargeNumberOfPlayers) {
    config.max_players = 100;
    gt_system = nimcp_gt_create(&config);
    ASSERT_NE(gt_system, nullptr);

    // System should handle large player counts
    nimcp_game_stats_t stats;
    int result = nimcp_gt_get_stats(gt_system, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(GameTheoryTest, VerySmallConvergenceEpsilon) {
    config.convergence_epsilon = 1e-10f;
    gt_system = nimcp_gt_create(&config);
    ASSERT_NE(gt_system, nullptr);
}

TEST_F(GameTheoryTest, MinimalConfiguration) {
    config.max_players = 2;
    config.max_iterations = 1;
    config.convergence_epsilon = 0.1f;

    gt_system = nimcp_gt_create(&config);
    ASSERT_NE(gt_system, nullptr);
}
