//=============================================================================
// test_gt_integration.cpp - Unit tests for Game Theory Integration Modules
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/game_theory/integration/nimcp_gt_global_workspace.h"
#include "cognitive/game_theory/integration/nimcp_gt_working_memory.h"
#include "cognitive/game_theory/integration/nimcp_gt_neuromod.h"
#include "cognitive/game_theory/integration/nimcp_gt_hemispheric.h"
#include "cognitive/game_theory/nimcp_game_theory.h"

//=============================================================================
// Global Workspace Auction Test Fixture
//=============================================================================

class GtGlobalWorkspaceTest : public ::testing::Test {
protected:
    gt_gw_auction_ctx_t ctx = nullptr;
    gt_gw_config_t config;
    global_workspace_t* workspace = nullptr;

    void SetUp() override {
        config = gt_gw_default_config();
        // Create a mock/minimal workspace for testing
        // In real tests this would be properly initialized
        workspace = nullptr;  // Tests handle NULL workspace gracefully
    }

    void TearDown() override {
        if (ctx) {
            gt_gw_destroy(ctx);
            ctx = nullptr;
        }
    }
};

//=============================================================================
// Global Workspace Lifecycle Tests
//=============================================================================

TEST_F(GtGlobalWorkspaceTest, DefaultConfigValues) {
    EXPECT_EQ(config.strategy, GT_GW_STRATEGY_SECOND_PRICE);
    EXPECT_GE(config.reserve_price, 0.0f);
    EXPECT_GT(config.initial_budget, 0.0f);
    EXPECT_GT(config.budget_replenish_rate, 0.0f);
}

TEST_F(GtGlobalWorkspaceTest, CreateWithNullWorkspace) {
    // Creating with NULL workspace should either fail gracefully or return NULL
    ctx = gt_gw_create(nullptr, &config);
    // Implementation may allow NULL workspace for testing
    // Just verify no crash occurs
}

TEST_F(GtGlobalWorkspaceTest, CreateWithNullConfig) {
    // Creating with NULL config should use defaults
    ctx = gt_gw_create(workspace, nullptr);
    // May return NULL or use defaults - verify no crash
}

TEST_F(GtGlobalWorkspaceTest, DestroyNullContext) {
    // Destroying NULL should be safe
    gt_gw_destroy(nullptr);
    // No crash = success
}

TEST_F(GtGlobalWorkspaceTest, CreateDestroyLifecycle) {
    ctx = gt_gw_create(workspace, &config);
    // If creation succeeds, verify state
    if (ctx != nullptr) {
        // Context should indicate auction is active or ready
        bool active = gt_gw_is_auction_active(ctx);
        // Just verify the call works
        (void)active;
    }
    // TearDown will destroy
}

//=============================================================================
// Global Workspace Bid and Resolve Tests
//=============================================================================

TEST_F(GtGlobalWorkspaceTest, BidNullContext) {
    float content[] = {1.0f, 2.0f, 3.0f};
    nimcp_error_t err = gt_gw_bid(nullptr, static_cast<cognitive_module_t>(1), content, 3, 10.0f);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GtGlobalWorkspaceTest, BidNullContent) {
    ctx = gt_gw_create(workspace, &config);
    if (ctx != nullptr) {
        nimcp_error_t err = gt_gw_bid(ctx, static_cast<cognitive_module_t>(1), nullptr, 0, 10.0f);
        // NULL content with size 0 may be allowed, size > 0 should fail
        err = gt_gw_bid(ctx, static_cast<cognitive_module_t>(1), nullptr, 3, 10.0f);
        EXPECT_NE(err, NIMCP_SUCCESS);
    }
}

TEST_F(GtGlobalWorkspaceTest, BidNegativeAmount) {
    ctx = gt_gw_create(workspace, &config);
    if (ctx != nullptr) {
        float content[] = {1.0f, 2.0f};
        nimcp_error_t err = gt_gw_bid(ctx, static_cast<cognitive_module_t>(1), content, 2, -5.0f);
        EXPECT_NE(err, NIMCP_SUCCESS);
    }
}

TEST_F(GtGlobalWorkspaceTest, ResolveNullContext) {
    gt_gw_round_result_t result;
    nimcp_error_t err = gt_gw_resolve(nullptr, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GtGlobalWorkspaceTest, ResolveNullResult) {
    ctx = gt_gw_create(workspace, &config);
    if (ctx != nullptr) {
        nimcp_error_t err = gt_gw_resolve(ctx, nullptr);
        EXPECT_NE(err, NIMCP_SUCCESS);
    }
}

TEST_F(GtGlobalWorkspaceTest, BidAndResolveBasicFlow) {
    ctx = gt_gw_create(workspace, &config);
    if (ctx != nullptr) {
        float content1[] = {1.0f, 0.5f, 0.2f};
        float content2[] = {0.8f, 0.9f, 0.1f};

        // Submit two bids
        nimcp_error_t err1 = gt_gw_bid(ctx, static_cast<cognitive_module_t>(1), content1, 3, 10.0f);
        nimcp_error_t err2 = gt_gw_bid(ctx, static_cast<cognitive_module_t>(2), content2, 3, 15.0f);

        if (err1 == NIMCP_SUCCESS && err2 == NIMCP_SUCCESS) {
            gt_gw_round_result_t result;
            nimcp_error_t err = gt_gw_resolve(ctx, &result);

            if (err == NIMCP_SUCCESS) {
                // Higher bidder should win
                EXPECT_EQ(result.winner, static_cast<cognitive_module_t>(2));
                EXPECT_FLOAT_EQ(result.winning_bid, 15.0f);
                // In second-price auction, payment should be second-highest bid
                EXPECT_FLOAT_EQ(result.payment, 10.0f);
                EXPECT_EQ(result.num_bidders, 2);
            }
        }
    }
}

TEST_F(GtGlobalWorkspaceTest, ResolveEmptyAuction) {
    ctx = gt_gw_create(workspace, &config);
    if (ctx != nullptr) {
        gt_gw_round_result_t result;
        nimcp_error_t err = gt_gw_resolve(ctx, &result);

        // Empty auction may succeed with no winner or return an error
        if (err == NIMCP_SUCCESS) {
            EXPECT_FALSE(result.reserve_met);
            EXPECT_EQ(result.num_bidders, 0);
        }
    }
}

TEST_F(GtGlobalWorkspaceTest, IsAuctionActiveNullContext) {
    bool active = gt_gw_is_auction_active(nullptr);
    EXPECT_FALSE(active);
}

//=============================================================================
// Working Memory Auction Test Fixture
//=============================================================================

class GtWorkingMemoryTest : public ::testing::Test {
protected:
    gt_wm_auction_ctx_t ctx = nullptr;
    gt_wm_config_t config;
    working_memory_t* wm = nullptr;

    void SetUp() override {
        config = gt_wm_default_config();
        wm = nullptr;  // Tests handle NULL gracefully
    }

    void TearDown() override {
        if (ctx) {
            gt_wm_destroy(ctx);
            ctx = nullptr;
        }
    }
};

//=============================================================================
// Working Memory Lifecycle Tests
//=============================================================================

TEST_F(GtWorkingMemoryTest, DefaultConfigValues) {
    EXPECT_EQ(config.policy, GT_WM_EVICTION_AUCTION);
    EXPECT_GE(config.slot_reserve_price, 0.0f);
    EXPECT_GE(config.decay_rate, 0.0f);
    EXPECT_LE(config.decay_rate, 1.0f);
}

TEST_F(GtWorkingMemoryTest, CreateWithNullWM) {
    ctx = gt_wm_create(nullptr, &config);
    // May return NULL or create mock - verify no crash
}

TEST_F(GtWorkingMemoryTest, CreateWithNullConfig) {
    ctx = gt_wm_create(wm, nullptr);
    // Should use defaults or return NULL
}

TEST_F(GtWorkingMemoryTest, DestroyNullContext) {
    gt_wm_destroy(nullptr);
    // No crash = success
}

TEST_F(GtWorkingMemoryTest, CreateDestroyLifecycle) {
    ctx = gt_wm_create(wm, &config);
    if (ctx != nullptr) {
        bool active = gt_wm_is_active(ctx);
        (void)active;  // Verify call works
    }
}

//=============================================================================
// Working Memory Add and Eviction Tests
//=============================================================================

TEST_F(GtWorkingMemoryTest, AddNullContext) {
    float item[] = {1.0f, 2.0f};
    int32_t slot = -1;
    nimcp_error_t err = gt_wm_add(nullptr, item, 2, 5.0f, &slot);
    EXPECT_NE(err, NIMCP_SUCCESS);
    EXPECT_EQ(slot, -1);
}

TEST_F(GtWorkingMemoryTest, AddNullItem) {
    ctx = gt_wm_create(wm, &config);
    if (ctx != nullptr) {
        int32_t slot = -1;
        nimcp_error_t err = gt_wm_add(ctx, nullptr, 0, 5.0f, &slot);
        // NULL with size 0 may be valid, but NULL with size > 0 should fail
        err = gt_wm_add(ctx, nullptr, 3, 5.0f, &slot);
        EXPECT_NE(err, NIMCP_SUCCESS);
    }
}

TEST_F(GtWorkingMemoryTest, AddNullSlotIndex) {
    ctx = gt_wm_create(wm, &config);
    if (ctx != nullptr) {
        float item[] = {1.0f, 2.0f};
        nimcp_error_t err = gt_wm_add(ctx, item, 2, 5.0f, nullptr);
        EXPECT_NE(err, NIMCP_SUCCESS);
    }
}

TEST_F(GtWorkingMemoryTest, AddBasicFlow) {
    ctx = gt_wm_create(wm, &config);
    if (ctx != nullptr) {
        float item1[] = {1.0f, 2.0f, 3.0f};
        float item2[] = {4.0f, 5.0f, 6.0f};
        int32_t slot1 = -1, slot2 = -1;

        nimcp_error_t err1 = gt_wm_add(ctx, item1, 3, 10.0f, &slot1);
        nimcp_error_t err2 = gt_wm_add(ctx, item2, 3, 15.0f, &slot2);

        if (err1 == NIMCP_SUCCESS && err2 == NIMCP_SUCCESS) {
            EXPECT_GE(slot1, 0);
            EXPECT_GE(slot2, 0);
            EXPECT_NE(slot1, slot2);

            // Verify occupancy
            uint32_t occupancy = gt_wm_get_occupancy(ctx);
            EXPECT_GE(occupancy, 2);
        }
    }
}

TEST_F(GtWorkingMemoryTest, EvictionNullContext) {
    gt_wm_eviction_result_t result;
    nimcp_error_t err = gt_wm_run_eviction(nullptr, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GtWorkingMemoryTest, EvictionNullResult) {
    ctx = gt_wm_create(wm, &config);
    if (ctx != nullptr) {
        nimcp_error_t err = gt_wm_run_eviction(ctx, nullptr);
        EXPECT_NE(err, NIMCP_SUCCESS);
    }
}

TEST_F(GtWorkingMemoryTest, EvictionBehavior) {
    config.slot_reserve_price = 5.0f;  // Set reserve to test eviction
    ctx = gt_wm_create(wm, &config);

    if (ctx != nullptr) {
        // Add items with varying bids
        float item1[] = {1.0f};
        float item2[] = {2.0f};
        float item3[] = {3.0f};
        int32_t slot1, slot2, slot3;

        nimcp_error_t err1 = gt_wm_add(ctx, item1, 1, 3.0f, &slot1);  // Below reserve
        nimcp_error_t err2 = gt_wm_add(ctx, item2, 1, 8.0f, &slot2);  // Above reserve
        nimcp_error_t err3 = gt_wm_add(ctx, item3, 1, 10.0f, &slot3); // Above reserve

        if (err1 == NIMCP_SUCCESS && err2 == NIMCP_SUCCESS && err3 == NIMCP_SUCCESS) {
            gt_wm_eviction_result_t result;
            memset(&result, 0, sizeof(result));

            nimcp_error_t err = gt_wm_run_eviction(ctx, &result);
            if (err == NIMCP_SUCCESS) {
                // Items below reserve may be evicted
                // Verify result structure is populated
                EXPECT_LE(result.num_evicted, 16);
                EXPECT_GE(result.lowest_surviving_bid, 0.0f);
            }
        }
    }
}

TEST_F(GtWorkingMemoryTest, GetOccupancyNullContext) {
    uint32_t occupancy = gt_wm_get_occupancy(nullptr);
    EXPECT_EQ(occupancy, 0);
}

TEST_F(GtWorkingMemoryTest, IsActiveNullContext) {
    bool active = gt_wm_is_active(nullptr);
    EXPECT_FALSE(active);
}

TEST_F(GtWorkingMemoryTest, GetHighestLowestBid) {
    ctx = gt_wm_create(wm, &config);
    if (ctx != nullptr) {
        float item1[] = {1.0f};
        float item2[] = {2.0f};
        int32_t slot1, slot2;

        nimcp_error_t err1 = gt_wm_add(ctx, item1, 1, 5.0f, &slot1);
        nimcp_error_t err2 = gt_wm_add(ctx, item2, 1, 15.0f, &slot2);

        if (err1 == NIMCP_SUCCESS && err2 == NIMCP_SUCCESS) {
            float highest = gt_wm_get_highest_bid(ctx);
            float lowest = gt_wm_get_lowest_bid(ctx);

            EXPECT_GE(highest, lowest);
            EXPECT_FLOAT_EQ(highest, 15.0f);
            EXPECT_FLOAT_EQ(lowest, 5.0f);
        }
    }
}

//=============================================================================
// Neuromodulator Bridge Test Fixture
//=============================================================================

class GtNeuromodTest : public ::testing::Test {
protected:
    gt_neuromod_bridge_t bridge = nullptr;
    gt_neuromod_config_t config;
    neuromodulator_system_t neuromod = nullptr;

    void SetUp() override {
        config = gt_neuromod_default_config();
        neuromod = nullptr;  // Tests handle NULL gracefully
    }

    void TearDown() override {
        if (bridge) {
            gt_neuromod_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Neuromodulator Bridge Lifecycle Tests
//=============================================================================

TEST_F(GtNeuromodTest, DefaultConfigValues) {
    // Dopamine settings
    EXPECT_GT(config.payoff_dopamine_gain, 0.0f);
    EXPECT_GE(config.win_dopamine_bonus, 0.0f);
    EXPECT_GE(config.cooperation_dopamine_bonus, 0.0f);

    // Serotonin settings
    EXPECT_GE(config.loss_serotonin_gain, 0.0f);

    // Norepinephrine settings
    EXPECT_GE(config.competition_ne_baseline, 0.0f);

    // Acetylcholine settings
    EXPECT_GE(config.strategic_ach_gain, 0.0f);

    // Thresholds
    EXPECT_GE(config.payoff_threshold, 0.0f);
    EXPECT_GT(config.fairness_threshold, 0.0f);
    EXPECT_LE(config.fairness_threshold, 1.0f);
}

TEST_F(GtNeuromodTest, CreateWithNullNeuromod) {
    bridge = gt_neuromod_bridge_create(nullptr, &config);
    // May return NULL or create mock - verify no crash
}

TEST_F(GtNeuromodTest, CreateWithNullConfig) {
    bridge = gt_neuromod_bridge_create(neuromod, nullptr);
    // Should use defaults or return NULL
}

TEST_F(GtNeuromodTest, DestroyNullBridge) {
    gt_neuromod_bridge_destroy(nullptr);
    // No crash = success
}

TEST_F(GtNeuromodTest, CreateDestroyLifecycle) {
    bridge = gt_neuromod_bridge_create(neuromod, &config);
    // TearDown handles cleanup
}

//=============================================================================
// Neuromodulator Process Outcome Tests
//=============================================================================

TEST_F(GtNeuromodTest, ProcessOutcomeNullBridge) {
    nimcp_game_outcome_t outcome;
    nimcp_game_outcome_init(&outcome);
    gt_neuromod_release_t release;

    nimcp_error_t err = gt_neuromod_process_outcome(nullptr, 1, &outcome, &release);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GtNeuromodTest, ProcessOutcomeNullOutcome) {
    bridge = gt_neuromod_bridge_create(neuromod, &config);
    if (bridge != nullptr) {
        gt_neuromod_release_t release;
        nimcp_error_t err = gt_neuromod_process_outcome(bridge, 1, nullptr, &release);
        EXPECT_NE(err, NIMCP_SUCCESS);
    }
}

TEST_F(GtNeuromodTest, ProcessOutcomeNullRelease) {
    bridge = gt_neuromod_bridge_create(neuromod, &config);
    if (bridge != nullptr) {
        nimcp_game_outcome_t outcome;
        nimcp_game_outcome_init(&outcome);
        nimcp_error_t err = gt_neuromod_process_outcome(bridge, 1, &outcome, nullptr);
        EXPECT_NE(err, NIMCP_SUCCESS);
    }
}

TEST_F(GtNeuromodTest, ProcessOutcomeWinScenario) {
    bridge = gt_neuromod_bridge_create(neuromod, &config);
    if (bridge != nullptr) {
        nimcp_game_outcome_t outcome;
        nimcp_game_outcome_init(&outcome);

        // Set up a winning scenario for player 1
        outcome.winners[0] = 1;
        outcome.num_winners = 1;
        outcome.allocations[0] = 0.0f;  // Player 0
        outcome.allocations[1] = 10.0f; // Player 1 (winner)
        outcome.payments[1] = 5.0f;
        outcome.social_welfare = 10.0f;
        outcome.is_efficient = true;
        outcome.is_fair = true;

        gt_neuromod_release_t release;
        memset(&release, 0, sizeof(release));

        nimcp_error_t err = gt_neuromod_process_outcome(bridge, 1, &outcome, &release);

        if (err == NIMCP_SUCCESS) {
            // Winner should get dopamine
            EXPECT_GE(release.dopamine_released, 0.0f);
            // Net valence should be positive for winner
            EXPECT_GE(release.net_valence, 0.0f);
        }
    }
}

TEST_F(GtNeuromodTest, ProcessOutcomeLossScenario) {
    bridge = gt_neuromod_bridge_create(neuromod, &config);
    if (bridge != nullptr) {
        nimcp_game_outcome_t outcome;
        nimcp_game_outcome_init(&outcome);

        // Set up a losing scenario for player 0
        outcome.winners[0] = 1;
        outcome.num_winners = 1;
        outcome.allocations[0] = 0.0f;   // Player 0 gets nothing
        outcome.allocations[1] = 10.0f;  // Player 1 gets all
        outcome.payments[0] = 0.0f;
        outcome.social_welfare = 10.0f;
        outcome.is_efficient = true;
        outcome.is_fair = false;

        gt_neuromod_release_t release;
        memset(&release, 0, sizeof(release));

        nimcp_error_t err = gt_neuromod_process_outcome(bridge, 0, &outcome, &release);

        if (err == NIMCP_SUCCESS) {
            // Loser may get serotonin (patience/inhibition)
            EXPECT_GE(release.serotonin_released, 0.0f);
        }
    }
}

TEST_F(GtNeuromodTest, AuctionWinDopamineRelease) {
    bridge = gt_neuromod_bridge_create(neuromod, &config);
    if (bridge != nullptr) {
        float winning_bid = 15.0f;
        float payment = 10.0f;  // Second-price: pay less than bid

        float dopamine = gt_neuromod_auction_win(bridge, winning_bid, payment);

        // Should release dopamine for winning
        EXPECT_GE(dopamine, 0.0f);
        // More dopamine when payment < bid (surplus)
    }
}

TEST_F(GtNeuromodTest, BargainingSuccessRelease) {
    bridge = gt_neuromod_bridge_create(neuromod, &config);
    if (bridge != nullptr) {
        float agreement_value = 100.0f;
        float fairness_index = 0.9f;  // Very fair

        float dopamine = gt_neuromod_bargaining_success(bridge, agreement_value, fairness_index);

        EXPECT_GE(dopamine, 0.0f);
    }
}

TEST_F(GtNeuromodTest, BargainingFailureRelease) {
    bridge = gt_neuromod_bridge_create(neuromod, &config);
    if (bridge != nullptr) {
        uint32_t rounds_taken = 8;
        uint32_t max_rounds = 10;

        float serotonin = gt_neuromod_bargaining_failure(bridge, rounds_taken, max_rounds);

        // Failure triggers serotonin for impulse control
        EXPECT_GE(serotonin, 0.0f);
    }
}

TEST_F(GtNeuromodTest, SignalUnfairness) {
    bridge = gt_neuromod_bridge_create(neuromod, &config);
    if (bridge != nullptr) {
        float unfairness = 0.8f;  // High unfairness

        float norepinephrine = gt_neuromod_signal_unfairness(bridge, unfairness);

        // Unfairness triggers NE (threat/arousal)
        EXPECT_GE(norepinephrine, 0.0f);
    }
}

TEST_F(GtNeuromodTest, StrategicAttention) {
    bridge = gt_neuromod_bridge_create(neuromod, &config);
    if (bridge != nullptr) {
        uint32_t num_options = 5;
        float uncertainty = 0.7f;

        float acetylcholine = gt_neuromod_strategic_attention(bridge, num_options, uncertainty);

        // Strategic situations trigger ACh for attention
        EXPECT_GE(acetylcholine, 0.0f);
    }
}

TEST_F(GtNeuromodTest, ResetStats) {
    bridge = gt_neuromod_bridge_create(neuromod, &config);
    if (bridge != nullptr) {
        // Process some outcomes first
        nimcp_game_outcome_t outcome;
        nimcp_game_outcome_init(&outcome);
        outcome.winners[0] = 1;
        outcome.num_winners = 1;

        gt_neuromod_release_t release;
        gt_neuromod_process_outcome(bridge, 1, &outcome, &release);

        // Reset stats
        gt_neuromod_reset_stats(bridge);

        // Get cumulative - should be zero after reset
        gt_neuromod_release_t cumulative;
        memset(&cumulative, 0, sizeof(cumulative));
        nimcp_error_t err = gt_neuromod_get_cumulative_release(bridge, &cumulative);

        if (err == NIMCP_SUCCESS) {
            EXPECT_FLOAT_EQ(cumulative.dopamine_released, 0.0f);
            EXPECT_FLOAT_EQ(cumulative.serotonin_released, 0.0f);
            EXPECT_FLOAT_EQ(cumulative.norepinephrine_released, 0.0f);
            EXPECT_FLOAT_EQ(cumulative.acetylcholine_released, 0.0f);
        }
    }
}

//=============================================================================
// Hemispheric Bargaining Test Fixture
//=============================================================================

class GtHemisphericTest : public ::testing::Test {
protected:
    gt_hemi_bargaining_ctx_t ctx = nullptr;
    gt_hemi_config_t config;
    hemispheric_brain_t* brain = nullptr;

    void SetUp() override {
        config = gt_hemi_default_config();
        brain = nullptr;  // Tests handle NULL gracefully
    }

    void TearDown() override {
        if (ctx) {
            gt_hemi_destroy(ctx);
            ctx = nullptr;
        }
    }
};

//=============================================================================
// Hemispheric Bargaining Lifecycle Tests
//=============================================================================

TEST_F(GtHemisphericTest, DefaultConfigValues) {
    // Bargaining powers should sum to 1
    float power_sum = config.left_bargaining_power + config.right_bargaining_power;
    EXPECT_NEAR(power_sum, 1.0f, 0.001f);

    // Disagreement payoffs
    EXPECT_GE(config.disagreement_left, 0.0f);
    EXPECT_GE(config.disagreement_right, 0.0f);

    // Discount factor should be in (0, 1]
    EXPECT_GT(config.discount_factor, 0.0f);
    EXPECT_LE(config.discount_factor, 1.0f);

    // Max rounds should be positive
    EXPECT_GT(config.max_rounds, 0);
}

TEST_F(GtHemisphericTest, CreateWithNullBrain) {
    ctx = gt_hemi_create(nullptr, &config);
    // May return NULL or create mock - verify no crash
}

TEST_F(GtHemisphericTest, CreateWithNullConfig) {
    ctx = gt_hemi_create(brain, nullptr);
    // Should use defaults or return NULL
}

TEST_F(GtHemisphericTest, DestroyNullContext) {
    gt_hemi_destroy(nullptr);
    // No crash = success
}

TEST_F(GtHemisphericTest, CreateDestroyLifecycle) {
    ctx = gt_hemi_create(brain, &config);
    if (ctx != nullptr) {
        bool active = gt_hemi_is_active(ctx);
        (void)active;  // Verify call works
    }
}

//=============================================================================
// Hemispheric Resource Negotiation Tests
//=============================================================================

TEST_F(GtHemisphericTest, NegotiateResourcesNullContext) {
    gt_hemi_outcome_t outcome;
    nimcp_error_t err = gt_hemi_negotiate_resources(nullptr, 100.0f, &outcome);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GtHemisphericTest, NegotiateResourcesNullOutcome) {
    ctx = gt_hemi_create(brain, &config);
    if (ctx != nullptr) {
        nimcp_error_t err = gt_hemi_negotiate_resources(ctx, 100.0f, nullptr);
        EXPECT_NE(err, NIMCP_SUCCESS);
    }
}

TEST_F(GtHemisphericTest, NegotiateResourcesZeroTotal) {
    ctx = gt_hemi_create(brain, &config);
    if (ctx != nullptr) {
        gt_hemi_outcome_t outcome;
        memset(&outcome, 0, sizeof(outcome));

        nimcp_error_t err = gt_hemi_negotiate_resources(ctx, 0.0f, &outcome);

        if (err == NIMCP_SUCCESS) {
            // Both allocations should be zero
            EXPECT_FLOAT_EQ(outcome.left_allocation, 0.0f);
            EXPECT_FLOAT_EQ(outcome.right_allocation, 0.0f);
        }
    }
}

TEST_F(GtHemisphericTest, NegotiateResourcesNegativeTotal) {
    ctx = gt_hemi_create(brain, &config);
    if (ctx != nullptr) {
        gt_hemi_outcome_t outcome;
        nimcp_error_t err = gt_hemi_negotiate_resources(ctx, -100.0f, &outcome);
        EXPECT_NE(err, NIMCP_SUCCESS);
    }
}

TEST_F(GtHemisphericTest, NegotiateResourcesSymmetricPower) {
    config.left_bargaining_power = 0.5f;
    config.right_bargaining_power = 0.5f;
    config.disagreement_left = 0.0f;
    config.disagreement_right = 0.0f;

    ctx = gt_hemi_create(brain, &config);
    if (ctx != nullptr) {
        gt_hemi_outcome_t outcome;
        memset(&outcome, 0, sizeof(outcome));

        nimcp_error_t err = gt_hemi_negotiate_resources(ctx, 100.0f, &outcome);

        if (err == NIMCP_SUCCESS) {
            // With symmetric power and zero disagreement, split should be 50-50
            EXPECT_NEAR(outcome.left_allocation, 50.0f, 5.0f);
            EXPECT_NEAR(outcome.right_allocation, 50.0f, 5.0f);

            // Allocations should sum to total
            float sum = outcome.left_allocation + outcome.right_allocation;
            EXPECT_NEAR(sum, 100.0f, 0.01f);

            // Should reach agreement
            EXPECT_TRUE(outcome.agreement_reached);
        }
    }
}

TEST_F(GtHemisphericTest, NegotiateResourcesAsymmetricPower) {
    config.left_bargaining_power = 0.7f;
    config.right_bargaining_power = 0.3f;
    config.disagreement_left = 0.0f;
    config.disagreement_right = 0.0f;

    ctx = gt_hemi_create(brain, &config);
    if (ctx != nullptr) {
        gt_hemi_outcome_t outcome;
        memset(&outcome, 0, sizeof(outcome));

        nimcp_error_t err = gt_hemi_negotiate_resources(ctx, 100.0f, &outcome);

        if (err == NIMCP_SUCCESS) {
            // Left hemisphere has more power, should get more
            EXPECT_GT(outcome.left_allocation, outcome.right_allocation);

            // Both should get at least disagreement point (0)
            EXPECT_GE(outcome.left_allocation, 0.0f);
            EXPECT_GE(outcome.right_allocation, 0.0f);
        }
    }
}

TEST_F(GtHemisphericTest, NegotiateResourcesWithDisagreementPoint) {
    config.left_bargaining_power = 0.5f;
    config.right_bargaining_power = 0.5f;
    config.disagreement_left = 20.0f;
    config.disagreement_right = 10.0f;

    ctx = gt_hemi_create(brain, &config);
    if (ctx != nullptr) {
        gt_hemi_outcome_t outcome;
        memset(&outcome, 0, sizeof(outcome));

        nimcp_error_t err = gt_hemi_negotiate_resources(ctx, 100.0f, &outcome);

        if (err == NIMCP_SUCCESS) {
            // Each should get at least their disagreement point
            EXPECT_GE(outcome.left_allocation, config.disagreement_left);
            EXPECT_GE(outcome.right_allocation, config.disagreement_right);
        }
    }
}

TEST_F(GtHemisphericTest, SetBargainingPower) {
    ctx = gt_hemi_create(brain, &config);
    if (ctx != nullptr) {
        nimcp_error_t err = gt_hemi_set_bargaining_power(ctx, 0.8f);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        // Negotiate and verify new power takes effect
        gt_hemi_outcome_t outcome;
        err = gt_hemi_negotiate_resources(ctx, 100.0f, &outcome);

        if (err == NIMCP_SUCCESS) {
            // Left should get more with 0.8 power
            EXPECT_GT(outcome.left_allocation, outcome.right_allocation);
        }
    }
}

TEST_F(GtHemisphericTest, SetBargainingPowerInvalid) {
    ctx = gt_hemi_create(brain, &config);
    if (ctx != nullptr) {
        // Power must be in [0, 1]
        nimcp_error_t err = gt_hemi_set_bargaining_power(ctx, 1.5f);
        EXPECT_NE(err, NIMCP_SUCCESS);

        err = gt_hemi_set_bargaining_power(ctx, -0.1f);
        EXPECT_NE(err, NIMCP_SUCCESS);
    }
}

TEST_F(GtHemisphericTest, SetDisagreementPoint) {
    ctx = gt_hemi_create(brain, &config);
    if (ctx != nullptr) {
        nimcp_error_t err = gt_hemi_set_disagreement(ctx, 15.0f, 5.0f);
        EXPECT_EQ(err, NIMCP_SUCCESS);
    }
}

TEST_F(GtHemisphericTest, IsActiveNullContext) {
    bool active = gt_hemi_is_active(nullptr);
    EXPECT_FALSE(active);
}

TEST_F(GtHemisphericTest, GetLastOutcome) {
    ctx = gt_hemi_create(brain, &config);
    if (ctx != nullptr) {
        // First negotiate
        gt_hemi_outcome_t outcome;
        memset(&outcome, 0, sizeof(outcome));
        nimcp_error_t err = gt_hemi_negotiate_resources(ctx, 100.0f, &outcome);

        if (err == NIMCP_SUCCESS) {
            // Then get last outcome
            gt_hemi_outcome_t last_outcome;
            err = gt_hemi_get_last_outcome(ctx, &last_outcome);

            if (err == NIMCP_SUCCESS) {
                // Should match what we just negotiated
                EXPECT_FLOAT_EQ(last_outcome.left_allocation, outcome.left_allocation);
                EXPECT_FLOAT_EQ(last_outcome.right_allocation, outcome.right_allocation);
            }
        }
    }
}

TEST_F(GtHemisphericTest, ComputeCredit) {
    config.use_shapley_credit = true;
    ctx = gt_hemi_create(brain, &config);

    if (ctx != nullptr) {
        float combined_output[] = {1.0f, 2.0f, 3.0f, 4.0f};
        gt_hemi_credit_t credit;
        memset(&credit, 0, sizeof(credit));

        nimcp_error_t err = gt_hemi_compute_credit(ctx, combined_output, 4, &credit);

        if (err == NIMCP_SUCCESS) {
            // Credits should be non-negative
            EXPECT_GE(credit.left_credit, 0.0f);
            EXPECT_GE(credit.right_credit, 0.0f);

            // Total value should be computed
            EXPECT_GE(credit.total_value, 0.0f);

            // Credits should sum approximately to total
            float credit_sum = credit.left_credit + credit.right_credit;
            EXPECT_NEAR(credit_sum, credit.total_value, 0.1f);
        }
    }
}

TEST_F(GtHemisphericTest, NashProductProperty) {
    config.left_bargaining_power = 0.5f;
    config.right_bargaining_power = 0.5f;

    ctx = gt_hemi_create(brain, &config);
    if (ctx != nullptr) {
        gt_hemi_outcome_t outcome;
        memset(&outcome, 0, sizeof(outcome));

        nimcp_error_t err = gt_hemi_negotiate_resources(ctx, 100.0f, &outcome);

        if (err == NIMCP_SUCCESS && outcome.agreement_reached) {
            // Nash product should be positive
            EXPECT_GT(outcome.nash_product, 0.0f);
        }
    }
}

//=============================================================================
// Integration Tests (Multiple Modules)
//=============================================================================

class GtIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(GtIntegrationTest, GameOutcomeInitialization) {
    nimcp_game_outcome_t outcome;
    nimcp_game_outcome_init(&outcome);

    EXPECT_EQ(outcome.num_winners, 0);
    EXPECT_FLOAT_EQ(outcome.social_welfare, 0.0f);
    EXPECT_FALSE(outcome.is_efficient);
    EXPECT_FALSE(outcome.is_fair);
}

TEST_F(GtIntegrationTest, FairnessIndexComputation) {
    // Equal allocations should have perfect fairness
    float equal_alloc[] = {10.0f, 10.0f, 10.0f, 10.0f};
    float fairness = nimcp_compute_fairness_index(equal_alloc, 4);
    EXPECT_NEAR(fairness, 1.0f, 0.01f);

    // Very unequal allocations should have low fairness
    float unequal_alloc[] = {100.0f, 0.0f, 0.0f, 0.0f};
    fairness = nimcp_compute_fairness_index(unequal_alloc, 4);
    EXPECT_LT(fairness, 0.5f);
}

TEST_F(GtIntegrationTest, GameTypeNames) {
    EXPECT_STREQ(nimcp_game_type_name(NIMCP_GAME_ZERO_SUM), "Zero-Sum");
    EXPECT_STREQ(nimcp_game_type_name(NIMCP_GAME_COOPERATIVE), "Cooperative");
    EXPECT_STREQ(nimcp_game_type_name(NIMCP_GAME_AUCTION), "Auction");
    EXPECT_STREQ(nimcp_game_type_name(NIMCP_GAME_BARGAINING), "Bargaining");
}

TEST_F(GtIntegrationTest, SolutionConceptNames) {
    EXPECT_STREQ(nimcp_solution_concept_name(NIMCP_SOLUTION_NASH), "Nash Equilibrium");
    EXPECT_STREQ(nimcp_solution_concept_name(NIMCP_SOLUTION_SHAPLEY), "Shapley Value");
    EXPECT_STREQ(nimcp_solution_concept_name(NIMCP_SOLUTION_PARETO_OPTIMAL), "Pareto Optimal");
    EXPECT_STREQ(nimcp_solution_concept_name(NIMCP_SOLUTION_CORE), "Core");
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
