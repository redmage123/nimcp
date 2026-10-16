//=============================================================================
// test_auction.cpp - Unit tests for Auction Module
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/game_theory/nimcp_auction.h"
#include "cognitive/game_theory/nimcp_game_theory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class AuctionTest : public ::testing::Test {
protected:
    nimcp_auction_t auction = nullptr;
    nimcp_auction_config_t config;

    void SetUp() override {
        config = nimcp_auction_default_config();
    }

    void TearDown() override {
        if (auction) {
            nimcp_auction_destroy(auction);
            auction = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(AuctionTest, DefaultConfigValues) {
    EXPECT_EQ(config.type, NIMCP_AUCTION_SECOND_PRICE);
    EXPECT_EQ(config.num_items, 1);
    EXPECT_GE(config.reserve_price, 0.0f);
    EXPECT_GT(config.max_bidders, 0);
}

TEST_F(AuctionTest, CreateDestroy) {
    auction = nimcp_auction_create(&config);
    ASSERT_NE(auction, nullptr);

    EXPECT_EQ(nimcp_auction_get_state(auction), NIMCP_AUCTION_STATE_CREATED);
    EXPECT_EQ(nimcp_auction_get_bid_count(auction), 0);
}

TEST_F(AuctionTest, CreateWithNullConfig) {
    auction = nimcp_auction_create(nullptr);
    // Should use defaults
    ASSERT_NE(auction, nullptr);
}

//=============================================================================
// Second-Price Auction Tests
//=============================================================================

TEST_F(AuctionTest, SecondPriceSingleBidder) {
    config.type = NIMCP_AUCTION_SECOND_PRICE;
    config.reserve_price = 0.0f;
    auction = nimcp_auction_create(&config);
    ASSERT_NE(auction, nullptr);

    // Single bid
    nimcp_error_t err = nimcp_auction_bid(auction, 1, 10.0f, 0);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_auction_get_bid_count(auction), 1);

    // Resolve
    nimcp_auction_result_t result;
    err = nimcp_auction_resolve(auction, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Winner should be bidder 1
    EXPECT_EQ(result.winner_id, 1);
    EXPECT_FLOAT_EQ(result.winning_bid, 10.0f);
    // Payment should be reserve (or 0) for single bidder
    EXPECT_LE(result.payment, result.winning_bid);
}

TEST_F(AuctionTest, SecondPriceTwoBidders) {
    config.type = NIMCP_AUCTION_SECOND_PRICE;
    config.reserve_price = 0.0f;
    auction = nimcp_auction_create(&config);
    ASSERT_NE(auction, nullptr);

    // Two bids
    EXPECT_EQ(nimcp_auction_bid(auction, 1, 10.0f, 0), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_auction_bid(auction, 2, 15.0f, 0), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_auction_get_bid_count(auction), 2);

    // Resolve
    nimcp_auction_result_t result;
    nimcp_error_t err = nimcp_auction_resolve(auction, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Winner should be bidder 2 (highest bid)
    EXPECT_EQ(result.winner_id, 2);
    EXPECT_FLOAT_EQ(result.winning_bid, 15.0f);
    // Payment should be second-highest bid (10.0)
    EXPECT_FLOAT_EQ(result.payment, 10.0f);
    EXPECT_FLOAT_EQ(result.second_highest_bid, 10.0f);
}

TEST_F(AuctionTest, SecondPriceStrategyproofProperty) {
    // Verify that second-price auction is strategyproof
    EXPECT_TRUE(nimcp_auction_is_strategyproof(NIMCP_AUCTION_SECOND_PRICE));
    EXPECT_FALSE(nimcp_auction_is_strategyproof(NIMCP_AUCTION_FIRST_PRICE));
    EXPECT_TRUE(nimcp_auction_is_strategyproof(NIMCP_AUCTION_VCG));
}

TEST_F(AuctionTest, SecondPriceMultipleBidders) {
    config.type = NIMCP_AUCTION_SECOND_PRICE;
    auction = nimcp_auction_create(&config);
    ASSERT_NE(auction, nullptr);

    // Multiple bids
    EXPECT_EQ(nimcp_auction_bid(auction, 1, 5.0f, 0), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_auction_bid(auction, 2, 12.0f, 0), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_auction_bid(auction, 3, 8.0f, 0), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_auction_bid(auction, 4, 20.0f, 0), NIMCP_SUCCESS);

    nimcp_auction_result_t result;
    EXPECT_EQ(nimcp_auction_resolve(auction, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.winner_id, 4);
    EXPECT_FLOAT_EQ(result.winning_bid, 20.0f);
    EXPECT_FLOAT_EQ(result.payment, 12.0f);  // Second highest
}

//=============================================================================
// First-Price Auction Tests
//=============================================================================

TEST_F(AuctionTest, FirstPriceTwoBidders) {
    config.type = NIMCP_AUCTION_FIRST_PRICE;
    auction = nimcp_auction_create(&config);
    ASSERT_NE(auction, nullptr);

    EXPECT_EQ(nimcp_auction_bid(auction, 1, 10.0f, 0), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_auction_bid(auction, 2, 15.0f, 0), NIMCP_SUCCESS);

    nimcp_auction_result_t result;
    EXPECT_EQ(nimcp_auction_resolve(auction, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.winner_id, 2);
    EXPECT_FLOAT_EQ(result.winning_bid, 15.0f);
    // In first-price, payment equals winning bid
    EXPECT_FLOAT_EQ(result.payment, 15.0f);
}

//=============================================================================
// Reserve Price Tests
//=============================================================================

TEST_F(AuctionTest, ReservePriceNotMet) {
    config.type = NIMCP_AUCTION_SECOND_PRICE;
    config.reserve_price = 20.0f;
    auction = nimcp_auction_create(&config);
    ASSERT_NE(auction, nullptr);

    // Bids below reserve
    EXPECT_EQ(nimcp_auction_bid(auction, 1, 10.0f, 0), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_auction_bid(auction, 2, 15.0f, 0), NIMCP_SUCCESS);

    nimcp_auction_result_t result;
    nimcp_error_t err = nimcp_auction_resolve(auction, &result);

    // Should indicate no winner or reserve not met
    EXPECT_TRUE(result.final_state == NIMCP_AUCTION_STATE_NO_WINNER ||
                result.winner_id == NIMCP_GT_INVALID_PLAYER);
}

TEST_F(AuctionTest, ReservePriceMet) {
    config.type = NIMCP_AUCTION_SECOND_PRICE;
    config.reserve_price = 10.0f;
    auction = nimcp_auction_create(&config);
    ASSERT_NE(auction, nullptr);

    EXPECT_EQ(nimcp_auction_bid(auction, 1, 15.0f, 0), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_auction_bid(auction, 2, 25.0f, 0), NIMCP_SUCCESS);

    nimcp_auction_result_t result;
    EXPECT_EQ(nimcp_auction_resolve(auction, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.winner_id, 2);
    EXPECT_FLOAT_EQ(result.winning_bid, 25.0f);
    // Payment should be max of second-highest and reserve
    EXPECT_GE(result.payment, 15.0f);  // At least second highest
}

//=============================================================================
// VCG Auction Tests
//=============================================================================

TEST_F(AuctionTest, VCGBasicAuction) {
    config.type = NIMCP_AUCTION_VCG;
    auction = nimcp_auction_create(&config);
    ASSERT_NE(auction, nullptr);

    // Submit VCG bids with valuations
    EXPECT_EQ(nimcp_auction_bid_vcg(auction, 1, 10.0f, 10.0f, 0), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_auction_bid_vcg(auction, 2, 15.0f, 15.0f, 0), NIMCP_SUCCESS);

    nimcp_auction_result_t result;
    EXPECT_EQ(nimcp_auction_resolve(auction, &result), NIMCP_SUCCESS);

    // Winner should be highest bidder
    EXPECT_EQ(result.winner_id, 2);
    // VCG payment is externality imposed on others
    EXPECT_FLOAT_EQ(result.payment, 10.0f);  // = value lost by bidder 1
}

//=============================================================================
// Query Function Tests
//=============================================================================

TEST_F(AuctionTest, GetCurrentHighest) {
    config.type = NIMCP_AUCTION_SECOND_PRICE;
    auction = nimcp_auction_create(&config);
    ASSERT_NE(auction, nullptr);

    EXPECT_EQ(nimcp_auction_get_current_highest(auction), 0.0f);

    nimcp_auction_bid(auction, 1, 10.0f, 0);
    EXPECT_FLOAT_EQ(nimcp_auction_get_current_highest(auction), 10.0f);

    nimcp_auction_bid(auction, 2, 5.0f, 0);
    EXPECT_FLOAT_EQ(nimcp_auction_get_current_highest(auction), 10.0f);

    nimcp_auction_bid(auction, 3, 15.0f, 0);
    EXPECT_FLOAT_EQ(nimcp_auction_get_current_highest(auction), 15.0f);
}

TEST_F(AuctionTest, AuctionTypeNames) {
    EXPECT_STREQ(nimcp_auction_type_name(NIMCP_AUCTION_FIRST_PRICE), "First-Price");
    EXPECT_STREQ(nimcp_auction_type_name(NIMCP_AUCTION_SECOND_PRICE), "Second-Price (Vickrey)");
    EXPECT_STREQ(nimcp_auction_type_name(NIMCP_AUCTION_VCG), "VCG");
    EXPECT_STREQ(nimcp_auction_type_name(NIMCP_AUCTION_ASCENDING), "Ascending (English)");
    EXPECT_STREQ(nimcp_auction_type_name(NIMCP_AUCTION_DESCENDING), "Descending (Dutch)");
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(AuctionTest, BidOnNullAuction) {
    nimcp_error_t err = nimcp_auction_bid(nullptr, 1, 10.0f, 0);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(AuctionTest, ResolveNullAuction) {
    nimcp_auction_result_t result;
    nimcp_error_t err = nimcp_auction_resolve(nullptr, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(AuctionTest, ResolveNullResult) {
    auction = nimcp_auction_create(&config);
    ASSERT_NE(auction, nullptr);

    nimcp_error_t err = nimcp_auction_resolve(auction, nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(AuctionTest, NegativeBid) {
    auction = nimcp_auction_create(&config);
    ASSERT_NE(auction, nullptr);

    nimcp_error_t err = nimcp_auction_bid(auction, 1, -5.0f, 0);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

//=============================================================================
// Cancellation Tests
//=============================================================================

TEST_F(AuctionTest, CancelAuction) {
    auction = nimcp_auction_create(&config);
    ASSERT_NE(auction, nullptr);

    nimcp_auction_bid(auction, 1, 10.0f, 0);

    nimcp_error_t err = nimcp_auction_cancel(auction);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_auction_get_state(auction), NIMCP_AUCTION_STATE_CANCELLED);
}
