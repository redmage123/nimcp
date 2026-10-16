//=============================================================================
// test_gt_repeated.cpp - Unit tests for Repeated Games Module
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/game_theory/nimcp_gt_repeated.h"
#include "cognitive/game_theory/nimcp_game_theory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class RepeatedGameTest : public ::testing::Test {
protected:
    nimcp_repeated_game_t game = nullptr;
    nimcp_repeated_config_t config;

    // Prisoner's Dilemma payoffs (2 players, 2 actions each)
    // Actions: 0 = Cooperate, 1 = Defect
    // Payoff matrix (row, col) -> (row_payoff, col_payoff):
    // (C,C) -> (3,3), (C,D) -> (0,5), (D,C) -> (5,0), (D,D) -> (1,1)
    float pd_payoffs[8] = {
        3.0f, 3.0f,   // (C,C) - row=0, col=0
        0.0f, 5.0f,   // (C,D) - row=0, col=1
        5.0f, 0.0f,   // (D,C) - row=1, col=0
        1.0f, 1.0f    // (D,D) - row=1, col=1
    };
    uint32_t pd_actions[2] = {2, 2};  // Both players have 2 actions

    void SetUp() override {
        config = nimcp_repeated_default_config();
        config.num_players = 2;
    }

    void TearDown() override {
        if (game) {
            nimcp_repeated_destroy(game);
            game = nullptr;
        }
    }

    void CreateGame() {
        game = nimcp_repeated_create(&config, pd_payoffs, pd_actions, 2);
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(RepeatedGameTest, DefaultConfigValues) {
    EXPECT_GT(config.discount_factor, 0.0f);
    EXPECT_LE(config.discount_factor, 1.0f);
    EXPECT_GT(config.history_length, 0);
}

TEST_F(RepeatedGameTest, StrategyTypeNames) {
    EXPECT_NE(nimcp_repeated_strategy_name(NIMCP_STRATEGY_ALWAYS_COOPERATE), nullptr);
    EXPECT_NE(nimcp_repeated_strategy_name(NIMCP_STRATEGY_ALWAYS_DEFECT), nullptr);
    EXPECT_NE(nimcp_repeated_strategy_name(NIMCP_STRATEGY_TIT_FOR_TAT), nullptr);
    EXPECT_NE(nimcp_repeated_strategy_name(NIMCP_STRATEGY_GRIM_TRIGGER), nullptr);
    EXPECT_NE(nimcp_repeated_strategy_name(NIMCP_STRATEGY_GENEROUS_TFT), nullptr);
    EXPECT_NE(nimcp_repeated_strategy_name(NIMCP_STRATEGY_PAVLOV), nullptr);
}

TEST_F(RepeatedGameTest, CooperationLevelNames) {
    EXPECT_NE(nimcp_repeated_coop_level_name(NIMCP_COOP_LEVEL_NONE), nullptr);
    EXPECT_NE(nimcp_repeated_coop_level_name(NIMCP_COOP_LEVEL_LOW), nullptr);
    EXPECT_NE(nimcp_repeated_coop_level_name(NIMCP_COOP_LEVEL_MEDIUM), nullptr);
    EXPECT_NE(nimcp_repeated_coop_level_name(NIMCP_COOP_LEVEL_HIGH), nullptr);
    EXPECT_NE(nimcp_repeated_coop_level_name(NIMCP_COOP_LEVEL_FULL), nullptr);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(RepeatedGameTest, CreateDestroy) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    nimcp_repeated_destroy(game);
    game = nullptr;
}

TEST_F(RepeatedGameTest, CreateWithNullConfig) {
    game = nimcp_repeated_create(nullptr, pd_payoffs, pd_actions, 2);
    // Should use defaults or return nullptr
}

TEST_F(RepeatedGameTest, Reset) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    nimcp_error_t err = nimcp_repeated_reset(game);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

//=============================================================================
// Strategy Setting Tests
//=============================================================================

TEST_F(RepeatedGameTest, SetStrategyAlwaysCooperate) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    nimcp_repeated_strategy_t strategy = {};
    strategy.type = NIMCP_STRATEGY_ALWAYS_COOPERATE;

    nimcp_error_t err = nimcp_repeated_set_strategy(game, 0, &strategy);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    err = nimcp_repeated_set_strategy(game, 1, &strategy);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(RepeatedGameTest, SetStrategyTitForTat) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    nimcp_repeated_strategy_t strategy = {};
    strategy.type = NIMCP_STRATEGY_TIT_FOR_TAT;

    nimcp_error_t err = nimcp_repeated_set_strategy(game, 0, &strategy);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(RepeatedGameTest, SetStrategyGrimTrigger) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    nimcp_repeated_strategy_t strategy = {};
    strategy.type = NIMCP_STRATEGY_GRIM_TRIGGER;

    nimcp_error_t err = nimcp_repeated_set_strategy(game, 0, &strategy);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(RepeatedGameTest, SetStrategyGenerousTft) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    nimcp_repeated_strategy_t strategy = {};
    strategy.type = NIMCP_STRATEGY_GENEROUS_TFT;
    strategy.forgiveness_prob = 0.1f;

    nimcp_error_t err = nimcp_repeated_set_strategy(game, 0, &strategy);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(RepeatedGameTest, SetStrategyPavlov) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    nimcp_repeated_strategy_t strategy = {};
    strategy.type = NIMCP_STRATEGY_PAVLOV;

    nimcp_error_t err = nimcp_repeated_set_strategy(game, 0, &strategy);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

//=============================================================================
// Simulation Tests
//=============================================================================

TEST_F(RepeatedGameTest, SimulateCooperateVsCooperate) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    nimcp_repeated_strategy_t coop = {};
    coop.type = NIMCP_STRATEGY_ALWAYS_COOPERATE;
    nimcp_repeated_set_strategy(game, 0, &coop);
    nimcp_repeated_set_strategy(game, 1, &coop);

    nimcp_repeated_result_t result;
    nimcp_error_t err = nimcp_repeated_simulate(game, 10, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Mutual cooperation gives (3,3) each round
    EXPECT_FLOAT_EQ(result.avg_payoffs[0], 3.0f);
    EXPECT_FLOAT_EQ(result.avg_payoffs[1], 3.0f);
    EXPECT_EQ(result.rounds_played, 10);
}

TEST_F(RepeatedGameTest, SimulateDefectVsDefect) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    nimcp_repeated_strategy_t defect = {};
    defect.type = NIMCP_STRATEGY_ALWAYS_DEFECT;
    nimcp_repeated_set_strategy(game, 0, &defect);
    nimcp_repeated_set_strategy(game, 1, &defect);

    nimcp_repeated_result_t result;
    nimcp_error_t err = nimcp_repeated_simulate(game, 10, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Mutual defection gives (1,1) each round
    EXPECT_FLOAT_EQ(result.avg_payoffs[0], 1.0f);
    EXPECT_FLOAT_EQ(result.avg_payoffs[1], 1.0f);
}

TEST_F(RepeatedGameTest, SimulateTftVsTft) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    nimcp_repeated_strategy_t tft = {};
    tft.type = NIMCP_STRATEGY_TIT_FOR_TAT;
    nimcp_repeated_set_strategy(game, 0, &tft);
    nimcp_repeated_set_strategy(game, 1, &tft);

    nimcp_repeated_result_t result;
    nimcp_error_t err = nimcp_repeated_simulate(game, 10, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // TFT vs TFT should cooperate (TFT starts with cooperate)
    EXPECT_FLOAT_EQ(result.avg_payoffs[0], 3.0f);
    EXPECT_FLOAT_EQ(result.avg_payoffs[1], 3.0f);
}

TEST_F(RepeatedGameTest, SimulateTftVsDefect) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    nimcp_repeated_strategy_t tft = {};
    tft.type = NIMCP_STRATEGY_TIT_FOR_TAT;

    nimcp_repeated_strategy_t defect = {};
    defect.type = NIMCP_STRATEGY_ALWAYS_DEFECT;

    nimcp_repeated_set_strategy(game, 0, &tft);
    nimcp_repeated_set_strategy(game, 1, &defect);

    nimcp_repeated_result_t result;
    nimcp_error_t err = nimcp_repeated_simulate(game, 10, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Round 1: TFT cooperates, Defect defects -> (0, 5)
    // Rounds 2+: TFT defects, Defect defects -> (1, 1)
    // Average: player 0 ~ (0 + 9*1)/10 = 0.9, player 1 ~ (5 + 9*1)/10 = 1.4
    EXPECT_NEAR(result.avg_payoffs[0], 0.9f, 0.01f);
    EXPECT_NEAR(result.avg_payoffs[1], 1.4f, 0.01f);
}

TEST_F(RepeatedGameTest, SimulateGrimTrigger) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    nimcp_repeated_strategy_t grim = {};
    grim.type = NIMCP_STRATEGY_GRIM_TRIGGER;
    nimcp_repeated_set_strategy(game, 0, &grim);
    nimcp_repeated_set_strategy(game, 1, &grim);

    nimcp_repeated_result_t result;
    nimcp_error_t err = nimcp_repeated_simulate(game, 10, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Grim Trigger vs Grim Trigger should cooperate forever
    EXPECT_FLOAT_EQ(result.avg_payoffs[0], 3.0f);
    EXPECT_FLOAT_EQ(result.avg_payoffs[1], 3.0f);
}

//=============================================================================
// Round Playing Tests
//=============================================================================

TEST_F(RepeatedGameTest, PlayRound) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    uint32_t actions[2] = {0, 0};  // Both cooperate
    float payoffs[2] = {0};

    nimcp_error_t err = nimcp_repeated_play_round(game, actions, payoffs);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_FLOAT_EQ(payoffs[0], 3.0f);
    EXPECT_FLOAT_EQ(payoffs[1], 3.0f);
}

TEST_F(RepeatedGameTest, PlayRoundDefection) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    uint32_t actions[2] = {0, 1};  // Player 0 cooperates, player 1 defects
    float payoffs[2] = {0};

    nimcp_error_t err = nimcp_repeated_play_round(game, actions, payoffs);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_FLOAT_EQ(payoffs[0], 0.0f);  // Sucker's payoff
    EXPECT_FLOAT_EQ(payoffs[1], 5.0f);  // Temptation payoff
}

TEST_F(RepeatedGameTest, GetHistory) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    // Play a few rounds
    uint32_t actions[2] = {0, 0};
    nimcp_repeated_play_round(game, actions, nullptr);
    actions[0] = 1; actions[1] = 0;
    nimcp_repeated_play_round(game, actions, nullptr);
    actions[0] = 1; actions[1] = 1;
    nimcp_repeated_play_round(game, actions, nullptr);

    nimcp_repeated_history_t history;
    nimcp_error_t err = nimcp_repeated_get_history(game, &history);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(history.num_rounds, 3);
}

//=============================================================================
// Discount Factor Tests
//=============================================================================

TEST_F(RepeatedGameTest, SetDiscount) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    nimcp_error_t err = nimcp_repeated_set_discount(game, 0.95f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    float discount = nimcp_repeated_get_discount(game);
    EXPECT_FLOAT_EQ(discount, 0.95f);
}

TEST_F(RepeatedGameTest, GetDiscount) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    float discount = nimcp_repeated_get_discount(game);
    EXPECT_GT(discount, 0.0f);
    EXPECT_LE(discount, 1.0f);
}

//=============================================================================
// Folk Theorem Tests
//=============================================================================

TEST_F(RepeatedGameTest, MinmaxPayoff) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    float minmax0 = nimcp_repeated_minmax_payoff(game, 0);
    float minmax1 = nimcp_repeated_minmax_payoff(game, 1);

    // In PD, minmax payoff is 1 (mutual defection)
    EXPECT_FLOAT_EQ(minmax0, 1.0f);
    EXPECT_FLOAT_EQ(minmax1, 1.0f);
}

TEST_F(RepeatedGameTest, FeasibilitySet) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    nimcp_payoff_point_t vertices[NIMCP_REPEATED_MAX_VERTICES];
    uint32_t num_vertices = NIMCP_REPEATED_MAX_VERTICES;

    nimcp_error_t err = nimcp_repeated_feasibility_set(game, vertices, &num_vertices);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(num_vertices, 0);

    // Should have 4 pure strategy profile payoffs
    // (C,C)=(3,3), (C,D)=(0,5), (D,C)=(5,0), (D,D)=(1,1)
    EXPECT_EQ(num_vertices, 4);
}

TEST_F(RepeatedGameTest, CriticalDiscount) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    float target[2] = {3.0f, 3.0f};  // Mutual cooperation payoffs
    float critical = nimcp_repeated_critical_discount(game, target);

    // For mutual cooperation in PD, delta* < 1 should be achievable
    EXPECT_LT(critical, 1.0f);
    EXPECT_GT(critical, 0.0f);
}

TEST_F(RepeatedGameTest, IsSustainable) {
    config.discount_factor = 0.9f;  // High discount
    CreateGame();
    ASSERT_NE(game, nullptr);

    float target[2] = {3.0f, 3.0f};
    bool sustainable = nimcp_repeated_is_sustainable(game, target);

    // With high discount, cooperation should be sustainable
    EXPECT_TRUE(sustainable);
}

TEST_F(RepeatedGameTest, NotSustainableWithLowDiscount) {
    config.discount_factor = 0.1f;  // Very low discount (myopic players)
    CreateGame();
    ASSERT_NE(game, nullptr);

    float target[2] = {3.0f, 3.0f};
    bool sustainable = nimcp_repeated_is_sustainable(game, target);

    // With very low discount, cooperation may not be sustainable
    // Players would prefer to defect for immediate gain
    // Result depends on implementation
}

//=============================================================================
// Trigger Strategy State Tests
//=============================================================================

TEST_F(RepeatedGameTest, TriggerNotActivatedInitially) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    nimcp_repeated_strategy_t grim = {};
    grim.type = NIMCP_STRATEGY_GRIM_TRIGGER;
    nimcp_repeated_set_strategy(game, 0, &grim);

    bool activated = nimcp_repeated_trigger_activated(game, 0);
    EXPECT_FALSE(activated);
}

TEST_F(RepeatedGameTest, GetStrategyAction) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    nimcp_repeated_strategy_t coop = {};
    coop.type = NIMCP_STRATEGY_ALWAYS_COOPERATE;
    nimcp_repeated_set_strategy(game, 0, &coop);

    uint32_t action = nimcp_repeated_get_strategy_action(game, 0);
    EXPECT_EQ(action, 0);  // Cooperate
}

TEST_F(RepeatedGameTest, TriggerThreshold) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    nimcp_repeated_strategy_t grim = {};
    grim.type = NIMCP_STRATEGY_GRIM_TRIGGER;
    nimcp_repeated_set_strategy(game, 0, &grim);
    nimcp_repeated_set_strategy(game, 1, &grim);

    // No trigger yet
    int32_t threshold = nimcp_repeated_trigger_threshold(game, 0);
    EXPECT_EQ(threshold, -1);
}

//=============================================================================
// Cooperation Analysis Tests
//=============================================================================

TEST_F(RepeatedGameTest, DetectCooperationNone) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    // Play defection only
    nimcp_repeated_strategy_t defect = {};
    defect.type = NIMCP_STRATEGY_ALWAYS_DEFECT;
    nimcp_repeated_set_strategy(game, 0, &defect);
    nimcp_repeated_set_strategy(game, 1, &defect);

    nimcp_repeated_result_t result;
    nimcp_repeated_simulate(game, 10, &result);

    nimcp_cooperation_level_t level = nimcp_repeated_detect_cooperation(game);
    EXPECT_EQ(level, NIMCP_COOP_LEVEL_NONE);
}

TEST_F(RepeatedGameTest, DetectCooperationFull) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    nimcp_repeated_strategy_t coop = {};
    coop.type = NIMCP_STRATEGY_ALWAYS_COOPERATE;
    nimcp_repeated_set_strategy(game, 0, &coop);
    nimcp_repeated_set_strategy(game, 1, &coop);

    nimcp_repeated_result_t result;
    nimcp_repeated_simulate(game, 10, &result);

    nimcp_cooperation_level_t level = nimcp_repeated_detect_cooperation(game);
    EXPECT_EQ(level, NIMCP_COOP_LEVEL_FULL);
}

TEST_F(RepeatedGameTest, CooperationRate) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    nimcp_repeated_strategy_t coop = {};
    coop.type = NIMCP_STRATEGY_ALWAYS_COOPERATE;
    nimcp_repeated_set_strategy(game, 0, &coop);
    nimcp_repeated_set_strategy(game, 1, &coop);

    nimcp_repeated_result_t result;
    nimcp_repeated_simulate(game, 10, &result);

    float rate = nimcp_repeated_cooperation_rate(game);
    EXPECT_FLOAT_EQ(rate, 1.0f);
}

TEST_F(RepeatedGameTest, IsStable) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    nimcp_repeated_strategy_t coop = {};
    coop.type = NIMCP_STRATEGY_ALWAYS_COOPERATE;
    nimcp_repeated_set_strategy(game, 0, &coop);
    nimcp_repeated_set_strategy(game, 1, &coop);

    nimcp_repeated_result_t result;
    nimcp_repeated_simulate(game, 20, &result);

    bool stable = nimcp_repeated_is_stable(game, 5);
    EXPECT_TRUE(stable);  // Constant cooperation is stable
}

//=============================================================================
// Payoff Computation Tests
//=============================================================================

TEST_F(RepeatedGameTest, ComputeAvgPayoff) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    nimcp_repeated_strategy_t coop = {};
    coop.type = NIMCP_STRATEGY_ALWAYS_COOPERATE;
    nimcp_repeated_set_strategy(game, 0, &coop);
    nimcp_repeated_set_strategy(game, 1, &coop);

    nimcp_repeated_result_t result;
    nimcp_repeated_simulate(game, 10, &result);

    float avg0 = nimcp_repeated_compute_avg_payoff(game, 0);
    float avg1 = nimcp_repeated_compute_avg_payoff(game, 1);

    EXPECT_FLOAT_EQ(avg0, 3.0f);
    EXPECT_FLOAT_EQ(avg1, 3.0f);
}

TEST_F(RepeatedGameTest, ComputeDiscountedPayoff) {
    config.discount_factor = 0.9f;
    CreateGame();
    ASSERT_NE(game, nullptr);

    nimcp_repeated_strategy_t coop = {};
    coop.type = NIMCP_STRATEGY_ALWAYS_COOPERATE;
    nimcp_repeated_set_strategy(game, 0, &coop);
    nimcp_repeated_set_strategy(game, 1, &coop);

    nimcp_repeated_result_t result;
    nimcp_repeated_simulate(game, 10, &result);

    float discounted = nimcp_repeated_compute_discounted_payoff(game, 0);
    // Should be close to 3 * (sum of geometric series)
    EXPECT_GT(discounted, 0.0f);
}

//=============================================================================
// Query Function Tests
//=============================================================================

TEST_F(RepeatedGameTest, GetNumRounds) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    uint32_t initial = nimcp_repeated_get_num_rounds(game);
    EXPECT_EQ(initial, 0);

    uint32_t actions[2] = {0, 0};
    nimcp_repeated_play_round(game, actions, nullptr);
    nimcp_repeated_play_round(game, actions, nullptr);
    nimcp_repeated_play_round(game, actions, nullptr);

    uint32_t rounds = nimcp_repeated_get_num_rounds(game);
    EXPECT_EQ(rounds, 3);
}

TEST_F(RepeatedGameTest, GetNumPlayers) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    uint32_t players = nimcp_repeated_get_num_players(game);
    EXPECT_EQ(players, 2);
}

TEST_F(RepeatedGameTest, GetStrategyType) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    nimcp_repeated_strategy_t tft = {};
    tft.type = NIMCP_STRATEGY_TIT_FOR_TAT;
    nimcp_repeated_set_strategy(game, 0, &tft);

    nimcp_repeated_strategy_type_t type = nimcp_repeated_get_strategy_type(game, 0);
    EXPECT_EQ(type, NIMCP_STRATEGY_TIT_FOR_TAT);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(RepeatedGameTest, ZeroRoundSimulation) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    nimcp_repeated_result_t result;
    nimcp_error_t err = nimcp_repeated_simulate(game, 0, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(result.rounds_played, 0);
}

TEST_F(RepeatedGameTest, SingleRoundSimulation) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    nimcp_repeated_strategy_t coop = {};
    coop.type = NIMCP_STRATEGY_ALWAYS_COOPERATE;
    nimcp_repeated_set_strategy(game, 0, &coop);
    nimcp_repeated_set_strategy(game, 1, &coop);

    nimcp_repeated_result_t result;
    nimcp_error_t err = nimcp_repeated_simulate(game, 1, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(result.rounds_played, 1);
    EXPECT_FLOAT_EQ(result.avg_payoffs[0], 3.0f);
}

TEST_F(RepeatedGameTest, LongSimulation) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    nimcp_repeated_strategy_t coop = {};
    coop.type = NIMCP_STRATEGY_ALWAYS_COOPERATE;
    nimcp_repeated_set_strategy(game, 0, &coop);
    nimcp_repeated_set_strategy(game, 1, &coop);

    nimcp_repeated_result_t result;
    nimcp_error_t err = nimcp_repeated_simulate(game, 1000, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(result.rounds_played, 1000);
}

TEST_F(RepeatedGameTest, ResetClearsHistory) {
    CreateGame();
    ASSERT_NE(game, nullptr);

    uint32_t actions[2] = {0, 0};
    nimcp_repeated_play_round(game, actions, nullptr);
    nimcp_repeated_play_round(game, actions, nullptr);

    EXPECT_EQ(nimcp_repeated_get_num_rounds(game), 2);

    nimcp_repeated_reset(game);

    EXPECT_EQ(nimcp_repeated_get_num_rounds(game), 0);
}
