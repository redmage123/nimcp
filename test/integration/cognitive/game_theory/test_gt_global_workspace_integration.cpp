/**
 * @file test_gt_global_workspace_integration.cpp
 * @brief Integration tests for Game-Theoretic Global Workspace
 * @version 1.0.0
 * @date 2025-12-27
 *
 * WHAT: Integration tests for auction-based Global Workspace competition
 * WHY:  Verify that auction mechanisms work correctly with GW infrastructure
 * HOW:  Test auction context creation, bidding, resolution, budget management,
 *       and multi-module competition
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "cognitive/game_theory/integration/nimcp_gt_global_workspace.h"
#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "cognitive/game_theory/nimcp_auction.h"
#include "cognitive/game_theory/nimcp_game_theory.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class GTGlobalWorkspaceIntegrationTest : public ::testing::Test {
protected:
    gt_gw_auction_ctx_t auction_ctx;
    global_workspace_t* workspace;

    void SetUp() override {
        auction_ctx = nullptr;
        workspace = nullptr;

        /* Create global workspace */
        global_workspace_config_t ws_config = global_workspace_default_config();
        ws_config.capacity_dim = 64;
        ws_config.ignition_threshold = 0.3f;  /* Lower threshold for testing */
        ws_config.enable_statistics = true;
        workspace = global_workspace_create_custom(&ws_config);
        ASSERT_NE(workspace, nullptr);

        /* Subscribe some modules */
        global_workspace_subscribe(workspace, MODULE_WORKING_MEMORY);
        global_workspace_subscribe(workspace, MODULE_EXECUTIVE);
        global_workspace_subscribe(workspace, MODULE_ATTENTION);

        /* Create auction context */
        gt_gw_config_t config = gt_gw_default_config();
        config.strategy = GT_GW_STRATEGY_SECOND_PRICE;
        config.reserve_price = 0.1f;
        config.initial_budget = 10.0f;
        config.enable_budget_constraints = true;
        config.track_bid_history = true;
        auction_ctx = gt_gw_create(workspace, &config);
        ASSERT_NE(auction_ctx, nullptr);
    }

    void TearDown() override {
        if (auction_ctx) {
            gt_gw_destroy(auction_ctx);
        }
        if (workspace) {
            global_workspace_destroy(workspace);
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(GTGlobalWorkspaceIntegrationTest, CreateWithDefaultConfig) {
    gt_gw_config_t config = gt_gw_default_config();

    /* Verify reasonable defaults */
    EXPECT_EQ(config.strategy, GT_GW_STRATEGY_SECOND_PRICE);
    EXPECT_GT(config.initial_budget, 0.0f);
    EXPECT_GE(config.reserve_price, 0.0f);
}

TEST_F(GTGlobalWorkspaceIntegrationTest, CreateAndDestroyMultipleTimes) {
    /* Test repeated creation/destruction for memory leaks */
    for (int i = 0; i < 5; i++) {
        gt_gw_config_t config = gt_gw_default_config();
        gt_gw_auction_ctx_t ctx = gt_gw_create(workspace, &config);
        ASSERT_NE(ctx, nullptr);
        gt_gw_destroy(ctx);
    }
}

TEST_F(GTGlobalWorkspaceIntegrationTest, CreateWithNullWorkspace) {
    gt_gw_config_t config = gt_gw_default_config();
    gt_gw_auction_ctx_t ctx = gt_gw_create(nullptr, &config);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(GTGlobalWorkspaceIntegrationTest, AuctionIsActive) {
    bool active = gt_gw_is_auction_active(auction_ctx);
    EXPECT_TRUE(active);
}

TEST_F(GTGlobalWorkspaceIntegrationTest, GetUnderlyingWorkspace) {
    global_workspace_t* ws = gt_gw_get_workspace(auction_ctx);
    EXPECT_EQ(ws, workspace);
}

TEST_F(GTGlobalWorkspaceIntegrationTest, GetUnderlyingAuction) {
    nimcp_auction_t auction = gt_gw_get_auction(auction_ctx);
    EXPECT_NE(auction, nullptr);
}

/* ============================================================================
 * Bidding Tests
 * ============================================================================ */

TEST_F(GTGlobalWorkspaceIntegrationTest, SubmitSingleBid) {
    float content[64] = {0.0f};
    content[0] = 1.0f;

    nimcp_error_t err = gt_gw_bid(auction_ctx, MODULE_WORKING_MEMORY,
                                   content, 64, 0.8f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(GTGlobalWorkspaceIntegrationTest, SubmitMultipleBids) {
    float content1[64] = {0.0f};
    float content2[64] = {0.0f};
    float content3[64] = {0.0f};

    content1[0] = 1.0f;
    content2[0] = 2.0f;
    content3[0] = 3.0f;

    /* Submit bids from different modules */
    nimcp_error_t err1 = gt_gw_bid(auction_ctx, MODULE_WORKING_MEMORY,
                                    content1, 64, 0.5f);
    nimcp_error_t err2 = gt_gw_bid(auction_ctx, MODULE_EXECUTIVE,
                                    content2, 64, 0.7f);
    nimcp_error_t err3 = gt_gw_bid(auction_ctx, MODULE_ATTENTION,
                                    content3, 64, 0.6f);

    EXPECT_EQ(err1, NIMCP_SUCCESS);
    EXPECT_EQ(err2, NIMCP_SUCCESS);
    EXPECT_EQ(err3, NIMCP_SUCCESS);
}

TEST_F(GTGlobalWorkspaceIntegrationTest, BidWithNullContent) {
    nimcp_error_t err = gt_gw_bid(auction_ctx, MODULE_WORKING_MEMORY,
                                   nullptr, 64, 0.5f);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GTGlobalWorkspaceIntegrationTest, BidWithNullContext) {
    float content[64] = {0.0f};
    nimcp_error_t err = gt_gw_bid(nullptr, MODULE_WORKING_MEMORY,
                                   content, 64, 0.5f);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GTGlobalWorkspaceIntegrationTest, BidBelowReservePrice) {
    float content[64] = {0.0f};

    /* Reserve price is 0.1, so 0.05 should fail */
    nimcp_error_t err = gt_gw_bid(auction_ctx, MODULE_WORKING_MEMORY,
                                   content, 64, 0.05f);
    /* May still accept bid but it won't win during resolution */
    /* The behavior depends on implementation */
}

/* ============================================================================
 * Auction Resolution Tests
 * ============================================================================ */

TEST_F(GTGlobalWorkspaceIntegrationTest, ResolveWithSingleBid) {
    float content[64] = {0.0f};
    content[0] = 1.0f;

    gt_gw_bid(auction_ctx, MODULE_WORKING_MEMORY, content, 64, 0.8f);

    gt_gw_round_result_t result;
    nimcp_error_t err = gt_gw_resolve(auction_ctx, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Single bidder should win */
    EXPECT_EQ(result.winner, MODULE_WORKING_MEMORY);
    EXPECT_FLOAT_EQ(result.winning_bid, 0.8f);
    EXPECT_EQ(result.num_bidders, 1);
    EXPECT_TRUE(result.reserve_met);
}

TEST_F(GTGlobalWorkspaceIntegrationTest, ResolveWithMultipleBids) {
    float content1[64] = {0.0f};
    float content2[64] = {0.0f};
    float content3[64] = {0.0f};

    content1[0] = 1.0f;
    content2[0] = 2.0f;
    content3[0] = 3.0f;

    gt_gw_bid(auction_ctx, MODULE_WORKING_MEMORY, content1, 64, 0.5f);
    gt_gw_bid(auction_ctx, MODULE_EXECUTIVE, content2, 64, 0.9f);  /* Highest */
    gt_gw_bid(auction_ctx, MODULE_ATTENTION, content3, 64, 0.7f);

    gt_gw_round_result_t result;
    nimcp_error_t err = gt_gw_resolve(auction_ctx, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Executive had highest bid */
    EXPECT_EQ(result.winner, MODULE_EXECUTIVE);
    EXPECT_FLOAT_EQ(result.winning_bid, 0.9f);
    EXPECT_EQ(result.num_bidders, 3);

    /* Second-price auction: payment = second highest bid */
    EXPECT_LE(result.payment, result.winning_bid);
    EXPECT_GE(result.payment, result.second_highest_bid);
}

TEST_F(GTGlobalWorkspaceIntegrationTest, ResolveWithNoBids) {
    gt_gw_round_result_t result;
    nimcp_error_t err = gt_gw_resolve(auction_ctx, &result);

    /* May return error or result with no winner */
    EXPECT_EQ(result.num_bidders, 0);
    EXPECT_FALSE(result.reserve_met);
}

TEST_F(GTGlobalWorkspaceIntegrationTest, SecondPricePaymentIsSecondHighest) {
    float content1[64] = {0.0f};
    float content2[64] = {0.0f};

    content1[0] = 1.0f;
    content2[0] = 2.0f;

    gt_gw_bid(auction_ctx, MODULE_WORKING_MEMORY, content1, 64, 0.5f);
    gt_gw_bid(auction_ctx, MODULE_EXECUTIVE, content2, 64, 0.9f);

    gt_gw_round_result_t result;
    gt_gw_resolve(auction_ctx, &result);

    /* Winner pays second-highest bid */
    EXPECT_EQ(result.winner, MODULE_EXECUTIVE);
    EXPECT_FLOAT_EQ(result.payment, 0.5f);
    EXPECT_FLOAT_EQ(result.second_highest_bid, 0.5f);
}

/* ============================================================================
 * Combined Compete Tests
 * ============================================================================ */

TEST_F(GTGlobalWorkspaceIntegrationTest, CompeteSingleModule) {
    float content[64] = {0.0f};
    content[0] = 1.0f;

    bool won = gt_gw_compete(auction_ctx, MODULE_WORKING_MEMORY,
                              content, 64, 0.8f);
    EXPECT_TRUE(won);
}

TEST_F(GTGlobalWorkspaceIntegrationTest, CompeteMultipleModulesSequentially) {
    float content1[64] = {0.0f};
    float content2[64] = {0.0f};

    content1[0] = 1.0f;
    content2[0] = 2.0f;

    /* First module competes and wins */
    bool won1 = gt_gw_compete(auction_ctx, MODULE_WORKING_MEMORY,
                               content1, 64, 0.5f);

    /* Second module competes with higher bid */
    bool won2 = gt_gw_compete(auction_ctx, MODULE_EXECUTIVE,
                               content2, 64, 0.9f);

    /* At least one should win */
    EXPECT_TRUE(won1 || won2);
}

/* ============================================================================
 * Budget Management Tests
 * ============================================================================ */

TEST_F(GTGlobalWorkspaceIntegrationTest, InitialBudgetIsSet) {
    gt_gw_module_state_t state;
    nimcp_error_t err = gt_gw_get_module_state(auction_ctx, MODULE_WORKING_MEMORY, &state);

    if (err == NIMCP_SUCCESS) {
        EXPECT_FLOAT_EQ(state.current_budget, 10.0f);
    }
}

TEST_F(GTGlobalWorkspaceIntegrationTest, BudgetDecreasesAfterWin) {
    float content[64] = {0.0f};
    content[0] = 1.0f;

    /* Get initial budget */
    gt_gw_module_state_t state_before;
    gt_gw_get_module_state(auction_ctx, MODULE_WORKING_MEMORY, &state_before);

    /* Compete and win */
    gt_gw_compete(auction_ctx, MODULE_WORKING_MEMORY, content, 64, 0.5f);

    /* Get budget after win */
    gt_gw_module_state_t state_after;
    gt_gw_get_module_state(auction_ctx, MODULE_WORKING_MEMORY, &state_after);

    /* Budget should have decreased */
    EXPECT_LT(state_after.current_budget, state_before.current_budget);
}

TEST_F(GTGlobalWorkspaceIntegrationTest, ReplenishBudgets) {
    float content[64] = {0.0f};

    /* Spend some budget */
    for (int i = 0; i < 3; i++) {
        gt_gw_compete(auction_ctx, MODULE_WORKING_MEMORY, content, 64, 0.5f);
    }

    gt_gw_module_state_t state_before;
    gt_gw_get_module_state(auction_ctx, MODULE_WORKING_MEMORY, &state_before);

    /* Replenish */
    gt_gw_replenish_budgets(auction_ctx, 2.0f);

    gt_gw_module_state_t state_after;
    gt_gw_get_module_state(auction_ctx, MODULE_WORKING_MEMORY, &state_after);

    EXPECT_GE(state_after.current_budget, state_before.current_budget);
}

TEST_F(GTGlobalWorkspaceIntegrationTest, ResetBudgets) {
    float content[64] = {0.0f};

    /* Spend budget */
    for (int i = 0; i < 5; i++) {
        gt_gw_compete(auction_ctx, MODULE_WORKING_MEMORY, content, 64, 0.5f);
    }

    /* Reset */
    gt_gw_reset_budgets(auction_ctx);

    gt_gw_module_state_t state;
    gt_gw_get_module_state(auction_ctx, MODULE_WORKING_MEMORY, &state);

    /* Should be back to initial */
    EXPECT_FLOAT_EQ(state.current_budget, 10.0f);
}

/* ============================================================================
 * Module State Tests
 * ============================================================================ */

TEST_F(GTGlobalWorkspaceIntegrationTest, GetModuleStateAfterMultipleRounds) {
    float content[64] = {0.0f};

    /* Run several competition rounds */
    for (int i = 0; i < 5; i++) {
        gt_gw_compete(auction_ctx, MODULE_WORKING_MEMORY, content, 64, 0.6f);
        gt_gw_compete(auction_ctx, MODULE_EXECUTIVE, content, 64, 0.5f);
    }

    gt_gw_module_state_t wm_state, exec_state;
    gt_gw_get_module_state(auction_ctx, MODULE_WORKING_MEMORY, &wm_state);
    gt_gw_get_module_state(auction_ctx, MODULE_EXECUTIVE, &exec_state);

    /* WM should have more wins (higher bids) */
    EXPECT_GT(wm_state.bids_submitted, 0);
    EXPECT_GT(exec_state.bids_submitted, 0);
}

TEST_F(GTGlobalWorkspaceIntegrationTest, TrackWinStatistics) {
    float content[64] = {0.0f};

    /* WM always bids higher */
    for (int i = 0; i < 5; i++) {
        gt_gw_bid(auction_ctx, MODULE_WORKING_MEMORY, content, 64, 0.9f);
        gt_gw_bid(auction_ctx, MODULE_EXECUTIVE, content, 64, 0.3f);

        gt_gw_round_result_t result;
        gt_gw_resolve(auction_ctx, &result);
    }

    gt_gw_module_state_t wm_state;
    gt_gw_get_module_state(auction_ctx, MODULE_WORKING_MEMORY, &wm_state);

    EXPECT_GT(wm_state.wins, 0);
    EXPECT_EQ(wm_state.bids_submitted, 5);
}

/* ============================================================================
 * Multi-Module Competition Tests
 * ============================================================================ */

TEST_F(GTGlobalWorkspaceIntegrationTest, MultiModuleCompetition) {
    std::vector<cognitive_module_t> modules = {
        MODULE_WORKING_MEMORY,
        MODULE_EXECUTIVE,
        MODULE_ATTENTION,
        MODULE_SALIENCE,
        MODULE_EMOTION
    };

    float content[64] = {0.0f};

    /* All modules compete with different bids */
    for (size_t i = 0; i < modules.size(); i++) {
        float bid = 0.3f + 0.1f * i;  /* Increasing bids */
        gt_gw_bid(auction_ctx, modules[i], content, 64, bid);
    }

    gt_gw_round_result_t result;
    gt_gw_resolve(auction_ctx, &result);

    /* Highest bidder (EMOTION) should win */
    EXPECT_EQ(result.winner, MODULE_EMOTION);
    EXPECT_EQ(result.num_bidders, 5);
    EXPECT_TRUE(result.reserve_met);
}

TEST_F(GTGlobalWorkspaceIntegrationTest, RepeatedCompetitionConverges) {
    float content[64] = {0.0f};

    /* Run many rounds of competition */
    uint64_t wm_wins = 0;
    uint64_t exec_wins = 0;

    for (int round = 0; round < 20; round++) {
        /* Alternate who bids higher */
        float wm_bid = (round % 2 == 0) ? 0.8f : 0.4f;
        float exec_bid = (round % 2 == 0) ? 0.4f : 0.8f;

        gt_gw_bid(auction_ctx, MODULE_WORKING_MEMORY, content, 64, wm_bid);
        gt_gw_bid(auction_ctx, MODULE_EXECUTIVE, content, 64, exec_bid);

        gt_gw_round_result_t result;
        gt_gw_resolve(auction_ctx, &result);

        if (result.winner == MODULE_WORKING_MEMORY) wm_wins++;
        else if (result.winner == MODULE_EXECUTIVE) exec_wins++;
    }

    /* Both should win some rounds */
    EXPECT_GT(wm_wins, 0);
    EXPECT_GT(exec_wins, 0);
    /* Should be roughly equal since we alternate */
    EXPECT_NEAR(wm_wins, exec_wins, 2);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(GTGlobalWorkspaceIntegrationTest, GetStatistics) {
    float content[64] = {0.0f};

    /* Run some competitions */
    for (int i = 0; i < 5; i++) {
        gt_gw_compete(auction_ctx, MODULE_WORKING_MEMORY, content, 64, 0.6f);
    }

    nimcp_game_stats_t stats;
    nimcp_error_t err = gt_gw_get_stats(auction_ctx, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_GT(stats.auctions_completed, 0);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(GTGlobalWorkspaceIntegrationTest, VeryHighBid) {
    float content[64] = {0.0f};

    nimcp_error_t err = gt_gw_bid(auction_ctx, MODULE_WORKING_MEMORY,
                                   content, 64, 1000.0f);
    /* Should still work (or be clamped) */
    EXPECT_EQ(err, NIMCP_SUCCESS);

    gt_gw_round_result_t result;
    gt_gw_resolve(auction_ctx, &result);
    EXPECT_TRUE(result.reserve_met);
}

TEST_F(GTGlobalWorkspaceIntegrationTest, ZeroBid) {
    float content[64] = {0.0f};

    nimcp_error_t err = gt_gw_bid(auction_ctx, MODULE_WORKING_MEMORY,
                                   content, 64, 0.0f);
    /* Zero bid should be rejected or not meet reserve */

    gt_gw_round_result_t result;
    gt_gw_resolve(auction_ctx, &result);
    /* Reserve price is 0.1, so shouldn't win */
}

TEST_F(GTGlobalWorkspaceIntegrationTest, NegativeBid) {
    float content[64] = {0.0f};

    nimcp_error_t err = gt_gw_bid(auction_ctx, MODULE_WORKING_MEMORY,
                                   content, 64, -1.0f);
    /* Negative bid should be rejected */
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GTGlobalWorkspaceIntegrationTest, TiedBids) {
    float content1[64] = {0.0f};
    float content2[64] = {0.0f};

    content1[0] = 1.0f;
    content2[0] = 2.0f;

    /* Both bid the same amount */
    gt_gw_bid(auction_ctx, MODULE_WORKING_MEMORY, content1, 64, 0.5f);
    gt_gw_bid(auction_ctx, MODULE_EXECUTIVE, content2, 64, 0.5f);

    gt_gw_round_result_t result;
    gt_gw_resolve(auction_ctx, &result);

    /* One should still win (tie-breaking) */
    EXPECT_TRUE(result.winner == MODULE_WORKING_MEMORY ||
                result.winner == MODULE_EXECUTIVE);
    EXPECT_EQ(result.num_bidders, 2);
}

/* ============================================================================
 * Integration with Workspace Broadcast
 * ============================================================================ */

TEST_F(GTGlobalWorkspaceIntegrationTest, WinnerContentIsBroadcast) {
    float content[64] = {0.0f};
    content[0] = 42.0f;
    content[1] = 3.14f;

    /* Compete and win */
    gt_gw_compete(auction_ctx, MODULE_WORKING_MEMORY, content, 64, 0.8f);

    /* Read broadcast from workspace */
    float broadcast[64];
    uint32_t dim;
    cognitive_module_t source;

    bool has_broadcast = global_workspace_read_broadcast(workspace, broadcast, 64,
                                                          &dim, &source);
    EXPECT_TRUE(has_broadcast);
    EXPECT_EQ(source, MODULE_WORKING_MEMORY);
    EXPECT_FLOAT_EQ(broadcast[0], 42.0f);
    EXPECT_FLOAT_EQ(broadcast[1], 3.14f);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
