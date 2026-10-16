//=============================================================================
// test_gt_auction_ext.cpp - Unit tests for Extended Auction Module
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/game_theory/nimcp_gt_auction_ext.h"
#include "cognitive/game_theory/nimcp_game_theory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class AuctionExtTest : public ::testing::Test {
protected:
    nimcp_combo_auction_t combo_auction = nullptr;
    nimcp_double_auction_t double_auction = nullptr;
    nimcp_multi_unit_auction_t multi_unit_auction = nullptr;

    void TearDown() override {
        if (combo_auction) {
            nimcp_combo_auction_destroy(combo_auction);
            combo_auction = nullptr;
        }
        if (double_auction) {
            nimcp_double_auction_destroy(double_auction);
            double_auction = nullptr;
        }
        if (multi_unit_auction) {
            nimcp_multi_unit_destroy(multi_unit_auction);
            multi_unit_auction = nullptr;
        }
    }
};

//=============================================================================
// Combinatorial Auction Tests
//=============================================================================

TEST_F(AuctionExtTest, ComboAuctionDefaultConfig) {
    nimcp_combo_auction_config_t config = nimcp_combo_auction_default_config(4);
    EXPECT_EQ(config.num_items, 4);
}

TEST_F(AuctionExtTest, ComboAuctionCreate) {
    nimcp_combo_auction_config_t config = nimcp_combo_auction_default_config(4);
    combo_auction = nimcp_combo_auction_create(&config);
    ASSERT_NE(combo_auction, nullptr);
}

TEST_F(AuctionExtTest, ComboAuctionSubmitBid) {
    nimcp_combo_auction_config_t config = nimcp_combo_auction_default_config(4);
    combo_auction = nimcp_combo_auction_create(&config);
    ASSERT_NE(combo_auction, nullptr);

    // Bid for items {0, 1} = 0b0011 = 3
    nimcp_error_t err = nimcp_combo_auction_submit_bundle_bid(combo_auction, 1, 0b0011, 100.0f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(nimcp_combo_auction_get_bid_count(combo_auction), 1);
}

TEST_F(AuctionExtTest, ComboAuctionMultipleBids) {
    nimcp_combo_auction_config_t config = nimcp_combo_auction_default_config(4);
    combo_auction = nimcp_combo_auction_create(&config);
    ASSERT_NE(combo_auction, nullptr);

    // Bidder 1 bids for {0, 1}
    nimcp_combo_auction_submit_bundle_bid(combo_auction, 1, 0b0011, 100.0f);
    // Bidder 2 bids for {2, 3}
    nimcp_combo_auction_submit_bundle_bid(combo_auction, 2, 0b1100, 80.0f);
    // Bidder 3 bids for {0, 1, 2, 3}
    nimcp_combo_auction_submit_bundle_bid(combo_auction, 3, 0b1111, 150.0f);

    EXPECT_EQ(nimcp_combo_auction_get_bid_count(combo_auction), 3);
}

TEST_F(AuctionExtTest, ComboAuctionSolveGreedy) {
    nimcp_combo_auction_config_t config = nimcp_combo_auction_default_config(4);
    combo_auction = nimcp_combo_auction_create(&config);
    ASSERT_NE(combo_auction, nullptr);

    // Bidder 1 bids for {0, 1} = 100
    nimcp_combo_auction_submit_bundle_bid(combo_auction, 1, 0b0011, 100.0f);
    // Bidder 2 bids for {2, 3} = 80
    nimcp_combo_auction_submit_bundle_bid(combo_auction, 2, 0b1100, 80.0f);
    // Bidder 3 bids for all {0,1,2,3} = 150
    nimcp_combo_auction_submit_bundle_bid(combo_auction, 3, 0b1111, 150.0f);

    nimcp_combo_result_t result;
    nimcp_error_t err = nimcp_combo_auction_solve_greedy(combo_auction, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Optimal: Bidder 1 + Bidder 2 = 180 > Bidder 3 alone = 150
    EXPECT_GT(result.total_value, 0.0f);
}

TEST_F(AuctionExtTest, ComboAuctionSolveOptimal) {
    nimcp_combo_auction_config_t config = nimcp_combo_auction_default_config(4);
    combo_auction = nimcp_combo_auction_create(&config);
    ASSERT_NE(combo_auction, nullptr);

    nimcp_combo_auction_submit_bundle_bid(combo_auction, 1, 0b0011, 100.0f);
    nimcp_combo_auction_submit_bundle_bid(combo_auction, 2, 0b1100, 80.0f);
    nimcp_combo_auction_submit_bundle_bid(combo_auction, 3, 0b1111, 150.0f);

    nimcp_combo_result_t result;
    nimcp_error_t err = nimcp_combo_auction_solve_optimal(combo_auction, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Optimal: Bidder 1 (100) + Bidder 2 (80) = 180
    EXPECT_FLOAT_EQ(result.total_value, 180.0f);
    EXPECT_EQ(result.num_winners, 2);
}

TEST_F(AuctionExtTest, ComboAuctionVcgPayments) {
    nimcp_combo_auction_config_t config = nimcp_combo_auction_default_config(4);
    config.compute_vcg_payments = true;
    combo_auction = nimcp_combo_auction_create(&config);
    ASSERT_NE(combo_auction, nullptr);

    nimcp_combo_auction_submit_bundle_bid(combo_auction, 1, 0b0011, 100.0f);
    nimcp_combo_auction_submit_bundle_bid(combo_auction, 2, 0b1100, 80.0f);
    nimcp_combo_auction_submit_bundle_bid(combo_auction, 3, 0b1111, 150.0f);

    nimcp_combo_result_t result;
    nimcp_combo_auction_solve_optimal(combo_auction, &result);

    nimcp_error_t err = nimcp_combo_auction_get_vcg_payments(combo_auction, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(AuctionExtTest, ComboAuctionGetState) {
    nimcp_combo_auction_config_t config = nimcp_combo_auction_default_config(4);
    combo_auction = nimcp_combo_auction_create(&config);
    ASSERT_NE(combo_auction, nullptr);

    nimcp_combo_state_t state = nimcp_combo_auction_get_state(combo_auction);
    EXPECT_EQ(state, NIMCP_COMBO_STATE_CREATED);
}

TEST_F(AuctionExtTest, ComboAuctionCancel) {
    nimcp_combo_auction_config_t config = nimcp_combo_auction_default_config(4);
    combo_auction = nimcp_combo_auction_create(&config);
    ASSERT_NE(combo_auction, nullptr);

    nimcp_error_t err = nimcp_combo_auction_cancel(combo_auction);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    nimcp_combo_state_t state = nimcp_combo_auction_get_state(combo_auction);
    EXPECT_EQ(state, NIMCP_COMBO_STATE_CANCELLED);
}

//=============================================================================
// Double Auction Tests
//=============================================================================

TEST_F(AuctionExtTest, DoubleAuctionDefaultConfig) {
    nimcp_double_auction_config_t config = nimcp_double_auction_default_config();
    EXPECT_GT(config.max_orders, 0);
}

TEST_F(AuctionExtTest, DoubleAuctionCreate) {
    nimcp_double_auction_config_t config = nimcp_double_auction_default_config();
    double_auction = nimcp_double_auction_create(&config);
    ASSERT_NE(double_auction, nullptr);
}

TEST_F(AuctionExtTest, DoubleAuctionSubmitBuy) {
    nimcp_double_auction_config_t config = nimcp_double_auction_default_config();
    double_auction = nimcp_double_auction_create(&config);
    ASSERT_NE(double_auction, nullptr);

    nimcp_error_t err = nimcp_double_auction_submit_buy(double_auction, 1, 50.0f, 10);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(AuctionExtTest, DoubleAuctionSubmitSell) {
    nimcp_double_auction_config_t config = nimcp_double_auction_default_config();
    double_auction = nimcp_double_auction_create(&config);
    ASSERT_NE(double_auction, nullptr);

    nimcp_error_t err = nimcp_double_auction_submit_sell(double_auction, 2, 40.0f, 10);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(AuctionExtTest, DoubleAuctionOrderCounts) {
    nimcp_double_auction_config_t config = nimcp_double_auction_default_config();
    double_auction = nimcp_double_auction_create(&config);
    ASSERT_NE(double_auction, nullptr);

    nimcp_double_auction_submit_buy(double_auction, 1, 50.0f, 10);
    nimcp_double_auction_submit_buy(double_auction, 2, 45.0f, 5);
    nimcp_double_auction_submit_sell(double_auction, 3, 40.0f, 8);

    uint32_t buy_orders = 0, sell_orders = 0;
    nimcp_error_t err = nimcp_double_auction_get_order_counts(double_auction, &buy_orders, &sell_orders);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(buy_orders, 2);
    EXPECT_EQ(sell_orders, 1);
}

TEST_F(AuctionExtTest, DoubleAuctionClear) {
    nimcp_double_auction_config_t config = nimcp_double_auction_default_config();
    double_auction = nimcp_double_auction_create(&config);
    ASSERT_NE(double_auction, nullptr);

    // Buyers willing to pay up to 50
    nimcp_double_auction_submit_buy(double_auction, 1, 50.0f, 10);
    // Sellers willing to sell at 40 or above
    nimcp_double_auction_submit_sell(double_auction, 2, 40.0f, 10);

    nimcp_clearing_result_t result;
    nimcp_error_t err = nimcp_double_auction_clear(double_auction, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Should have trades since buy price > sell price
    EXPECT_GT(result.total_quantity, 0);
    EXPECT_GE(result.clearing_price, 40.0f);
    EXPECT_LE(result.clearing_price, 50.0f);
}

TEST_F(AuctionExtTest, DoubleAuctionGetTrades) {
    nimcp_double_auction_config_t config = nimcp_double_auction_default_config();
    double_auction = nimcp_double_auction_create(&config);
    ASSERT_NE(double_auction, nullptr);

    nimcp_double_auction_submit_buy(double_auction, 1, 50.0f, 10);
    nimcp_double_auction_submit_sell(double_auction, 2, 40.0f, 10);

    nimcp_clearing_result_t clear_result;
    nimcp_double_auction_clear(double_auction, &clear_result);

    nimcp_trade_t trades[10];
    uint32_t num_trades = 0;
    nimcp_error_t err = nimcp_double_auction_get_trades(double_auction, trades, 10, &num_trades);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(num_trades, 0);
}

TEST_F(AuctionExtTest, DoubleAuctionGetSurplus) {
    nimcp_double_auction_config_t config = nimcp_double_auction_default_config();
    double_auction = nimcp_double_auction_create(&config);
    ASSERT_NE(double_auction, nullptr);

    nimcp_double_auction_submit_buy(double_auction, 1, 50.0f, 10);
    nimcp_double_auction_submit_sell(double_auction, 2, 40.0f, 10);

    nimcp_clearing_result_t clear_result;
    nimcp_double_auction_clear(double_auction, &clear_result);

    float buyer_surplus = 0, seller_surplus = 0;
    nimcp_error_t err = nimcp_double_auction_get_surplus(double_auction, &buyer_surplus, &seller_surplus);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(buyer_surplus, 0.0f);
    EXPECT_GE(seller_surplus, 0.0f);
}

TEST_F(AuctionExtTest, DoubleAuctionNoTrades) {
    nimcp_double_auction_config_t config = nimcp_double_auction_default_config();
    double_auction = nimcp_double_auction_create(&config);
    ASSERT_NE(double_auction, nullptr);

    // Buyer max < Seller min -> no trades
    nimcp_double_auction_submit_buy(double_auction, 1, 30.0f, 10);
    nimcp_double_auction_submit_sell(double_auction, 2, 50.0f, 10);

    nimcp_clearing_result_t result;
    nimcp_error_t err = nimcp_double_auction_clear(double_auction, &result);
    // May return success or NO_TRADES error
    EXPECT_EQ(result.total_quantity, 0);
}

TEST_F(AuctionExtTest, DoubleAuctionCancel) {
    nimcp_double_auction_config_t config = nimcp_double_auction_default_config();
    double_auction = nimcp_double_auction_create(&config);
    ASSERT_NE(double_auction, nullptr);

    nimcp_error_t err = nimcp_double_auction_cancel(double_auction);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    nimcp_double_state_t state = nimcp_double_auction_get_state(double_auction);
    EXPECT_EQ(state, NIMCP_DOUBLE_STATE_CANCELLED);
}

//=============================================================================
// Multi-Unit Auction Tests
//=============================================================================

TEST_F(AuctionExtTest, MultiUnitDefaultConfig) {
    nimcp_multi_unit_config_t config = nimcp_multi_unit_default_config(100);
    EXPECT_EQ(config.total_units, 100);
}

TEST_F(AuctionExtTest, MultiUnitCreate) {
    nimcp_multi_unit_config_t config = nimcp_multi_unit_default_config(100);
    multi_unit_auction = nimcp_multi_unit_create(&config);
    ASSERT_NE(multi_unit_auction, nullptr);
}

TEST_F(AuctionExtTest, MultiUnitSubmitBid) {
    nimcp_multi_unit_config_t config = nimcp_multi_unit_default_config(100);
    multi_unit_auction = nimcp_multi_unit_create(&config);
    ASSERT_NE(multi_unit_auction, nullptr);

    nimcp_error_t err = nimcp_multi_unit_submit_bid(multi_unit_auction, 1, 50.0f, 20);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(AuctionExtTest, MultiUnitMultipleBids) {
    nimcp_multi_unit_config_t config = nimcp_multi_unit_default_config(100);
    multi_unit_auction = nimcp_multi_unit_create(&config);
    ASSERT_NE(multi_unit_auction, nullptr);

    nimcp_multi_unit_submit_bid(multi_unit_auction, 1, 50.0f, 30);
    nimcp_multi_unit_submit_bid(multi_unit_auction, 2, 45.0f, 40);
    nimcp_multi_unit_submit_bid(multi_unit_auction, 3, 40.0f, 50);

    EXPECT_EQ(nimcp_multi_unit_get_bid_count(multi_unit_auction), 3);
}

TEST_F(AuctionExtTest, MultiUnitAllocate) {
    nimcp_multi_unit_config_t config = nimcp_multi_unit_default_config(100);
    config.type = NIMCP_MULTI_UNIT_DISCRIMINATORY;
    multi_unit_auction = nimcp_multi_unit_create(&config);
    ASSERT_NE(multi_unit_auction, nullptr);

    nimcp_multi_unit_submit_bid(multi_unit_auction, 1, 50.0f, 30);
    nimcp_multi_unit_submit_bid(multi_unit_auction, 2, 45.0f, 40);
    nimcp_multi_unit_submit_bid(multi_unit_auction, 3, 40.0f, 50);

    nimcp_multi_unit_result_t result;
    nimcp_error_t err = nimcp_multi_unit_allocate(multi_unit_auction, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(result.units_allocated, 100);
    EXPECT_GT(result.total_revenue, 0.0f);
}

TEST_F(AuctionExtTest, MultiUnitUniformPricing) {
    nimcp_multi_unit_config_t config = nimcp_multi_unit_default_config(50);
    config.type = NIMCP_MULTI_UNIT_UNIFORM;
    multi_unit_auction = nimcp_multi_unit_create(&config);
    ASSERT_NE(multi_unit_auction, nullptr);

    nimcp_multi_unit_submit_bid(multi_unit_auction, 1, 100.0f, 30);
    nimcp_multi_unit_submit_bid(multi_unit_auction, 2, 80.0f, 30);

    nimcp_multi_unit_result_t result;
    nimcp_error_t err = nimcp_multi_unit_allocate(multi_unit_auction, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // All winners pay the same price
    EXPECT_GT(result.clearing_price, 0.0f);
}

TEST_F(AuctionExtTest, MultiUnitGetState) {
    nimcp_multi_unit_config_t config = nimcp_multi_unit_default_config(100);
    multi_unit_auction = nimcp_multi_unit_create(&config);
    ASSERT_NE(multi_unit_auction, nullptr);

    nimcp_multi_unit_state_t state = nimcp_multi_unit_get_state(multi_unit_auction);
    EXPECT_EQ(state, NIMCP_MULTI_STATE_CREATED);
}

TEST_F(AuctionExtTest, MultiUnitCancel) {
    nimcp_multi_unit_config_t config = nimcp_multi_unit_default_config(100);
    multi_unit_auction = nimcp_multi_unit_create(&config);
    ASSERT_NE(multi_unit_auction, nullptr);

    nimcp_error_t err = nimcp_multi_unit_cancel(multi_unit_auction);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    nimcp_multi_unit_state_t state = nimcp_multi_unit_get_state(multi_unit_auction);
    EXPECT_EQ(state, NIMCP_MULTI_STATE_CANCELLED);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(AuctionExtTest, ComboAuctionSingleItem) {
    nimcp_combo_auction_config_t config = nimcp_combo_auction_default_config(1);
    combo_auction = nimcp_combo_auction_create(&config);
    ASSERT_NE(combo_auction, nullptr);

    nimcp_combo_auction_submit_bundle_bid(combo_auction, 1, 0b0001, 50.0f);
    nimcp_combo_auction_submit_bundle_bid(combo_auction, 2, 0b0001, 60.0f);

    nimcp_combo_result_t result;
    nimcp_combo_auction_solve_optimal(combo_auction, &result);

    EXPECT_EQ(result.num_winners, 1);
    EXPECT_FLOAT_EQ(result.total_value, 60.0f);
}

TEST_F(AuctionExtTest, DoubleAuctionSingleTrade) {
    nimcp_double_auction_config_t config = nimcp_double_auction_default_config();
    double_auction = nimcp_double_auction_create(&config);
    ASSERT_NE(double_auction, nullptr);

    nimcp_double_auction_submit_buy(double_auction, 1, 100.0f, 1);
    nimcp_double_auction_submit_sell(double_auction, 2, 50.0f, 1);

    nimcp_clearing_result_t result;
    nimcp_double_auction_clear(double_auction, &result);

    EXPECT_EQ(result.num_trades, 1);
    EXPECT_EQ(result.total_quantity, 1);
}

TEST_F(AuctionExtTest, MultiUnitSingleUnit) {
    nimcp_multi_unit_config_t config = nimcp_multi_unit_default_config(1);
    multi_unit_auction = nimcp_multi_unit_create(&config);
    ASSERT_NE(multi_unit_auction, nullptr);

    nimcp_multi_unit_submit_bid(multi_unit_auction, 1, 100.0f, 1);
    nimcp_multi_unit_submit_bid(multi_unit_auction, 2, 80.0f, 1);

    nimcp_multi_unit_result_t result;
    nimcp_multi_unit_allocate(multi_unit_auction, &result);

    EXPECT_EQ(result.units_allocated, 1);
    EXPECT_EQ(result.num_winners, 1);
}
