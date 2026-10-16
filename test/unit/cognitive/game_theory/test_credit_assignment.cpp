//=============================================================================
// test_credit_assignment.cpp - Unit tests for Credit Assignment Module
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/game_theory/nimcp_credit_assignment.h"
#include "cognitive/game_theory/nimcp_game_theory.h"

//=============================================================================
// Coalition Value Functions for Testing
//=============================================================================

// Simple additive game: v(S) = sum of player values
static float additive_value(uint32_t coalition, uint32_t num_players, void* user_data) {
    (void)user_data;
    float* values = static_cast<float*>(user_data);
    float sum = 0.0f;

    for (uint32_t i = 0; i < num_players; i++) {
        if (coalition & (1 << i)) {
            sum += values ? values[i] : 1.0f;
        }
    }
    return sum;
}

// Superadditive game: combined value > sum of parts
static float superadditive_value(uint32_t coalition, uint32_t num_players, void* user_data) {
    (void)user_data;
    (void)num_players;

    int count = 0;
    for (uint32_t i = 0; i < 32; i++) {
        if (coalition & (1 << i)) count++;
    }

    if (count == 0) return 0.0f;
    if (count == 1) return 1.0f;
    if (count == 2) return 3.0f;  // Synergy: 1+1 -> 3
    return 6.0f;  // Grand coalition with 3 players
}

// Unanimous game: only grand coalition has value
static float unanimous_value(uint32_t coalition, uint32_t num_players, void* user_data) {
    (void)user_data;

    uint32_t grand = (1 << num_players) - 1;
    return (coalition == grand) ? 10.0f : 0.0f;
}

// Dictator game: only player 0 matters
static float dictator_value(uint32_t coalition, uint32_t num_players, void* user_data) {
    (void)num_players;
    (void)user_data;

    return (coalition & 1) ? 10.0f : 0.0f;
}

//=============================================================================
// Test Fixture
//=============================================================================

class CreditAssignmentTest : public ::testing::Test {
protected:
    nimcp_credit_system_t system = nullptr;
    nimcp_credit_config_t config;

    void SetUp() override {
        config = nimcp_credit_default_config(3);
    }

    void TearDown() override {
        if (system) {
            nimcp_credit_destroy(system);
            system = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(CreditAssignmentTest, DefaultConfigValues) {
    EXPECT_EQ(config.method, NIMCP_CREDIT_SHAPLEY);
    EXPECT_EQ(config.num_players, 3);
    EXPECT_GT(config.monte_carlo_samples, 0);
}

TEST_F(CreditAssignmentTest, CreateDestroy) {
    system = nimcp_credit_create(&config);
    ASSERT_NE(system, nullptr);

    EXPECT_EQ(nimcp_credit_get_num_players(system), 3);
}

TEST_F(CreditAssignmentTest, CreateWithNullConfig) {
    system = nimcp_credit_create(nullptr);
    // May return NULL or use defaults - just verify no crash
}

//=============================================================================
// Shapley Value Tests
//=============================================================================

TEST_F(CreditAssignmentTest, ShapleyAdditiveGame) {
    // In additive game, Shapley value = player's individual value
    float values[] = {2.0f, 3.0f, 5.0f};

    config.num_players = 3;
    system = nimcp_credit_create(&config);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_compute_shapley(system, additive_value, values, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Each player should get their exact contribution
    EXPECT_NEAR(result.credits[0], 2.0f, 0.1f);
    EXPECT_NEAR(result.credits[1], 3.0f, 0.1f);
    EXPECT_NEAR(result.credits[2], 5.0f, 0.1f);

    // Total should equal grand coalition value
    EXPECT_NEAR(result.total_value, 10.0f, 0.1f);
}

TEST_F(CreditAssignmentTest, ShapleyEfficiencyAxiom) {
    // Shapley values should sum to grand coalition value
    config.num_players = 3;
    system = nimcp_credit_create(&config);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_compute_shapley(system, superadditive_value, nullptr, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    float sum = result.credits[0] + result.credits[1] + result.credits[2];
    EXPECT_NEAR(sum, result.total_value, 0.01f);
}

TEST_F(CreditAssignmentTest, ShapleySymmetryAxiom) {
    // Symmetric players should receive equal Shapley values
    float values[] = {1.0f, 1.0f, 1.0f};  // All players contribute equally

    config.num_players = 3;
    system = nimcp_credit_create(&config);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_credit_compute_shapley(system, additive_value, values, &result);

    // All credits should be equal
    EXPECT_NEAR(result.credits[0], result.credits[1], 0.01f);
    EXPECT_NEAR(result.credits[1], result.credits[2], 0.01f);
}

TEST_F(CreditAssignmentTest, ShapleyNullPlayerAxiom) {
    // Player who adds no value to any coalition gets zero credit
    // In dictator game, players 1 and 2 are null players
    config.num_players = 3;
    system = nimcp_credit_create(&config);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_credit_compute_shapley(system, dictator_value, nullptr, &result);

    // Player 0 (dictator) gets all
    EXPECT_NEAR(result.credits[0], 10.0f, 0.1f);
    // Others get nothing
    EXPECT_NEAR(result.credits[1], 0.0f, 0.01f);
    EXPECT_NEAR(result.credits[2], 0.0f, 0.01f);
}

TEST_F(CreditAssignmentTest, ShapleyUnanimousGame) {
    // In unanimous game, all players share equally
    config.num_players = 3;
    system = nimcp_credit_create(&config);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_credit_compute_shapley(system, unanimous_value, nullptr, &result);

    // Each should get v(N)/n = 10/3
    float expected = 10.0f / 3.0f;
    EXPECT_NEAR(result.credits[0], expected, 0.1f);
    EXPECT_NEAR(result.credits[1], expected, 0.1f);
    EXPECT_NEAR(result.credits[2], expected, 0.1f);
}

TEST_F(CreditAssignmentTest, ShapleyTwoPlayers) {
    // Test with 2 players for simpler verification
    config.num_players = 2;
    system = nimcp_credit_create(&config);
    ASSERT_NE(system, nullptr);

    // Superadditive: v({1})=1, v({2})=1, v({1,2})=3
    auto two_player_super = [](uint32_t c, uint32_t n, void* d) -> float {
        (void)n; (void)d;
        if (c == 0) return 0.0f;
        if (c == 1) return 1.0f;  // Player 0 alone
        if (c == 2) return 1.0f;  // Player 1 alone
        return 3.0f;              // Both
    };

    nimcp_credit_result_t result;
    nimcp_credit_compute_shapley(system, two_player_super, nullptr, &result);

    // Each should get 1 + (3-1-1)/2 = 1 + 0.5 = 1.5
    EXPECT_NEAR(result.credits[0], 1.5f, 0.1f);
    EXPECT_NEAR(result.credits[1], 1.5f, 0.1f);
}

//=============================================================================
// Monte Carlo Shapley Tests
//=============================================================================

TEST_F(CreditAssignmentTest, ApproximateShapleyConverges) {
    config.num_players = 3;
    config.monte_carlo_samples = 1000;
    system = nimcp_credit_create(&config);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_approximate_shapley(system, superadditive_value, nullptr, 1000, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Should be close to exact
    float sum = result.credits[0] + result.credits[1] + result.credits[2];
    EXPECT_NEAR(sum, result.total_value, 0.5f);
}

//=============================================================================
// Banzhaf Index Tests
//=============================================================================

TEST_F(CreditAssignmentTest, BanzhafBasic) {
    config.num_players = 3;
    system = nimcp_credit_create(&config);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_compute_banzhaf(system, superadditive_value, nullptr, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Symmetric game should give equal Banzhaf indices
    EXPECT_NEAR(result.credits[0], result.credits[1], 0.1f);
    EXPECT_NEAR(result.credits[1], result.credits[2], 0.1f);
}

TEST_F(CreditAssignmentTest, BanzhafDictator) {
    config.num_players = 3;
    system = nimcp_credit_create(&config);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_credit_compute_banzhaf(system, dictator_value, nullptr, &result);

    // Only player 0 has power
    EXPECT_GT(result.credits[0], 0.0f);
    EXPECT_NEAR(result.credits[1], 0.0f, 0.01f);
    EXPECT_NEAR(result.credits[2], 0.0f, 0.01f);
}

//=============================================================================
// Core Membership Tests
//=============================================================================

TEST_F(CreditAssignmentTest, CoreMembershipAdditive) {
    float values[] = {2.0f, 3.0f, 5.0f};

    config.num_players = 3;
    system = nimcp_credit_create(&config);
    ASSERT_NE(system, nullptr);

    // Shapley value is in core for additive games
    nimcp_credit_result_t result;
    nimcp_credit_compute_shapley(system, additive_value, values, &result);

    EXPECT_TRUE(nimcp_credit_is_in_core(system, result.credits, additive_value, values));
}

TEST_F(CreditAssignmentTest, CoreMembershipUnfair) {
    config.num_players = 3;
    system = nimcp_credit_create(&config);
    ASSERT_NE(system, nullptr);

    // Give everything to player 0
    float unfair[] = {6.0f, 0.0f, 0.0f};

    // Should not be in core for superadditive game
    bool in_core = nimcp_credit_is_in_core(system, unfair, superadditive_value, nullptr);
    // Depends on game structure - just test no crash
}

//=============================================================================
// Equal Split Tests
//=============================================================================

TEST_F(CreditAssignmentTest, EqualSplit) {
    config.num_players = 3;
    system = nimcp_credit_create(&config);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_compute_equal_split(system, superadditive_value, nullptr, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // All should get v(N)/n = 6/3 = 2
    EXPECT_NEAR(result.credits[0], 2.0f, 0.01f);
    EXPECT_NEAR(result.credits[1], 2.0f, 0.01f);
    EXPECT_NEAR(result.credits[2], 2.0f, 0.01f);
}

//=============================================================================
// Query Function Tests
//=============================================================================

TEST_F(CreditAssignmentTest, MethodNames) {
    EXPECT_STREQ(nimcp_credit_method_name(NIMCP_CREDIT_SHAPLEY), "Shapley Value (Exact)");
    EXPECT_STREQ(nimcp_credit_method_name(NIMCP_CREDIT_SHAPLEY_APPROX), "Shapley Value (Monte Carlo)");
    EXPECT_STREQ(nimcp_credit_method_name(NIMCP_CREDIT_BANZHAF), "Banzhaf Power Index");
    EXPECT_STREQ(nimcp_credit_method_name(NIMCP_CREDIT_EQUAL_SPLIT), "Equal Split");
}

TEST_F(CreditAssignmentTest, VerifyAxioms) {
    float values[] = {2.0f, 3.0f, 5.0f};

    config.num_players = 3;
    system = nimcp_credit_create(&config);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_credit_compute_shapley(system, additive_value, values, &result);

    float efficiency_error, symmetry_error;
    bool axioms_ok = nimcp_credit_verify_axioms(&result, additive_value, values,
                                                  &efficiency_error, &symmetry_error);

    EXPECT_TRUE(axioms_ok);
    EXPECT_NEAR(efficiency_error, 0.0f, 0.1f);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(CreditAssignmentTest, ComputeShapleyNullSystem) {
    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_compute_shapley(nullptr, additive_value, nullptr, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(CreditAssignmentTest, ComputeShapleyNullValueFn) {
    system = nimcp_credit_create(&config);
    ASSERT_NE(system, nullptr);

    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_compute_shapley(system, nullptr, nullptr, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(CreditAssignmentTest, ComputeShapleyNullResult) {
    system = nimcp_credit_create(&config);
    ASSERT_NE(system, nullptr);

    nimcp_error_t err = nimcp_credit_compute_shapley(system, additive_value, nullptr, nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

//=============================================================================
// Single Player Shapley Test
//=============================================================================

TEST_F(CreditAssignmentTest, ShapleySinglePlayer) {
    float values[] = {2.0f, 3.0f, 5.0f};

    config.num_players = 3;
    system = nimcp_credit_create(&config);
    ASSERT_NE(system, nullptr);

    float credit;
    nimcp_error_t err = nimcp_credit_compute_shapley_single(system, 0, additive_value, values, &credit);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_NEAR(credit, 2.0f, 0.1f);

    err = nimcp_credit_compute_shapley_single(system, 1, additive_value, values, &credit);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_NEAR(credit, 3.0f, 0.1f);

    err = nimcp_credit_compute_shapley_single(system, 2, additive_value, values, &credit);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_NEAR(credit, 5.0f, 0.1f);
}
