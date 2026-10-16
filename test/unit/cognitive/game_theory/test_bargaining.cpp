//=============================================================================
// test_bargaining.cpp - Unit tests for Bargaining Module
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/game_theory/nimcp_bargaining.h"
#include "cognitive/game_theory/nimcp_game_theory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BargainingTest : public ::testing::Test {
protected:
    nimcp_bargaining_t bargaining = nullptr;
    nimcp_bargaining_config_t config;

    void SetUp() override {
        config = nimcp_bargaining_default_config(2);
    }

    void TearDown() override {
        if (bargaining) {
            nimcp_bargaining_destroy(bargaining);
            bargaining = nullptr;
        }
    }

    // Helper to create simple linear feasible set
    void SetLinearFeasibleSet(float total = 1.0f) {
        const uint32_t num_samples = 11;
        nimcp_feasible_point_t feasible[11];

        for (uint32_t i = 0; i < num_samples; i++) {
            float share = static_cast<float>(i) / (num_samples - 1);
            feasible[i].utilities[0] = share * total;
            feasible[i].utilities[1] = (1.0f - share) * total;
        }

        nimcp_bargaining_set_feasible_set(bargaining, feasible, num_samples);
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(BargainingTest, DefaultConfigValues) {
    EXPECT_EQ(config.type, NIMCP_BARGAINING_NASH);
    EXPECT_EQ(config.num_players, 2);
    EXPECT_GT(config.discount_factor, 0.0f);
    EXPECT_LE(config.discount_factor, 1.0f);
    EXPECT_GT(config.max_rounds, 0);

    // Bargaining powers should sum to 1
    float power_sum = config.bargaining_powers[0] + config.bargaining_powers[1];
    EXPECT_NEAR(power_sum, 1.0f, 0.001f);
}

TEST_F(BargainingTest, CreateDestroy) {
    bargaining = nimcp_bargaining_create(&config);
    ASSERT_NE(bargaining, nullptr);

    EXPECT_EQ(nimcp_bargaining_get_state(bargaining), NIMCP_BARGAINING_STATE_INITIALIZED);
    EXPECT_EQ(nimcp_bargaining_get_round(bargaining), 0);
}

TEST_F(BargainingTest, CreateWithNullConfig) {
    bargaining = nimcp_bargaining_create(nullptr);
    // Should use defaults - may succeed or return NULL depending on implementation
    // Just verify no crash
}

//=============================================================================
// Nash Bargaining Solution Tests
//=============================================================================

TEST_F(BargainingTest, NashSolutionSymmetric) {
    config.type = NIMCP_BARGAINING_NASH;
    config.bargaining_powers[0] = 0.5f;
    config.bargaining_powers[1] = 0.5f;
    config.disagreement_payoffs[0] = 0.0f;
    config.disagreement_payoffs[1] = 0.0f;

    bargaining = nimcp_bargaining_create(&config);
    ASSERT_NE(bargaining, nullptr);

    SetLinearFeasibleSet(1.0f);

    nimcp_bargaining_outcome_t outcome;
    nimcp_error_t err = nimcp_bargaining_compute_nash_solution(bargaining, &outcome);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // With symmetric powers and zero disagreement, should split 50-50
    EXPECT_NEAR(outcome.allocations[0], 0.5f, 0.1f);
    EXPECT_NEAR(outcome.allocations[1], 0.5f, 0.1f);

    // Should be Pareto optimal
    EXPECT_TRUE(outcome.is_pareto_optimal);

    // Should be individually rational
    EXPECT_TRUE(outcome.is_individually_rational);
}

TEST_F(BargainingTest, NashSolutionAsymmetricPower) {
    config.type = NIMCP_BARGAINING_NASH;
    config.bargaining_powers[0] = 0.7f;
    config.bargaining_powers[1] = 0.3f;
    config.disagreement_payoffs[0] = 0.0f;
    config.disagreement_payoffs[1] = 0.0f;

    bargaining = nimcp_bargaining_create(&config);
    ASSERT_NE(bargaining, nullptr);

    SetLinearFeasibleSet(1.0f);

    nimcp_bargaining_outcome_t outcome;
    nimcp_error_t err = nimcp_bargaining_compute_nash_solution(bargaining, &outcome);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Player 0 has more power, should get more
    EXPECT_GT(outcome.allocations[0], outcome.allocations[1]);
}

TEST_F(BargainingTest, NashSolutionNonZeroDisagreement) {
    config.type = NIMCP_BARGAINING_NASH;
    config.bargaining_powers[0] = 0.5f;
    config.bargaining_powers[1] = 0.5f;
    config.disagreement_payoffs[0] = 0.2f;
    config.disagreement_payoffs[1] = 0.1f;

    bargaining = nimcp_bargaining_create(&config);
    ASSERT_NE(bargaining, nullptr);

    SetLinearFeasibleSet(1.0f);

    nimcp_bargaining_outcome_t outcome;
    nimcp_error_t err = nimcp_bargaining_compute_nash_solution(bargaining, &outcome);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Both should get at least their disagreement point
    EXPECT_GE(outcome.allocations[0], config.disagreement_payoffs[0]);
    EXPECT_GE(outcome.allocations[1], config.disagreement_payoffs[1]);
}

TEST_F(BargainingTest, NashProductComputation) {
    float allocation[] = {0.6f, 0.4f};
    float disagreement[] = {0.1f, 0.1f};
    float powers[] = {0.5f, 0.5f};

    float product = nimcp_compute_nash_product(allocation, disagreement, powers, 2);

    // (0.6 - 0.1)^0.5 * (0.4 - 0.1)^0.5 = 0.5^0.5 * 0.3^0.5
    float expected = sqrtf(0.5f) * sqrtf(0.3f);
    EXPECT_NEAR(product, expected, 0.01f);
}

//=============================================================================
// Kalai-Smorodinsky Solution Tests
//=============================================================================

TEST_F(BargainingTest, KalaiSmorodinskySymmetric) {
    config.type = NIMCP_BARGAINING_KALAI_SMORODINSKY;
    config.disagreement_payoffs[0] = 0.0f;
    config.disagreement_payoffs[1] = 0.0f;

    bargaining = nimcp_bargaining_create(&config);
    ASSERT_NE(bargaining, nullptr);

    SetLinearFeasibleSet(1.0f);

    nimcp_bargaining_outcome_t outcome;
    nimcp_error_t err = nimcp_bargaining_compute_kalai_smorodinsky(bargaining, &outcome);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // With symmetric feasible set, should split evenly
    EXPECT_NEAR(outcome.allocations[0], 0.5f, 0.1f);
    EXPECT_NEAR(outcome.allocations[1], 0.5f, 0.1f);
}

//=============================================================================
// Egalitarian Solution Tests
//=============================================================================

TEST_F(BargainingTest, EgalitarianSymmetric) {
    config.type = NIMCP_BARGAINING_EGALITARIAN;
    config.disagreement_payoffs[0] = 0.0f;
    config.disagreement_payoffs[1] = 0.0f;

    bargaining = nimcp_bargaining_create(&config);
    ASSERT_NE(bargaining, nullptr);

    SetLinearFeasibleSet(1.0f);

    nimcp_bargaining_outcome_t outcome;
    nimcp_error_t err = nimcp_bargaining_compute_egalitarian(bargaining, &outcome);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Egalitarian maximizes minimum - should split evenly for symmetric set
    EXPECT_NEAR(outcome.allocations[0], outcome.allocations[1], 0.1f);
}

//=============================================================================
// Alternating Offers Tests
//=============================================================================

TEST_F(BargainingTest, MakeOffer) {
    config.type = NIMCP_BARGAINING_RUBINSTEIN;
    bargaining = nimcp_bargaining_create(&config);
    ASSERT_NE(bargaining, nullptr);

    float offer[] = {0.6f, 0.4f};
    nimcp_error_t err = nimcp_bargaining_make_offer(bargaining, 0, offer);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(nimcp_bargaining_get_state(bargaining), NIMCP_BARGAINING_STATE_NEGOTIATING);
}

TEST_F(BargainingTest, RespondAccept) {
    config.type = NIMCP_BARGAINING_RUBINSTEIN;
    bargaining = nimcp_bargaining_create(&config);
    ASSERT_NE(bargaining, nullptr);

    float offer[] = {0.5f, 0.5f};
    nimcp_bargaining_make_offer(bargaining, 0, offer);

    nimcp_error_t err = nimcp_bargaining_respond(bargaining, 1, true);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_TRUE(nimcp_bargaining_has_agreement(bargaining));
    EXPECT_EQ(nimcp_bargaining_get_state(bargaining), NIMCP_BARGAINING_STATE_AGREED);
}

TEST_F(BargainingTest, RespondReject) {
    config.type = NIMCP_BARGAINING_RUBINSTEIN;
    bargaining = nimcp_bargaining_create(&config);
    ASSERT_NE(bargaining, nullptr);

    float offer[] = {0.9f, 0.1f};  // Unfair offer
    nimcp_bargaining_make_offer(bargaining, 0, offer);

    nimcp_error_t err = nimcp_bargaining_respond(bargaining, 1, false);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_FALSE(nimcp_bargaining_has_agreement(bargaining));
}

TEST_F(BargainingTest, GetCurrentOffer) {
    config.type = NIMCP_BARGAINING_RUBINSTEIN;
    bargaining = nimcp_bargaining_create(&config);
    ASSERT_NE(bargaining, nullptr);

    float offer[] = {0.7f, 0.3f};
    nimcp_bargaining_make_offer(bargaining, 0, offer);

    nimcp_offer_t retrieved;
    nimcp_error_t err = nimcp_bargaining_get_current_offer(bargaining, &retrieved);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(retrieved.proposer, 0);
    EXPECT_NEAR(retrieved.proposed_allocation[0], 0.7f, 0.001f);
    EXPECT_NEAR(retrieved.proposed_allocation[1], 0.3f, 0.001f);
}

//=============================================================================
// Query Function Tests
//=============================================================================

TEST_F(BargainingTest, BargainingTypeNames) {
    EXPECT_STREQ(nimcp_bargaining_type_name(NIMCP_BARGAINING_NASH), "Nash Bargaining");
    EXPECT_STREQ(nimcp_bargaining_type_name(NIMCP_BARGAINING_KALAI_SMORODINSKY), "Kalai-Smorodinsky");
    EXPECT_STREQ(nimcp_bargaining_type_name(NIMCP_BARGAINING_EGALITARIAN), "Egalitarian");
    EXPECT_STREQ(nimcp_bargaining_type_name(NIMCP_BARGAINING_RUBINSTEIN), "Rubinstein (Alternating Offers)");
}

TEST_F(BargainingTest, GetOutcome) {
    config.type = NIMCP_BARGAINING_NASH;
    bargaining = nimcp_bargaining_create(&config);
    ASSERT_NE(bargaining, nullptr);

    SetLinearFeasibleSet(1.0f);

    // Compute Nash solution first
    nimcp_bargaining_outcome_t computed_outcome;
    nimcp_error_t err = nimcp_bargaining_compute_nash_solution(bargaining, &computed_outcome);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Now get the stored outcome
    nimcp_bargaining_outcome_t outcome;
    err = nimcp_bargaining_get_outcome(bargaining, &outcome);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Allocations should sum to total
    float sum = outcome.allocations[0] + outcome.allocations[1];
    EXPECT_NEAR(sum, 1.0f, 0.01f);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(BargainingTest, ComputeNashNullBargaining) {
    nimcp_bargaining_outcome_t outcome;
    nimcp_error_t err = nimcp_bargaining_compute_nash_solution(nullptr, &outcome);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(BargainingTest, MakeOfferNullBargaining) {
    float offer[] = {0.5f, 0.5f};
    nimcp_error_t err = nimcp_bargaining_make_offer(nullptr, 0, offer);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(BargainingTest, MakeOfferNullAllocation) {
    bargaining = nimcp_bargaining_create(&config);
    ASSERT_NE(bargaining, nullptr);

    nimcp_error_t err = nimcp_bargaining_make_offer(bargaining, 0, nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

//=============================================================================
// Property Tests (Nash Axioms)
//=============================================================================

TEST_F(BargainingTest, NashAxiomIndividualRationality) {
    // All solutions should give each player at least their disagreement point
    config.type = NIMCP_BARGAINING_NASH;
    config.disagreement_payoffs[0] = 0.2f;
    config.disagreement_payoffs[1] = 0.15f;

    bargaining = nimcp_bargaining_create(&config);
    ASSERT_NE(bargaining, nullptr);

    SetLinearFeasibleSet(1.0f);

    nimcp_bargaining_outcome_t outcome;
    nimcp_bargaining_compute_nash_solution(bargaining, &outcome);

    EXPECT_GE(outcome.utilities[0], config.disagreement_payoffs[0] - 0.01f);
    EXPECT_GE(outcome.utilities[1], config.disagreement_payoffs[1] - 0.01f);
}

TEST_F(BargainingTest, NashAxiomParetoOptimality) {
    // Nash solution should be Pareto optimal
    config.type = NIMCP_BARGAINING_NASH;
    bargaining = nimcp_bargaining_create(&config);
    ASSERT_NE(bargaining, nullptr);

    SetLinearFeasibleSet(1.0f);

    nimcp_bargaining_outcome_t outcome;
    nimcp_bargaining_compute_nash_solution(bargaining, &outcome);

    EXPECT_TRUE(outcome.is_pareto_optimal);
}
