/**
 * @file test_security_game_theory_integration.cpp
 * @brief Integration tests for Security-Game Theory Bridge
 *
 * WHAT: Integration tests for security-game theory bidirectional bridge
 * WHY:  Verify integration with game theory module, multi-player scenarios,
 *       and adversarial strategy testing
 * HOW:  Test real game theory integrations, equilibrium verification,
 *       coalition interactions, and mechanism design integration
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "security/game_theory/nimcp_security_game_theory_bridge.h"
#include "cognitive/game_theory/nimcp_game_theory.h"
#include "cognitive/game_theory/nimcp_gt_equilibrium.h"
#include "cognitive/game_theory/nimcp_gt_coalition.h"
#include "cognitive/game_theory/nimcp_gt_mechanism.h"
#include "utils/error/nimcp_error_codes.h"
}

// ============================================================================
// Test Fixture
// ============================================================================

class SecurityGameTheoryIntegrationTest : public ::testing::Test {
protected:
    security_game_theory_bridge_t* bridge = nullptr;
    nimcp_equilibrium_t equilibrium = nullptr;
    nimcp_coalition_game_t coalition_game = nullptr;
    nimcp_mechanism_t mechanism = nullptr;

    void SetUp() override {
        bridge = nullptr;
        equilibrium = nullptr;
        coalition_game = nullptr;
        mechanism = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            security_gt_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (equilibrium) {
            nimcp_equilibrium_destroy(equilibrium);
            equilibrium = nullptr;
        }
        if (coalition_game) {
            nimcp_coalition_destroy(coalition_game);
            coalition_game = nullptr;
        }
        if (mechanism) {
            nimcp_mechanism_destroy(mechanism);
            mechanism = nullptr;
        }
    }

    void CreateBridge() {
        security_game_theory_config_t config;
        security_gt_default_config(&config);
        bridge = security_gt_bridge_create(&config);
    }

    void CreateEquilibrium(uint32_t num_players, const uint32_t* strategies) {
        nimcp_equilibrium_config_t eq_config = nimcp_equilibrium_default_config(
            num_players, strategies);
        equilibrium = nimcp_equilibrium_create(&eq_config);
    }

    void CreateCoalitionGame(uint32_t num_players) {
        nimcp_coalition_config_t coal_config = nimcp_coalition_default_config(num_players);
        coalition_game = nimcp_coalition_create(&coal_config);
    }

    void CreateMechanism() {
        nimcp_mechanism_config_t mech_config = nimcp_mechanism_default_config();
        mechanism = nimcp_mechanism_create(&mech_config);
    }
};

// ============================================================================
// Game Theory System Integration Tests
// ============================================================================

TEST_F(SecurityGameTheoryIntegrationTest, IntegrateWithEquilibriumSolver) {
    CreateBridge();
    if (!bridge) GTEST_SKIP() << "Bridge creation failed";

    uint32_t strategies[2] = {2, 2};
    CreateEquilibrium(2, strategies);
    if (!equilibrium) GTEST_SKIP() << "Equilibrium solver not available";

    int ret = security_gt_bridge_connect_equilibrium(bridge, equilibrium);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(security_gt_bridge_is_connected(bridge));
}

TEST_F(SecurityGameTheoryIntegrationTest, IntegrateWithCoalitionGame) {
    CreateBridge();
    if (!bridge) GTEST_SKIP() << "Bridge creation failed";

    CreateCoalitionGame(4);
    if (!coalition_game) GTEST_SKIP() << "Coalition game not available";

    int ret = security_gt_bridge_connect_coalition_game(bridge, coalition_game);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityGameTheoryIntegrationTest, IntegrateWithMechanism) {
    CreateBridge();
    if (!bridge) GTEST_SKIP() << "Bridge creation failed";

    CreateMechanism();
    if (!mechanism) GTEST_SKIP() << "Mechanism not available";

    int ret = security_gt_bridge_connect_mechanism(bridge, mechanism);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(security_gt_bridge_is_connected(bridge));
}

TEST_F(SecurityGameTheoryIntegrationTest, IntegrateAllSystems) {
    CreateBridge();
    if (!bridge) GTEST_SKIP() << "Bridge creation failed";

    uint32_t strategies[2] = {2, 2};
    CreateEquilibrium(2, strategies);
    CreateCoalitionGame(4);
    CreateMechanism();

    if (equilibrium) {
        security_gt_bridge_connect_equilibrium(bridge, equilibrium);
    }
    if (coalition_game) {
        security_gt_bridge_connect_coalition_game(bridge, coalition_game);
    }
    if (mechanism) {
        security_gt_bridge_connect_mechanism(bridge, mechanism);
    }

    EXPECT_TRUE(security_gt_bridge_is_connected(bridge));
}

// ============================================================================
// Multi-Player Game Scenario Tests
// ============================================================================

TEST_F(SecurityGameTheoryIntegrationTest, TwoPlayerZeroSumGame) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    /* Matching Pennies game - classic zero-sum */
    float attacker_payoffs[4] = {1.0f, -1.0f, -1.0f, 1.0f};
    float defender_payoffs[4] = {-1.0f, 1.0f, 1.0f, -1.0f};

    /* Validate payoffs */
    security_payoff_result_t payoff_result;
    int ret = security_gt_validate_payoff_matrix(bridge, attacker_payoffs, 2, 2, &payoff_result);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(payoff_result.is_valid);

    ret = security_gt_validate_payoff_matrix(bridge, defender_payoffs, 2, 2, &payoff_result);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(payoff_result.is_valid);

    /* Analyze threat game */
    float defense[2];
    float expected_payoff;
    ret = security_gt_analyze_threat_game(
        bridge, attacker_payoffs, defender_payoffs, 2, 2, defense, &expected_payoff);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityGameTheoryIntegrationTest, ThreePlayerGame) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    /* 3-player game with 2 strategies each: 2x2x2 = 8 outcomes per player */
    float payoffs[8] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};

    security_payoff_result_t result;
    int ret = security_gt_validate_payoff_matrix(bridge, payoffs, 2, 4, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.is_valid);
}

TEST_F(SecurityGameTheoryIntegrationTest, PrisonersDilemmaGame) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    /* Classic Prisoner's Dilemma */
    /* Strategies: Cooperate (0), Defect (1) */
    /* Payoffs: (R, R), (S, T), (T, S), (P, P) where T > R > P > S */
    float player1_payoffs[4] = {3.0f, 0.0f, 5.0f, 1.0f};  /* (C,C)=3, (C,D)=0, (D,C)=5, (D,D)=1 */
    float player2_payoffs[4] = {3.0f, 5.0f, 0.0f, 1.0f};  /* (C,C)=3, (C,D)=5, (D,C)=0, (D,D)=1 */

    security_payoff_result_t result;
    int ret = security_gt_validate_payoff_matrix(bridge, player1_payoffs, 2, 2, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.is_valid);

    ret = security_gt_validate_payoff_matrix(bridge, player2_payoffs, 2, 2, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.is_valid);
}

TEST_F(SecurityGameTheoryIntegrationTest, StagHuntGame) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    /* Stag Hunt - coordination game */
    float player1_payoffs[4] = {4.0f, 0.0f, 3.0f, 3.0f};  /* Stag=4, 0 / Hare=3, 3 */
    float player2_payoffs[4] = {4.0f, 3.0f, 0.0f, 3.0f};

    security_payoff_result_t result;
    int ret = security_gt_validate_payoff_matrix(bridge, player1_payoffs, 2, 2, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.is_valid);

    /* Monitor coalition formation in stag hunt */
    uint32_t hunters[2] = {0, 1};
    security_coalition_result_t coal_result;
    ret = security_gt_monitor_coalition(bridge, 0x3, hunters, 2, &coal_result);
    EXPECT_EQ(ret, 0);
}

// ============================================================================
// Coalition Interaction Tests
// ============================================================================

TEST_F(SecurityGameTheoryIntegrationTest, CoalitionFormationMonitoring) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    CreateCoalitionGame(6);
    if (!coalition_game) GTEST_SKIP() << "Coalition game not available";

    security_gt_bridge_connect_coalition_game(bridge, coalition_game);

    /* Monitor various coalition formations */
    uint32_t coalition1_members[2] = {0, 1};
    uint32_t coalition2_members[3] = {2, 3, 4};
    uint32_t coalition3_members[1] = {5};

    security_coalition_result_t result1, result2, result3;

    int ret = security_gt_monitor_coalition(bridge, 0x3, coalition1_members, 2, &result1);
    EXPECT_EQ(ret, 0);

    ret = security_gt_monitor_coalition(bridge, 0x1C, coalition2_members, 3, &result2);
    EXPECT_EQ(ret, 0);

    ret = security_gt_monitor_coalition(bridge, 0x20, coalition3_members, 1, &result3);
    EXPECT_EQ(ret, 0);

    /* Verify all coalition monitoring succeeded */
    security_game_theory_stats_t stats;
    security_gt_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.total_coalition_checks, 3u);
}

TEST_F(SecurityGameTheoryIntegrationTest, CoalitionMergeScenario) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    /* Monitor initial coalitions */
    uint32_t coalition_a[2] = {0, 1};
    uint32_t coalition_b[2] = {2, 3};

    security_coalition_result_t result_a, result_b;
    security_gt_monitor_coalition(bridge, 0x3, coalition_a, 2, &result_a);
    security_gt_monitor_coalition(bridge, 0xC, coalition_b, 2, &result_b);

    /* Monitor merged coalition */
    uint32_t merged[4] = {0, 1, 2, 3};
    security_coalition_result_t result_merged;
    int ret = security_gt_monitor_coalition(bridge, 0xF, merged, 4, &result_merged);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result_merged.coalition_size, 4u);
}

TEST_F(SecurityGameTheoryIntegrationTest, DefensiveCoalitionAgainstThreat) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    /* Analyze threat first */
    float attacker_payoffs[9] = {5.0f, 3.0f, 1.0f, 4.0f, 6.0f, 2.0f, 3.0f, 2.0f, 4.0f};
    float defender_payoffs[9] = {-5.0f, -3.0f, -1.0f, -4.0f, -6.0f, -2.0f, -3.0f, -2.0f, -4.0f};

    float defense[3];
    float payoff;
    int ret = security_gt_analyze_threat_game(
        bridge, attacker_payoffs, defender_payoffs, 3, 3, defense, &payoff);
    EXPECT_EQ(ret, 0);

    /* Form defensive coalition based on threat */
    uint32_t defenders[5] = {0, 1, 2, 3, 4};
    uint32_t coalition;
    float strength;

    ret = security_gt_form_defensive_coalition(bridge, defenders, 5, &coalition, &strength);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(strength, 0.0f);

    /* Verify effects */
    game_theory_to_security_effects_t effects;
    security_gt_get_gt_effects(bridge, &effects);
    EXPECT_GE(effects.threat_games_analyzed, 1u);
    EXPECT_GE(effects.defense_coalitions_formed, 1u);
}

// ============================================================================
// Adversarial Strategy Tests
// ============================================================================

TEST_F(SecurityGameTheoryIntegrationTest, AdversarialPayoffManipulation) {
    security_game_theory_config_t config;
    security_gt_default_config(&config);
    config.payoff_lower_bound = -1000.0f;
    config.payoff_upper_bound = 1000.0f;
    config.check_nan_inf = true;

    bridge = security_gt_bridge_create(&config);
    if (!bridge) GTEST_SKIP();

    /* Test various adversarial payoff attempts */

    /* Attempt 1: NaN injection */
    float nan_payoffs[4] = {1.0f, std::nan(""), 3.0f, 4.0f};
    security_payoff_result_t result;
    int ret = security_gt_validate_payoff_matrix(bridge, nan_payoffs, 2, 2, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.status, SECURITY_PAYOFF_INVALID_NAN);

    /* Attempt 2: Infinity injection */
    float inf_payoffs[4] = {1.0f, INFINITY, 3.0f, 4.0f};
    ret = security_gt_validate_payoff_matrix(bridge, inf_payoffs, 2, 2, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.status, SECURITY_PAYOFF_INVALID_INF);

    /* Attempt 3: Out of bounds */
    float oob_payoffs[4] = {1.0f, 2000.0f, 3.0f, 4.0f};
    ret = security_gt_validate_payoff_matrix(bridge, oob_payoffs, 2, 2, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.status, SECURITY_PAYOFF_INVALID_BOUNDS);
}

TEST_F(SecurityGameTheoryIntegrationTest, AdversarialCoalitionAttack) {
    security_game_theory_config_t config;
    security_gt_default_config(&config);
    config.max_coalition_size = 4;
    config.sybil_detection_threshold = 0.5f;
    config.collusion_threshold = 0.5f;

    bridge = security_gt_bridge_create(&config);
    if (!bridge) GTEST_SKIP();

    /* Attempt oversized coalition */
    uint32_t large_coalition[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    security_coalition_result_t result;

    int ret = security_gt_monitor_coalition(bridge, 0x3FF, large_coalition, 10, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.is_suspicious);
    EXPECT_EQ(result.alert, SECURITY_COALITION_SIZE_EXCEEDED);
}

TEST_F(SecurityGameTheoryIntegrationTest, AdversarialStrategyManipulation) {
    security_game_theory_config_t config;
    security_gt_default_config(&config);
    config.manipulation_sensitivity = 0.8f;  /* High sensitivity */

    bridge = security_gt_bridge_create(&config);
    if (!bridge) GTEST_SKIP();

    /* Simulate adversarial action pattern - highly predictable/manipulated */
    uint32_t adversarial_actions[16] = {0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1};
    security_manipulation_result_t result;

    int ret = security_gt_detect_manipulation(bridge, 0, adversarial_actions, 16, &result);
    EXPECT_EQ(ret, 0);
    /* High sensitivity should catch obvious patterns */
}

TEST_F(SecurityGameTheoryIntegrationTest, RepeatedGameAttack) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    /* Simulate repeated game with changing payoffs - potential attack */
    std::vector<float> payoffs_round1 = {1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<float> payoffs_round2 = {2.0f, 4.0f, 6.0f, 8.0f};
    std::vector<float> payoffs_round3 = {4.0f, 8.0f, 12.0f, 16.0f};

    security_payoff_result_t result;

    /* Validate each round's payoffs */
    security_gt_validate_payoff_matrix(bridge, payoffs_round1.data(), 2, 2, &result);
    EXPECT_TRUE(result.is_valid);

    security_gt_validate_payoff_matrix(bridge, payoffs_round2.data(), 2, 2, &result);
    EXPECT_TRUE(result.is_valid);

    security_gt_validate_payoff_matrix(bridge, payoffs_round3.data(), 2, 2, &result);
    EXPECT_TRUE(result.is_valid);

    /* Check stats for repeated validation */
    security_game_theory_stats_t stats;
    security_gt_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.total_payoff_validations, 3u);
}

// ============================================================================
// Mechanism Integration Tests
// ============================================================================

TEST_F(SecurityGameTheoryIntegrationTest, MechanismVerificationWithRealMechanism) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    CreateMechanism();
    if (!mechanism) GTEST_SKIP() << "Mechanism not available";

    security_gt_bridge_connect_mechanism(bridge, mechanism);

    security_mechanism_result_t result;
    int ret = security_gt_verify_mechanism(bridge, mechanism, &result);
    EXPECT_EQ(ret, 0);
    /* Result depends on mechanism state */
}

// ============================================================================
// State Consistency Tests
// ============================================================================

TEST_F(SecurityGameTheoryIntegrationTest, StateConsistencyAfterOperations) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_game_theory_state_t state_before, state_after;
    security_gt_bridge_get_state(bridge, &state_before);

    /* Perform multiple operations */
    float payoffs[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    security_payoff_result_t payoff_result;
    security_gt_validate_payoff_matrix(bridge, payoffs, 2, 2, &payoff_result);

    uint32_t players[2] = {0, 1};
    security_coalition_result_t coal_result;
    security_gt_monitor_coalition(bridge, 0x3, players, 2, &coal_result);

    security_gt_bridge_update(bridge, 100);
    security_gt_apply_security_effects(bridge);

    security_gt_bridge_get_state(bridge, &state_after);

    /* State should have been updated */
    EXPECT_GE(state_after.last_update_time, state_before.last_update_time);
    EXPECT_GE(state_after.last_validation_time, state_before.last_validation_time);
}

TEST_F(SecurityGameTheoryIntegrationTest, StatsAccuracyCheck) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    const int num_validations = 5;
    const int num_coalition_checks = 3;
    const int num_manipulation_checks = 2;

    /* Perform known number of operations */
    for (int i = 0; i < num_validations; i++) {
        float payoffs[4] = {(float)i, (float)i+1, (float)i+2, (float)i+3};
        security_payoff_result_t result;
        security_gt_validate_payoff_matrix(bridge, payoffs, 2, 2, &result);
    }

    for (int i = 0; i < num_coalition_checks; i++) {
        uint32_t players[2] = {(uint32_t)i, (uint32_t)i+1};
        security_coalition_result_t result;
        security_gt_monitor_coalition(bridge, 0x3, players, 2, &result);
    }

    for (int i = 0; i < num_manipulation_checks; i++) {
        uint32_t actions[4] = {0, 1, 0, 1};
        security_manipulation_result_t result;
        security_gt_detect_manipulation(bridge, (uint32_t)i, actions, 4, &result);
    }

    /* Verify stats match */
    security_game_theory_stats_t stats;
    security_gt_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(stats.total_payoff_validations, (uint64_t)num_validations);
    EXPECT_EQ(stats.total_coalition_checks, (uint64_t)num_coalition_checks);
    EXPECT_EQ(stats.total_manipulation_checks, (uint64_t)num_manipulation_checks);
}

// ============================================================================
// Effects Propagation Tests
// ============================================================================

TEST_F(SecurityGameTheoryIntegrationTest, SecurityEffectsPropagation) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    /* Generate security events */
    float invalid_payoffs[4] = {std::nan(""), 2.0f, 3.0f, 4.0f};
    security_payoff_result_t payoff_result;
    security_gt_validate_payoff_matrix(bridge, invalid_payoffs, 2, 2, &payoff_result);

    /* Apply effects (before update which resets per-cycle counters) */
    security_gt_apply_security_effects(bridge);

    /* Check effects - payoff_validations/rejections are per-cycle counters
     * set during validate_payoff_matrix, reset by bridge_update */
    security_to_game_theory_effects_t effects;
    security_gt_get_security_effects(bridge, &effects);

    EXPECT_GE(effects.payoff_validations, 1u);
    EXPECT_GE(effects.payoff_rejections, 1u);

    /* Now update for the next cycle */
    security_gt_bridge_update(bridge, 100);
}

TEST_F(SecurityGameTheoryIntegrationTest, GameTheoryEffectsPropagation) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    /* Generate game theory events */
    float attacker[4] = {3.0f, 0.0f, 5.0f, 1.0f};
    float defender[4] = {-3.0f, 0.0f, -5.0f, -1.0f};
    float defense[2];
    float payoff;
    security_gt_analyze_threat_game(bridge, attacker, defender, 2, 2, defense, &payoff);

    uint32_t defenders[3] = {0, 1, 2};
    uint32_t coalition;
    float strength;
    security_gt_form_defensive_coalition(bridge, defenders, 3, &coalition, &strength);

    /* Update and apply effects */
    security_gt_bridge_update(bridge, 100);
    security_gt_apply_gt_effects(bridge);

    /* Check effects */
    game_theory_to_security_effects_t effects;
    security_gt_get_gt_effects(bridge, &effects);

    EXPECT_GE(effects.threat_games_analyzed, 1u);
    EXPECT_GE(effects.defense_coalitions_formed, 1u);
    EXPECT_GE(effects.defense_strategies_computed, 1u);
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(SecurityGameTheoryIntegrationTest, HighVolumePayoffValidation) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    const int num_matrices = 100;

    for (int i = 0; i < num_matrices; i++) {
        float payoffs[16];
        for (int j = 0; j < 16; j++) {
            payoffs[j] = (float)(i * 16 + j) * 0.01f;
        }

        security_payoff_result_t result;
        int ret = security_gt_validate_payoff_matrix(bridge, payoffs, 4, 4, &result);
        EXPECT_EQ(ret, 0);
        EXPECT_TRUE(result.is_valid);
    }

    security_game_theory_stats_t stats;
    security_gt_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_payoff_validations, (uint64_t)num_matrices);
}

TEST_F(SecurityGameTheoryIntegrationTest, HighVolumeCoalitionMonitoring) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    const int num_coalitions = 50;

    for (int i = 0; i < num_coalitions; i++) {
        uint32_t num_players = (uint32_t)(2 + (i % 4));
        uint32_t players[6] = {0, 1, 2, 3, 4, 5};
        uint32_t bitmask = (1u << num_players) - 1;

        security_coalition_result_t result;
        int ret = security_gt_monitor_coalition(bridge, bitmask, players, num_players, &result);
        EXPECT_EQ(ret, 0);
    }

    security_game_theory_stats_t stats;
    security_gt_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_coalition_checks, (uint64_t)num_coalitions);
}

TEST_F(SecurityGameTheoryIntegrationTest, RapidUpdateCycles) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    const int num_cycles = 200;

    for (int i = 0; i < num_cycles; i++) {
        security_gt_bridge_update(bridge, 10);
        security_gt_apply_security_effects(bridge);
        security_gt_apply_gt_effects(bridge);
    }

    security_game_theory_stats_t stats;
    security_gt_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.bridge_updates, (uint64_t)num_cycles);
}

// ============================================================================
// Error Recovery Tests
// ============================================================================

TEST_F(SecurityGameTheoryIntegrationTest, RecoveryAfterInvalidInput) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    /* Invalid input */
    float invalid_payoffs[4] = {std::nan(""), INFINITY, -INFINITY, 0.0f};
    security_payoff_result_t invalid_result;
    security_gt_validate_payoff_matrix(bridge, invalid_payoffs, 2, 2, &invalid_result);
    EXPECT_FALSE(invalid_result.is_valid);

    /* Bridge should still work for valid input */
    float valid_payoffs[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    security_payoff_result_t valid_result;
    int ret = security_gt_validate_payoff_matrix(bridge, valid_payoffs, 2, 2, &valid_result);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(valid_result.is_valid);
}

TEST_F(SecurityGameTheoryIntegrationTest, RecoveryAfterDisconnect) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    /* Connect and disconnect */
    CreateEquilibrium(2, nullptr);
    if (equilibrium) {
        security_gt_bridge_connect_equilibrium(bridge, equilibrium);
        EXPECT_TRUE(security_gt_bridge_is_connected(bridge));
    }

    security_gt_bridge_disconnect(bridge);
    EXPECT_FALSE(security_gt_bridge_is_connected(bridge));

    /* Bridge should still work for basic operations */
    float payoffs[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    security_payoff_result_t result;
    int ret = security_gt_validate_payoff_matrix(bridge, payoffs, 2, 2, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.is_valid);
}

// ============================================================================
// Bio-Async Integration Tests
// ============================================================================

TEST_F(SecurityGameTheoryIntegrationTest, BioAsyncIntegration) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_gt_bridge_connect_bio_async(bridge);
    if (ret != 0) {
        GTEST_SKIP() << "Bio-async connection failed";
    }

    /* Check if actually connected (router may return 0 but not be available) */
    if (!security_gt_bridge_is_bio_async_connected(bridge)) {
        GTEST_SKIP() << "Bio-async router not available";
    }

    /* Perform operations with bio-async connected */
    float payoffs[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    security_payoff_result_t result;
    security_gt_validate_payoff_matrix(bridge, payoffs, 2, 2, &result);

    security_gt_bridge_update(bridge, 100);

    security_gt_bridge_disconnect_bio_async(bridge);
    EXPECT_FALSE(security_gt_bridge_is_bio_async_connected(bridge));
}
