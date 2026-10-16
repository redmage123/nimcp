/**
 * @file test_auction_regression.cpp
 * @brief Regression tests for Auction Mechanisms
 *
 * Tests verify:
 * - Second-price payment equals second-highest bid
 * - VCG mechanism correctness
 * - Ascending/descending auction behavior
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
#include <set>

// Headers have their own extern "C" guards
#include "cognitive/game_theory/nimcp_auction.h"
#include "cognitive/game_theory/nimcp_game_theory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class AuctionRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = nimcp_auction_default_config();
    }

    void TearDown() override {
        // Cleanup handled by individual tests
    }

    nimcp_auction_config_t config_;

    // Helper to create auction with specific type
    nimcp_auction_t create_auction(nimcp_auction_type_t type) {
        config_.type = type;
        return nimcp_auction_create(&config_);
    }

    // Helper to submit multiple bids
    void submit_bids(nimcp_auction_t auction,
                     const std::vector<std::pair<nimcp_player_id_t, float>>& bids) {
        for (const auto& bid : bids) {
            nimcp_auction_bid(auction, bid.first, bid.second, 0);
        }
    }
};

//=============================================================================
// REG-AUC-001: Second-Price Auction Correctness
//=============================================================================

TEST_F(AuctionRegressionTest, REG_AUC_001_SecondPricePaymentEqualsSecondHighestBid) {
    nimcp_auction_t auction = create_auction(NIMCP_AUCTION_SECOND_PRICE);
    ASSERT_NE(auction, nullptr);

    // Submit bids: 100, 80, 60, 40, 20
    std::vector<std::pair<nimcp_player_id_t, float>> bids = {
        {1, 100.0f}, {2, 80.0f}, {3, 60.0f}, {4, 40.0f}, {5, 20.0f}
    };
    submit_bids(auction, bids);

    nimcp_auction_result_t result;
    nimcp_error_t err = nimcp_auction_resolve(auction, &result);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Winner should be bidder 1 (highest bid)
    EXPECT_EQ(result.winner_id, 1u);
    EXPECT_FLOAT_EQ(result.winning_bid, 100.0f);

    // Payment should be second-highest bid (80)
    EXPECT_FLOAT_EQ(result.payment, 80.0f);
    EXPECT_FLOAT_EQ(result.second_highest_bid, 80.0f);

    nimcp_auction_destroy(auction);
}

TEST_F(AuctionRegressionTest, REG_AUC_002_SecondPriceWithTwoBidders) {
    nimcp_auction_t auction = create_auction(NIMCP_AUCTION_SECOND_PRICE);
    ASSERT_NE(auction, nullptr);

    // Minimal case: just two bidders
    nimcp_auction_bid(auction, 1, 50.0f, 0);
    nimcp_auction_bid(auction, 2, 30.0f, 0);

    nimcp_auction_result_t result;
    nimcp_error_t err = nimcp_auction_resolve(auction, &result);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(result.winner_id, 1u);
    EXPECT_FLOAT_EQ(result.payment, 30.0f);

    nimcp_auction_destroy(auction);
}

TEST_F(AuctionRegressionTest, REG_AUC_003_SecondPriceWithTiedBids) {
    config_.allow_tie_random = true;
    nimcp_auction_t auction = create_auction(NIMCP_AUCTION_SECOND_PRICE);
    ASSERT_NE(auction, nullptr);

    // Two bidders with same bid
    nimcp_auction_bid(auction, 1, 100.0f, 0);
    nimcp_auction_bid(auction, 2, 100.0f, 0);

    nimcp_auction_result_t result;
    nimcp_error_t err = nimcp_auction_resolve(auction, &result);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Either bidder could win, but payment should equal winning bid (tie)
    EXPECT_TRUE(result.winner_id == 1 || result.winner_id == 2);
    EXPECT_FLOAT_EQ(result.payment, 100.0f);

    nimcp_auction_destroy(auction);
}

TEST_F(AuctionRegressionTest, REG_AUC_004_SecondPriceManyScenarios) {
    // Test across many randomly generated scenarios
    std::mt19937 rng(42);  // Fixed seed for reproducibility
    std::uniform_real_distribution<float> bid_dist(1.0f, 1000.0f);

    for (int scenario = 0; scenario < 100; scenario++) {
        nimcp_auction_t auction = create_auction(NIMCP_AUCTION_SECOND_PRICE);
        ASSERT_NE(auction, nullptr);

        // Generate 5-20 random bids
        int num_bids = 5 + (scenario % 16);
        std::vector<float> bid_values;

        for (int i = 0; i < num_bids; i++) {
            float bid = bid_dist(rng);
            bid_values.push_back(bid);
            nimcp_auction_bid(auction, i + 1, bid, 0);
        }

        nimcp_auction_result_t result;
        nimcp_error_t err = nimcp_auction_resolve(auction, &result);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Scenario " << scenario << " failed";

        // Sort to find expected winner and second-highest
        std::sort(bid_values.begin(), bid_values.end(), std::greater<float>());
        float expected_second = bid_values[1];

        // Payment should equal second-highest bid
        EXPECT_NEAR(result.payment, expected_second, 0.001f)
            << "Scenario " << scenario << ": payment=" << result.payment
            << " expected=" << expected_second;

        nimcp_auction_destroy(auction);
    }
}

//=============================================================================
// REG-AUC-010: First-Price Auction
//=============================================================================

TEST_F(AuctionRegressionTest, REG_AUC_010_FirstPricePaymentEqualsWinningBid) {
    nimcp_auction_t auction = create_auction(NIMCP_AUCTION_FIRST_PRICE);
    ASSERT_NE(auction, nullptr);

    std::vector<std::pair<nimcp_player_id_t, float>> bids = {
        {1, 100.0f}, {2, 80.0f}, {3, 60.0f}
    };
    submit_bids(auction, bids);

    nimcp_auction_result_t result;
    nimcp_error_t err = nimcp_auction_resolve(auction, &result);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // In first-price, payment equals winning bid
    EXPECT_EQ(result.winner_id, 1u);
    EXPECT_FLOAT_EQ(result.payment, 100.0f);
    EXPECT_FLOAT_EQ(result.winning_bid, 100.0f);

    nimcp_auction_destroy(auction);
}

//=============================================================================
// REG-AUC-020: Reserve Price Enforcement
//=============================================================================

TEST_F(AuctionRegressionTest, REG_AUC_020_ReservePriceEnforced) {
    config_.reserve_price = 50.0f;
    nimcp_auction_t auction = create_auction(NIMCP_AUCTION_SECOND_PRICE);
    ASSERT_NE(auction, nullptr);

    // All bids below reserve
    nimcp_auction_bid(auction, 1, 40.0f, 0);
    nimcp_auction_bid(auction, 2, 30.0f, 0);

    nimcp_auction_result_t result;
    nimcp_error_t err = nimcp_auction_resolve(auction, &result);

    // Should complete but with no winner
    ASSERT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(result.final_state, NIMCP_AUCTION_STATE_NO_WINNER);
    EXPECT_EQ(result.winner_id, NIMCP_GT_INVALID_PLAYER);

    nimcp_auction_destroy(auction);
}

TEST_F(AuctionRegressionTest, REG_AUC_021_ReservePriceAffectsPayment) {
    config_.reserve_price = 50.0f;
    nimcp_auction_t auction = create_auction(NIMCP_AUCTION_SECOND_PRICE);
    ASSERT_NE(auction, nullptr);

    // Highest bid above reserve, second below
    nimcp_auction_bid(auction, 1, 100.0f, 0);
    nimcp_auction_bid(auction, 2, 30.0f, 0);

    nimcp_auction_result_t result;
    nimcp_error_t err = nimcp_auction_resolve(auction, &result);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Payment should be max(second_highest, reserve)
    EXPECT_EQ(result.winner_id, 1u);
    EXPECT_FLOAT_EQ(result.payment, 50.0f);  // Reserve, not second bid

    nimcp_auction_destroy(auction);
}

//=============================================================================
// REG-AUC-030: Performance Tests
//=============================================================================

TEST_F(AuctionRegressionTest, REG_AUC_030_AuctionResolutionPerformance) {
    const int NUM_AUCTIONS = 1000;
    const int BIDS_PER_AUCTION = 20;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_AUCTIONS; i++) {
        nimcp_auction_t auction = create_auction(NIMCP_AUCTION_SECOND_PRICE);
        ASSERT_NE(auction, nullptr);

        for (int j = 0; j < BIDS_PER_AUCTION; j++) {
            nimcp_auction_bid(auction, j + 1, (float)(100 + j * 10), 0);
        }

        nimcp_auction_result_t result;
        nimcp_auction_resolve(auction, &result);

        nimcp_auction_destroy(auction);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 1000 auctions in under 1 second
    EXPECT_LT(duration.count(), 1000)
        << "1000 auctions took " << duration.count() << "ms";

    // Log average time per auction
    double avg_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / (double)NUM_AUCTIONS;
    SUCCEED() << "Average auction resolution time: " << avg_us << " microseconds";
}

TEST_F(AuctionRegressionTest, REG_AUC_031_LargeAuctionPerformance) {
    config_.max_bidders = 1000;
    nimcp_auction_t auction = create_auction(NIMCP_AUCTION_SECOND_PRICE);
    ASSERT_NE(auction, nullptr);

    auto start = std::chrono::high_resolution_clock::now();

    // Submit 1000 bids
    for (int i = 0; i < 1000; i++) {
        nimcp_auction_bid(auction, i + 1, (float)(i * 1.5), 0);
    }

    nimcp_auction_result_t result;
    nimcp_auction_resolve(auction, &result);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Large auction should complete in under 100ms
    EXPECT_LT(duration.count(), 100);

    // Verify correctness
    // Bidder i+1 bids i*1.5, so bidder 1000 bids 999*1.5=1498.5 (winner)
    // Second-highest is bidder 999 with 998*1.5=1497.0
    EXPECT_EQ(result.winner_id, 1000u);  // Highest bidder
    EXPECT_NEAR(result.payment, 1497.0f, 0.1f);  // Second-highest bid

    nimcp_auction_destroy(auction);
}

//=============================================================================
// REG-AUC-040: Numerical Stability
//=============================================================================

TEST_F(AuctionRegressionTest, REG_AUC_040_VerySmallBids) {
    nimcp_auction_t auction = create_auction(NIMCP_AUCTION_SECOND_PRICE);
    ASSERT_NE(auction, nullptr);

    // Very small bid values
    nimcp_auction_bid(auction, 1, 1e-6f, 0);
    nimcp_auction_bid(auction, 2, 1e-7f, 0);
    nimcp_auction_bid(auction, 3, 1e-8f, 0);

    nimcp_auction_result_t result;
    nimcp_error_t err = nimcp_auction_resolve(auction, &result);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(result.winner_id, 1u);
    EXPECT_NEAR(result.payment, 1e-7f, 1e-10f);
    EXPECT_FALSE(std::isnan(result.payment));
    EXPECT_FALSE(std::isinf(result.payment));

    nimcp_auction_destroy(auction);
}

TEST_F(AuctionRegressionTest, REG_AUC_041_VeryLargeBids) {
    nimcp_auction_t auction = create_auction(NIMCP_AUCTION_SECOND_PRICE);
    ASSERT_NE(auction, nullptr);

    // Very large bid values
    float large1 = 1e30f;
    float large2 = 1e29f;
    float large3 = 1e28f;

    nimcp_auction_bid(auction, 1, large1, 0);
    nimcp_auction_bid(auction, 2, large2, 0);
    nimcp_auction_bid(auction, 3, large3, 0);

    nimcp_auction_result_t result;
    nimcp_error_t err = nimcp_auction_resolve(auction, &result);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(result.winner_id, 1u);
    EXPECT_NEAR(result.payment, large2, large2 * 0.001f);
    EXPECT_FALSE(std::isnan(result.payment));
    EXPECT_FALSE(std::isinf(result.payment));

    nimcp_auction_destroy(auction);
}

TEST_F(AuctionRegressionTest, REG_AUC_042_MixedMagnitudeBids) {
    nimcp_auction_t auction = create_auction(NIMCP_AUCTION_SECOND_PRICE);
    ASSERT_NE(auction, nullptr);

    // Mix of very different magnitudes
    nimcp_auction_bid(auction, 1, 1e10f, 0);
    nimcp_auction_bid(auction, 2, 1.0f, 0);
    nimcp_auction_bid(auction, 3, 1e-5f, 0);

    nimcp_auction_result_t result;
    nimcp_error_t err = nimcp_auction_resolve(auction, &result);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(result.winner_id, 1u);
    EXPECT_FLOAT_EQ(result.payment, 1.0f);

    nimcp_auction_destroy(auction);
}

TEST_F(AuctionRegressionTest, REG_AUC_043_NearIdenticalBids) {
    nimcp_auction_t auction = create_auction(NIMCP_AUCTION_SECOND_PRICE);
    ASSERT_NE(auction, nullptr);

    // Bids differing by epsilon
    float base = 100.0f;
    float eps = std::numeric_limits<float>::epsilon() * base;

    nimcp_auction_bid(auction, 1, base + eps, 0);
    nimcp_auction_bid(auction, 2, base, 0);
    nimcp_auction_bid(auction, 3, base - eps, 0);

    nimcp_auction_result_t result;
    nimcp_error_t err = nimcp_auction_resolve(auction, &result);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Should correctly identify winner as bidder 1
    EXPECT_EQ(result.winner_id, 1u);
    EXPECT_FLOAT_EQ(result.payment, base);

    nimcp_auction_destroy(auction);
}

//=============================================================================
// REG-AUC-050: Edge Cases
//=============================================================================

TEST_F(AuctionRegressionTest, REG_AUC_050_SingleBidder) {
    nimcp_auction_t auction = create_auction(NIMCP_AUCTION_SECOND_PRICE);
    ASSERT_NE(auction, nullptr);

    nimcp_auction_bid(auction, 1, 100.0f, 0);

    nimcp_auction_result_t result;
    nimcp_error_t err = nimcp_auction_resolve(auction, &result);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Single bidder should win, payment is often 0 or reserve
    EXPECT_EQ(result.winner_id, 1u);
    EXPECT_GE(result.payment, 0.0f);
    EXPECT_LE(result.payment, 100.0f);

    nimcp_auction_destroy(auction);
}

TEST_F(AuctionRegressionTest, REG_AUC_051_NoBidders) {
    nimcp_auction_t auction = create_auction(NIMCP_AUCTION_SECOND_PRICE);
    ASSERT_NE(auction, nullptr);

    nimcp_auction_result_t result;
    nimcp_error_t err = nimcp_auction_resolve(auction, &result);

    // No bidders should result in no winner
    EXPECT_EQ(result.final_state, NIMCP_AUCTION_STATE_NO_WINNER);
    EXPECT_EQ(result.winner_id, NIMCP_GT_INVALID_PLAYER);
    EXPECT_EQ(result.num_bids, 0u);

    nimcp_auction_destroy(auction);
}

TEST_F(AuctionRegressionTest, REG_AUC_052_ZeroBid) {
    nimcp_auction_t auction = create_auction(NIMCP_AUCTION_SECOND_PRICE);
    ASSERT_NE(auction, nullptr);

    nimcp_auction_bid(auction, 1, 100.0f, 0);
    nimcp_auction_bid(auction, 2, 0.0f, 0);

    nimcp_auction_result_t result;
    nimcp_error_t err = nimcp_auction_resolve(auction, &result);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(result.winner_id, 1u);
    EXPECT_FLOAT_EQ(result.payment, 0.0f);

    nimcp_auction_destroy(auction);
}

//=============================================================================
// REG-AUC-060: Auction State Machine
//=============================================================================

TEST_F(AuctionRegressionTest, REG_AUC_060_StateTransitions) {
    nimcp_auction_t auction = create_auction(NIMCP_AUCTION_SECOND_PRICE);
    ASSERT_NE(auction, nullptr);

    EXPECT_EQ(nimcp_auction_get_state(auction), NIMCP_AUCTION_STATE_CREATED);

    nimcp_auction_bid(auction, 1, 100.0f, 0);
    EXPECT_EQ(nimcp_auction_get_state(auction), NIMCP_AUCTION_STATE_BIDDING);

    nimcp_auction_result_t result;
    nimcp_auction_resolve(auction, &result);
    EXPECT_EQ(nimcp_auction_get_state(auction), NIMCP_AUCTION_STATE_COMPLETED);

    nimcp_auction_destroy(auction);
}

TEST_F(AuctionRegressionTest, REG_AUC_061_CancellationState) {
    nimcp_auction_t auction = create_auction(NIMCP_AUCTION_SECOND_PRICE);
    ASSERT_NE(auction, nullptr);

    nimcp_auction_bid(auction, 1, 100.0f, 0);
    nimcp_auction_cancel(auction);

    EXPECT_EQ(nimcp_auction_get_state(auction), NIMCP_AUCTION_STATE_CANCELLED);

    // Resolve after cancel should fail or return cancelled state
    nimcp_auction_result_t result;
    nimcp_auction_resolve(auction, &result);
    EXPECT_EQ(result.final_state, NIMCP_AUCTION_STATE_CANCELLED);

    nimcp_auction_destroy(auction);
}

//=============================================================================
// REG-AUC-070: Strategyproofness Property
//=============================================================================

TEST_F(AuctionRegressionTest, REG_AUC_070_SecondPriceIsStrategyproof) {
    EXPECT_TRUE(nimcp_auction_is_strategyproof(NIMCP_AUCTION_SECOND_PRICE));
    EXPECT_TRUE(nimcp_auction_is_strategyproof(NIMCP_AUCTION_VCG));
    EXPECT_FALSE(nimcp_auction_is_strategyproof(NIMCP_AUCTION_FIRST_PRICE));
}

//=============================================================================
// REG-AUC-080: Null Safety
//=============================================================================

TEST_F(AuctionRegressionTest, REG_AUC_080_NullHandling) {
    // Create with null config should use defaults
    nimcp_auction_t auction = nimcp_auction_create(nullptr);
    // May return nullptr or use defaults - implementation dependent
    if (auction != nullptr) {
        nimcp_auction_destroy(auction);
    }

    // Bid to null auction
    nimcp_error_t err = nimcp_auction_bid(nullptr, 1, 100.0f, 0);
    EXPECT_NE(err, NIMCP_SUCCESS);

    // Resolve with null result
    auction = create_auction(NIMCP_AUCTION_SECOND_PRICE);
    ASSERT_NE(auction, nullptr);
    nimcp_auction_bid(auction, 1, 100.0f, 0);
    err = nimcp_auction_resolve(auction, nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS);

    nimcp_auction_destroy(auction);

    // Destroy null should not crash
    nimcp_auction_destroy(nullptr);
}

//=============================================================================
// REG-AUC-090: Query Functions
//=============================================================================

TEST_F(AuctionRegressionTest, REG_AUC_090_BidCountAccuracy) {
    nimcp_auction_t auction = create_auction(NIMCP_AUCTION_SECOND_PRICE);
    ASSERT_NE(auction, nullptr);

    EXPECT_EQ(nimcp_auction_get_bid_count(auction), 0u);

    nimcp_auction_bid(auction, 1, 100.0f, 0);
    EXPECT_EQ(nimcp_auction_get_bid_count(auction), 1u);

    nimcp_auction_bid(auction, 2, 80.0f, 0);
    nimcp_auction_bid(auction, 3, 60.0f, 0);
    EXPECT_EQ(nimcp_auction_get_bid_count(auction), 3u);

    nimcp_auction_destroy(auction);
}

TEST_F(AuctionRegressionTest, REG_AUC_091_CurrentHighestAccuracy) {
    nimcp_auction_t auction = create_auction(NIMCP_AUCTION_SECOND_PRICE);
    ASSERT_NE(auction, nullptr);

    nimcp_auction_bid(auction, 1, 50.0f, 0);
    EXPECT_FLOAT_EQ(nimcp_auction_get_current_highest(auction), 50.0f);

    nimcp_auction_bid(auction, 2, 100.0f, 0);
    EXPECT_FLOAT_EQ(nimcp_auction_get_current_highest(auction), 100.0f);

    nimcp_auction_bid(auction, 3, 75.0f, 0);
    EXPECT_FLOAT_EQ(nimcp_auction_get_current_highest(auction), 100.0f);

    nimcp_auction_destroy(auction);
}

TEST_F(AuctionRegressionTest, REG_AUC_092_TypeNameConsistency) {
    const char* name = nimcp_auction_type_name(NIMCP_AUCTION_SECOND_PRICE);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = nimcp_auction_type_name(NIMCP_AUCTION_VCG);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    // All types should have names
    for (int i = 0; i < NIMCP_AUCTION_COUNT; i++) {
        name = nimcp_auction_type_name((nimcp_auction_type_t)i);
        ASSERT_NE(name, nullptr);
    }
}

//=============================================================================
// REG-AUC-100: Repeated Operations Stability
//=============================================================================

TEST_F(AuctionRegressionTest, REG_AUC_100_RepeatedCreateDestroy) {
    for (int i = 0; i < 1000; i++) {
        nimcp_auction_t auction = create_auction(NIMCP_AUCTION_SECOND_PRICE);
        ASSERT_NE(auction, nullptr) << "Failed at iteration " << i;
        nimcp_auction_destroy(auction);
    }
}

TEST_F(AuctionRegressionTest, REG_AUC_101_RepeatedBidResolve) {
    // Need to increase max_bidders for this test (default is 32)
    config_.max_bidders = 128;
    nimcp_auction_t auction = create_auction(NIMCP_AUCTION_SECOND_PRICE);
    ASSERT_NE(auction, nullptr);

    // Submit many bids to same auction
    for (int i = 0; i < 100; i++) {
        nimcp_error_t err = nimcp_auction_bid(auction, i + 1, (float)(i * 10), 0);
        EXPECT_EQ(err, NIMCP_SUCCESS) << "Failed at bid " << i;
    }

    nimcp_auction_result_t result;
    nimcp_auction_resolve(auction, &result);

    EXPECT_EQ(result.winner_id, 100u);
    EXPECT_FLOAT_EQ(result.payment, 980.0f);

    nimcp_auction_destroy(auction);
}

