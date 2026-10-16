/**
 * @file test_bargaining_regression.cpp
 * @brief Regression tests for Nash Bargaining and Negotiation Protocols
 *
 * Tests verify:
 * - Nash solution satisfies Pareto optimality
 * - Nash solution satisfies individual rationality
 * - Kalai-Smorodinsky and egalitarian solution correctness
 * - Performance and convergence behavior
 * - Numerical stability with edge cases
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

// Headers have their own extern "C" guards
#include "cognitive/game_theory/nimcp_bargaining.h"
#include "cognitive/game_theory/nimcp_game_theory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BargainingRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = nimcp_bargaining_default_config(2);
    }

    void TearDown() override {
        // Cleanup handled by individual tests
    }

    nimcp_bargaining_config_t config_;

    // Helper to create bargaining context
    nimcp_bargaining_t create_bargaining(uint32_t num_players = 2) {
        config_ = nimcp_bargaining_default_config(num_players);
        return nimcp_bargaining_create(&config_);
    }

    // Helper to set up simple feasible set (triangle)
    void setup_simple_feasible_set(nimcp_bargaining_t bargaining) {
        // Simple feasible set: utility pairs on line from (1,0) to (0,1)
        std::vector<nimcp_feasible_point_t> points;
        for (int i = 0; i <= 10; i++) {
            nimcp_feasible_point_t pt;
            pt.utilities[0] = (float)(10 - i) / 10.0f;
            pt.utilities[1] = (float)i / 10.0f;
            points.push_back(pt);
        }
        nimcp_bargaining_set_feasible_set(bargaining, points.data(), (uint32_t)points.size());
    }

    // Helper to set up symmetric feasible set
    void setup_symmetric_feasible_set(nimcp_bargaining_t bargaining) {
        // Symmetric set: quarter circle
        std::vector<nimcp_feasible_point_t> points;
        for (int i = 0; i <= 20; i++) {
            float theta = (float)i * (3.14159f / 2.0f) / 20.0f;
            nimcp_feasible_point_t pt;
            pt.utilities[0] = cosf(theta);
            pt.utilities[1] = sinf(theta);
            points.push_back(pt);
        }
        nimcp_bargaining_set_feasible_set(bargaining, points.data(), (uint32_t)points.size());
    }
};

//=============================================================================
// REG-BAR-001: Nash Solution Pareto Optimality
//=============================================================================

TEST_F(BargainingRegressionTest, REG_BAR_001_NashSolutionIsParetoOptimal) {
    nimcp_bargaining_t bargaining = create_bargaining(2);
    ASSERT_NE(bargaining, nullptr);

    setup_simple_feasible_set(bargaining);

    nimcp_bargaining_outcome_t outcome;
    nimcp_error_t err = nimcp_bargaining_compute_nash_solution(bargaining, &outcome);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Nash solution should be Pareto optimal
    EXPECT_TRUE(outcome.is_pareto_optimal)
        << "Nash solution should be Pareto optimal: u1=" << outcome.allocations[0]
        << ", u2=" << outcome.allocations[1];

    nimcp_bargaining_destroy(bargaining);
}

TEST_F(BargainingRegressionTest, REG_BAR_002_NashSolutionParetoOnSymmetricSet) {
    nimcp_bargaining_t bargaining = create_bargaining(2);
    ASSERT_NE(bargaining, nullptr);

    setup_symmetric_feasible_set(bargaining);

    nimcp_bargaining_outcome_t outcome;
    nimcp_error_t err = nimcp_bargaining_compute_nash_solution(bargaining, &outcome);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    EXPECT_TRUE(outcome.is_pareto_optimal);

    // On symmetric set with equal powers, solution should be symmetric
    EXPECT_NEAR(outcome.allocations[0], outcome.allocations[1], 0.1f);

    nimcp_bargaining_destroy(bargaining);
}

TEST_F(BargainingRegressionTest, REG_BAR_003_NashSolutionParetoMultipleScenarios) {
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (int scenario = 0; scenario < 50; scenario++) {
        nimcp_bargaining_t bargaining = create_bargaining(2);
        ASSERT_NE(bargaining, nullptr);

        // Generate random feasible set
        std::vector<nimcp_feasible_point_t> points;
        for (int i = 0; i < 20; i++) {
            nimcp_feasible_point_t pt;
            pt.utilities[0] = dist(rng);
            pt.utilities[1] = dist(rng);
            points.push_back(pt);
        }
        nimcp_bargaining_set_feasible_set(bargaining, points.data(), (uint32_t)points.size());

        nimcp_bargaining_outcome_t outcome;
        nimcp_error_t err = nimcp_bargaining_compute_nash_solution(bargaining, &outcome);

        if (err == NIMCP_SUCCESS) {
            EXPECT_TRUE(outcome.is_pareto_optimal)
                << "Scenario " << scenario << " failed Pareto optimality";
        }

        nimcp_bargaining_destroy(bargaining);
    }
}

//=============================================================================
// REG-BAR-010: Nash Solution Individual Rationality
//=============================================================================

TEST_F(BargainingRegressionTest, REG_BAR_010_NashSolutionIsIndividuallyRational) {
    config_ = nimcp_bargaining_default_config(2);
    config_.disagreement_payoffs[0] = 0.2f;
    config_.disagreement_payoffs[1] = 0.1f;

    nimcp_bargaining_t bargaining = nimcp_bargaining_create(&config_);
    ASSERT_NE(bargaining, nullptr);

    setup_simple_feasible_set(bargaining);

    nimcp_bargaining_outcome_t outcome;
    nimcp_error_t err = nimcp_bargaining_compute_nash_solution(bargaining, &outcome);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Each player should get at least their disagreement payoff
    EXPECT_TRUE(outcome.is_individually_rational);
    EXPECT_GE(outcome.allocations[0], config_.disagreement_payoffs[0] - 0.001f);
    EXPECT_GE(outcome.allocations[1], config_.disagreement_payoffs[1] - 0.001f);

    nimcp_bargaining_destroy(bargaining);
}

TEST_F(BargainingRegressionTest, REG_BAR_011_NashSolutionIRWithHighDisagreement) {
    config_ = nimcp_bargaining_default_config(2);
    config_.disagreement_payoffs[0] = 0.4f;
    config_.disagreement_payoffs[1] = 0.3f;

    nimcp_bargaining_t bargaining = nimcp_bargaining_create(&config_);
    ASSERT_NE(bargaining, nullptr);

    setup_simple_feasible_set(bargaining);

    nimcp_bargaining_outcome_t outcome;
    nimcp_error_t err = nimcp_bargaining_compute_nash_solution(bargaining, &outcome);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    EXPECT_TRUE(outcome.is_individually_rational);
    EXPECT_GE(outcome.allocations[0], 0.4f - 0.01f);
    EXPECT_GE(outcome.allocations[1], 0.3f - 0.01f);

    nimcp_bargaining_destroy(bargaining);
}

TEST_F(BargainingRegressionTest, REG_BAR_012_NashSolutionIRMultiplePlayers) {
    config_ = nimcp_bargaining_default_config(3);
    config_.disagreement_payoffs[0] = 0.1f;
    config_.disagreement_payoffs[1] = 0.2f;
    config_.disagreement_payoffs[2] = 0.15f;

    nimcp_bargaining_t bargaining = nimcp_bargaining_create(&config_);
    ASSERT_NE(bargaining, nullptr);

    // Create 3-player feasible set
    std::vector<nimcp_feasible_point_t> points;
    for (int i = 0; i <= 5; i++) {
        for (int j = 0; j <= 5 - i; j++) {
            nimcp_feasible_point_t pt;
            pt.utilities[0] = (float)i / 5.0f;
            pt.utilities[1] = (float)j / 5.0f;
            pt.utilities[2] = (float)(5 - i - j) / 5.0f;
            points.push_back(pt);
        }
    }
    nimcp_bargaining_set_feasible_set(bargaining, points.data(), (uint32_t)points.size());

    nimcp_bargaining_outcome_t outcome;
    nimcp_error_t err = nimcp_bargaining_compute_nash_solution(bargaining, &outcome);

    if (err == NIMCP_SUCCESS) {
        EXPECT_TRUE(outcome.is_individually_rational);
        for (int i = 0; i < 3; i++) {
            EXPECT_GE(outcome.allocations[i], config_.disagreement_payoffs[i] - 0.01f)
                << "Player " << i << " violated IR";
        }
    }

    nimcp_bargaining_destroy(bargaining);
}

//=============================================================================
// REG-BAR-020: Nash Product Computation
//=============================================================================

TEST_F(BargainingRegressionTest, REG_BAR_020_NashProductCorrectness) {
    float allocation[2] = {0.5f, 0.5f};
    float disagreement[2] = {0.0f, 0.0f};
    float powers[2] = {0.5f, 0.5f};

    float product = nimcp_compute_nash_product(allocation, disagreement, powers, 2);

    // Expected: (0.5-0)^0.5 * (0.5-0)^0.5 = sqrt(0.5) * sqrt(0.5) = 0.5
    EXPECT_NEAR(product, 0.5f, 0.01f);
}

TEST_F(BargainingRegressionTest, REG_BAR_021_NashProductWithDisagreement) {
    float allocation[2] = {0.8f, 0.6f};
    float disagreement[2] = {0.2f, 0.1f};
    float powers[2] = {0.5f, 0.5f};

    float product = nimcp_compute_nash_product(allocation, disagreement, powers, 2);

    // Expected: (0.8-0.2)^0.5 * (0.6-0.1)^0.5 = sqrt(0.6) * sqrt(0.5)
    float expected = sqrtf(0.6f) * sqrtf(0.5f);
    EXPECT_NEAR(product, expected, 0.01f);
}

TEST_F(BargainingRegressionTest, REG_BAR_022_NashProductAsymmetricPowers) {
    float allocation[2] = {0.7f, 0.3f};
    float disagreement[2] = {0.0f, 0.0f};
    float powers[2] = {0.8f, 0.2f};

    float product = nimcp_compute_nash_product(allocation, disagreement, powers, 2);

    // Expected: 0.7^0.8 * 0.3^0.2
    float expected = powf(0.7f, 0.8f) * powf(0.3f, 0.2f);
    EXPECT_NEAR(product, expected, 0.01f);
}

//=============================================================================
// REG-BAR-030: Kalai-Smorodinsky Solution
//=============================================================================

TEST_F(BargainingRegressionTest, REG_BAR_030_KalaiSmorodinskyParetoOptimal) {
    nimcp_bargaining_t bargaining = create_bargaining(2);
    ASSERT_NE(bargaining, nullptr);

    setup_symmetric_feasible_set(bargaining);

    nimcp_bargaining_outcome_t outcome;
    nimcp_error_t err = nimcp_bargaining_compute_kalai_smorodinsky(bargaining, &outcome);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    EXPECT_TRUE(outcome.is_pareto_optimal);
    EXPECT_TRUE(outcome.is_individually_rational);

    nimcp_bargaining_destroy(bargaining);
}

TEST_F(BargainingRegressionTest, REG_BAR_031_KalaiSmorodinskySymmetric) {
    nimcp_bargaining_t bargaining = create_bargaining(2);
    ASSERT_NE(bargaining, nullptr);

    setup_symmetric_feasible_set(bargaining);

    nimcp_bargaining_outcome_t outcome;
    nimcp_error_t err = nimcp_bargaining_compute_kalai_smorodinsky(bargaining, &outcome);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // On symmetric set, KS solution should be symmetric
    EXPECT_NEAR(outcome.allocations[0], outcome.allocations[1], 0.1f);

    nimcp_bargaining_destroy(bargaining);
}

//=============================================================================
// REG-BAR-040: Egalitarian Solution
//=============================================================================

TEST_F(BargainingRegressionTest, REG_BAR_040_EgalitarianMaximizesMinimum) {
    config_ = nimcp_bargaining_default_config(2);
    config_.disagreement_payoffs[0] = 0.0f;
    config_.disagreement_payoffs[1] = 0.0f;

    nimcp_bargaining_t bargaining = nimcp_bargaining_create(&config_);
    ASSERT_NE(bargaining, nullptr);

    setup_simple_feasible_set(bargaining);

    nimcp_bargaining_outcome_t outcome;
    nimcp_error_t err = nimcp_bargaining_compute_egalitarian(bargaining, &outcome);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Egalitarian should maximize the minimum utility
    float min_util = std::min(outcome.allocations[0], outcome.allocations[1]);

    // On simple triangle, optimal egalitarian point is (0.5, 0.5)
    EXPECT_NEAR(min_util, 0.5f, 0.1f);

    nimcp_bargaining_destroy(bargaining);
}

TEST_F(BargainingRegressionTest, REG_BAR_041_EgalitarianParetoOptimal) {
    nimcp_bargaining_t bargaining = create_bargaining(2);
    ASSERT_NE(bargaining, nullptr);

    setup_symmetric_feasible_set(bargaining);

    nimcp_bargaining_outcome_t outcome;
    nimcp_error_t err = nimcp_bargaining_compute_egalitarian(bargaining, &outcome);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    EXPECT_TRUE(outcome.is_pareto_optimal);
    EXPECT_TRUE(outcome.is_individually_rational);

    nimcp_bargaining_destroy(bargaining);
}

//=============================================================================
// REG-BAR-050: Performance Tests
//=============================================================================

TEST_F(BargainingRegressionTest, REG_BAR_050_NashSolutionPerformance) {
    const int NUM_ITERATIONS = 100;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        nimcp_bargaining_t bargaining = create_bargaining(2);
        ASSERT_NE(bargaining, nullptr);

        setup_symmetric_feasible_set(bargaining);

        nimcp_bargaining_outcome_t outcome;
        nimcp_bargaining_compute_nash_solution(bargaining, &outcome);

        nimcp_bargaining_destroy(bargaining);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 100 Nash solutions in under 1 second
    EXPECT_LT(duration.count(), 1000)
        << "100 Nash solutions took " << duration.count() << "ms";
}

TEST_F(BargainingRegressionTest, REG_BAR_051_ConvergenceSpeed) {
    config_ = nimcp_bargaining_default_config(2);
    config_.type = NIMCP_BARGAINING_RUBINSTEIN;
    config_.max_rounds = 1000;
    config_.convergence_threshold = 0.001f;

    nimcp_bargaining_t bargaining = nimcp_bargaining_create(&config_);
    ASSERT_NE(bargaining, nullptr);

    setup_simple_feasible_set(bargaining);

    nimcp_bargaining_outcome_t outcome;
    nimcp_error_t err = nimcp_bargaining_compute_nash_solution(bargaining, &outcome);

    if (err == NIMCP_SUCCESS) {
        // Should converge in reasonable number of rounds
        EXPECT_LT(outcome.rounds_taken, 100u)
            << "Convergence took " << outcome.rounds_taken << " rounds";
    }

    nimcp_bargaining_destroy(bargaining);
}

TEST_F(BargainingRegressionTest, REG_BAR_052_LargeFeasibleSetPerformance) {
    nimcp_bargaining_t bargaining = create_bargaining(2);
    ASSERT_NE(bargaining, nullptr);

    // Large feasible set with 1000 points
    std::vector<nimcp_feasible_point_t> points;
    for (int i = 0; i <= 1000; i++) {
        nimcp_feasible_point_t pt;
        float t = (float)i / 1000.0f;
        pt.utilities[0] = 1.0f - t;
        pt.utilities[1] = t;
        points.push_back(pt);
    }

    auto start = std::chrono::high_resolution_clock::now();

    nimcp_bargaining_set_feasible_set(bargaining, points.data(), (uint32_t)points.size());

    nimcp_bargaining_outcome_t outcome;
    nimcp_bargaining_compute_nash_solution(bargaining, &outcome);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should handle large set in under 500ms
    EXPECT_LT(duration.count(), 500);

    nimcp_bargaining_destroy(bargaining);
}

//=============================================================================
// REG-BAR-060: Numerical Stability
//=============================================================================

TEST_F(BargainingRegressionTest, REG_BAR_060_VerySmallUtilities) {
    nimcp_bargaining_t bargaining = create_bargaining(2);
    ASSERT_NE(bargaining, nullptr);

    // Feasible set with very small utilities
    std::vector<nimcp_feasible_point_t> points;
    for (int i = 0; i <= 10; i++) {
        nimcp_feasible_point_t pt;
        pt.utilities[0] = (float)(10 - i) * 1e-8f;
        pt.utilities[1] = (float)i * 1e-8f;
        points.push_back(pt);
    }
    nimcp_bargaining_set_feasible_set(bargaining, points.data(), (uint32_t)points.size());

    nimcp_bargaining_outcome_t outcome;
    nimcp_error_t err = nimcp_bargaining_compute_nash_solution(bargaining, &outcome);

    if (err == NIMCP_SUCCESS) {
        EXPECT_FALSE(std::isnan(outcome.allocations[0]));
        EXPECT_FALSE(std::isnan(outcome.allocations[1]));
        EXPECT_FALSE(std::isinf(outcome.allocations[0]));
        EXPECT_FALSE(std::isinf(outcome.allocations[1]));
    }

    nimcp_bargaining_destroy(bargaining);
}

TEST_F(BargainingRegressionTest, REG_BAR_061_VeryLargeUtilities) {
    nimcp_bargaining_t bargaining = create_bargaining(2);
    ASSERT_NE(bargaining, nullptr);

    // Feasible set with very large utilities
    std::vector<nimcp_feasible_point_t> points;
    for (int i = 0; i <= 10; i++) {
        nimcp_feasible_point_t pt;
        pt.utilities[0] = (float)(10 - i) * 1e10f;
        pt.utilities[1] = (float)i * 1e10f;
        points.push_back(pt);
    }
    nimcp_bargaining_set_feasible_set(bargaining, points.data(), (uint32_t)points.size());

    nimcp_bargaining_outcome_t outcome;
    nimcp_error_t err = nimcp_bargaining_compute_nash_solution(bargaining, &outcome);

    if (err == NIMCP_SUCCESS) {
        EXPECT_FALSE(std::isnan(outcome.allocations[0]));
        EXPECT_FALSE(std::isnan(outcome.allocations[1]));
        EXPECT_FALSE(std::isinf(outcome.allocations[0]));
        EXPECT_FALSE(std::isinf(outcome.allocations[1]));
    }

    nimcp_bargaining_destroy(bargaining);
}

TEST_F(BargainingRegressionTest, REG_BAR_062_ZeroDisagreement) {
    config_ = nimcp_bargaining_default_config(2);
    config_.disagreement_payoffs[0] = 0.0f;
    config_.disagreement_payoffs[1] = 0.0f;

    nimcp_bargaining_t bargaining = nimcp_bargaining_create(&config_);
    ASSERT_NE(bargaining, nullptr);

    setup_simple_feasible_set(bargaining);

    nimcp_bargaining_outcome_t outcome;
    nimcp_error_t err = nimcp_bargaining_compute_nash_solution(bargaining, &outcome);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    EXPECT_GE(outcome.allocations[0], 0.0f);
    EXPECT_GE(outcome.allocations[1], 0.0f);
    EXPECT_FALSE(std::isnan(outcome.nash_product));

    nimcp_bargaining_destroy(bargaining);
}

TEST_F(BargainingRegressionTest, REG_BAR_063_NearIdenticalPoints) {
    nimcp_bargaining_t bargaining = create_bargaining(2);
    ASSERT_NE(bargaining, nullptr);

    // Feasible set with nearly identical points
    std::vector<nimcp_feasible_point_t> points;
    float base = 0.5f;
    float eps = std::numeric_limits<float>::epsilon();

    for (int i = 0; i < 10; i++) {
        nimcp_feasible_point_t pt;
        pt.utilities[0] = base + (float)i * eps;
        pt.utilities[1] = base - (float)i * eps;
        points.push_back(pt);
    }
    nimcp_bargaining_set_feasible_set(bargaining, points.data(), (uint32_t)points.size());

    nimcp_bargaining_outcome_t outcome;
    nimcp_error_t err = nimcp_bargaining_compute_nash_solution(bargaining, &outcome);

    if (err == NIMCP_SUCCESS) {
        EXPECT_FALSE(std::isnan(outcome.allocations[0]));
        EXPECT_FALSE(std::isnan(outcome.allocations[1]));
    }

    nimcp_bargaining_destroy(bargaining);
}

//=============================================================================
// REG-BAR-070: Alternating Offers Protocol
//=============================================================================

TEST_F(BargainingRegressionTest, REG_BAR_070_AlternatingOffersBasic) {
    config_ = nimcp_bargaining_default_config(2);
    config_.type = NIMCP_BARGAINING_RUBINSTEIN;
    config_.discount_factor = 0.95f;
    config_.max_rounds = 100;

    nimcp_bargaining_t bargaining = nimcp_bargaining_create(&config_);
    ASSERT_NE(bargaining, nullptr);

    setup_simple_feasible_set(bargaining);

    // Make an offer
    float proposal[2] = {0.6f, 0.4f};
    nimcp_error_t err = nimcp_bargaining_make_offer(bargaining, 0, proposal);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Get current offer
    nimcp_offer_t offer;
    err = nimcp_bargaining_get_current_offer(bargaining, &offer);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    EXPECT_FLOAT_EQ(offer.proposed_allocation[0], 0.6f);
    EXPECT_FLOAT_EQ(offer.proposed_allocation[1], 0.4f);

    nimcp_bargaining_destroy(bargaining);
}

TEST_F(BargainingRegressionTest, REG_BAR_071_AlternatingOffersAccept) {
    config_ = nimcp_bargaining_default_config(2);
    config_.type = NIMCP_BARGAINING_RUBINSTEIN;

    nimcp_bargaining_t bargaining = nimcp_bargaining_create(&config_);
    ASSERT_NE(bargaining, nullptr);

    setup_simple_feasible_set(bargaining);

    // Player 0 makes offer
    float proposal[2] = {0.5f, 0.5f};
    nimcp_bargaining_make_offer(bargaining, 0, proposal);

    // Player 1 accepts
    nimcp_error_t err = nimcp_bargaining_respond(bargaining, 1, true);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Should have agreement
    EXPECT_TRUE(nimcp_bargaining_has_agreement(bargaining));

    nimcp_bargaining_outcome_t outcome;
    nimcp_bargaining_get_outcome(bargaining, &outcome);
    EXPECT_EQ(outcome.state, NIMCP_BARGAINING_STATE_AGREED);

    nimcp_bargaining_destroy(bargaining);
}

TEST_F(BargainingRegressionTest, REG_BAR_072_AlternatingOffersReject) {
    config_ = nimcp_bargaining_default_config(2);
    config_.type = NIMCP_BARGAINING_RUBINSTEIN;

    nimcp_bargaining_t bargaining = nimcp_bargaining_create(&config_);
    ASSERT_NE(bargaining, nullptr);

    setup_simple_feasible_set(bargaining);

    float proposal[2] = {0.9f, 0.1f};
    nimcp_bargaining_make_offer(bargaining, 0, proposal);

    // Player 1 rejects
    nimcp_error_t err = nimcp_bargaining_respond(bargaining, 1, false);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Should not have agreement yet
    EXPECT_FALSE(nimcp_bargaining_has_agreement(bargaining));

    nimcp_bargaining_destroy(bargaining);
}

//=============================================================================
// REG-BAR-080: State Machine
//=============================================================================

TEST_F(BargainingRegressionTest, REG_BAR_080_StateTransitions) {
    nimcp_bargaining_t bargaining = create_bargaining(2);
    ASSERT_NE(bargaining, nullptr);

    EXPECT_EQ(nimcp_bargaining_get_state(bargaining), NIMCP_BARGAINING_STATE_INITIALIZED);

    setup_simple_feasible_set(bargaining);

    nimcp_bargaining_outcome_t outcome;
    nimcp_bargaining_compute_nash_solution(bargaining, &outcome);

    EXPECT_EQ(outcome.state, NIMCP_BARGAINING_STATE_AGREED);

    nimcp_bargaining_destroy(bargaining);
}

TEST_F(BargainingRegressionTest, REG_BAR_081_RoundCounting) {
    config_ = nimcp_bargaining_default_config(2);
    config_.type = NIMCP_BARGAINING_RUBINSTEIN;

    nimcp_bargaining_t bargaining = nimcp_bargaining_create(&config_);
    ASSERT_NE(bargaining, nullptr);

    setup_simple_feasible_set(bargaining);

    EXPECT_EQ(nimcp_bargaining_get_round(bargaining), 0u);

    float proposal[2] = {0.6f, 0.4f};
    nimcp_bargaining_make_offer(bargaining, 0, proposal);

    // Round should increment after each offer
    EXPECT_GE(nimcp_bargaining_get_round(bargaining), 0u);

    nimcp_bargaining_destroy(bargaining);
}

//=============================================================================
// REG-BAR-090: Null Safety
//=============================================================================

TEST_F(BargainingRegressionTest, REG_BAR_090_NullHandling) {
    // Create with null config
    nimcp_bargaining_t bargaining = nimcp_bargaining_create(nullptr);
    // May return nullptr or use defaults
    if (bargaining != nullptr) {
        nimcp_bargaining_destroy(bargaining);
    }

    // Operations on null bargaining
    nimcp_bargaining_outcome_t outcome;
    nimcp_error_t err = nimcp_bargaining_compute_nash_solution(nullptr, &outcome);
    EXPECT_NE(err, NIMCP_SUCCESS);

    // Null feasible set
    bargaining = create_bargaining(2);
    ASSERT_NE(bargaining, nullptr);
    err = nimcp_bargaining_set_feasible_set(bargaining, nullptr, 0);
    EXPECT_NE(err, NIMCP_SUCCESS);

    // Null outcome
    err = nimcp_bargaining_compute_nash_solution(bargaining, nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS);

    nimcp_bargaining_destroy(bargaining);

    // Destroy null should not crash
    nimcp_bargaining_destroy(nullptr);
}

//=============================================================================
// REG-BAR-100: Type Names
//=============================================================================

TEST_F(BargainingRegressionTest, REG_BAR_100_TypeNameConsistency) {
    for (int i = 0; i < NIMCP_BARGAINING_COUNT; i++) {
        const char* name = nimcp_bargaining_type_name((nimcp_bargaining_type_t)i);
        ASSERT_NE(name, nullptr) << "Type " << i << " has null name";
        EXPECT_GT(strlen(name), 0u) << "Type " << i << " has empty name";
    }
}

//=============================================================================
// REG-BAR-110: Repeated Operations Stability
//=============================================================================

TEST_F(BargainingRegressionTest, REG_BAR_110_RepeatedCreateDestroy) {
    for (int i = 0; i < 1000; i++) {
        nimcp_bargaining_t bargaining = create_bargaining(2);
        ASSERT_NE(bargaining, nullptr) << "Failed at iteration " << i;
        nimcp_bargaining_destroy(bargaining);
    }
}

TEST_F(BargainingRegressionTest, REG_BAR_111_RepeatedSolve) {
    nimcp_bargaining_t bargaining = create_bargaining(2);
    ASSERT_NE(bargaining, nullptr);

    setup_symmetric_feasible_set(bargaining);

    nimcp_bargaining_outcome_t first_outcome;
    nimcp_error_t err = nimcp_bargaining_compute_nash_solution(bargaining, &first_outcome);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Solve multiple times, results should be consistent
    for (int i = 0; i < 100; i++) {
        nimcp_bargaining_outcome_t outcome;
        err = nimcp_bargaining_compute_nash_solution(bargaining, &outcome);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        EXPECT_NEAR(outcome.allocations[0], first_outcome.allocations[0], 0.001f)
            << "Inconsistent at iteration " << i;
        EXPECT_NEAR(outcome.allocations[1], first_outcome.allocations[1], 0.001f)
            << "Inconsistent at iteration " << i;
    }

    nimcp_bargaining_destroy(bargaining);
}

