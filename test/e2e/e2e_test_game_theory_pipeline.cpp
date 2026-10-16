//=============================================================================
// e2e_test_game_theory_pipeline.cpp - Game Theory Pipeline E2E Tests
//=============================================================================
/**
 * @file e2e_test_game_theory_pipeline.cpp
 * @brief End-to-end tests for game theory module pipeline
 *
 * WHAT: Complete pipeline tests for game theory subsystem
 * WHY:  Verify full system integration from auctions to credit assignment
 * HOW:  GoogleTest with realistic cognitive system simulations
 *
 * Test Scenarios:
 * 1. Auction Pipeline - Modules compete for Global Workspace access
 * 2. Multi-Round Auctions - Multiple auction rounds with winner determination
 * 3. Hemispheric Bargaining - Resource allocation via Nash bargaining
 * 4. Credit Assignment - Shapley value computation for contributions
 * 5. Full Cycle Integration - Complete bid->auction->broadcast->credit flow
 *
 * @version 1.0.0
 * @date 2025-12-27
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/game_theory/nimcp_game_theory.h"
#include "cognitive/game_theory/nimcp_auction.h"
#include "cognitive/game_theory/nimcp_bargaining.h"
#include "cognitive/game_theory/nimcp_credit_assignment.h"
#include "cognitive/game_theory/integration/nimcp_gt_global_workspace.h"
#include "cognitive/game_theory/integration/nimcp_gt_hemispheric.h"
#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"

//=============================================================================
// Test Constants
//=============================================================================

static const uint32_t CONTENT_DIM = 64;
static const uint32_t NUM_MODULES = 4;
static const uint32_t NUM_AUCTION_ROUNDS = 10;
static const float CONVERGENCE_EPSILON = 1e-4f;

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * GameTheoryPipelineE2E Fixture
 *
 * WHAT: Fixture for game theory pipeline E2E tests
 * WHY:  Provides common setup/teardown for game theory tests
 * HOW:  Creates game theory system and test resources
 */
class GameTheoryPipelineE2E : public ::testing::Test {
protected:
    nimcp_gt_system_t gt_system = nullptr;
    nimcp_auction_t auction = nullptr;
    nimcp_bargaining_t bargaining = nullptr;
    nimcp_credit_system_t credit_system = nullptr;
    global_workspace_t* workspace = nullptr;
    hemispheric_brain_t* hemi_brain = nullptr;
    gt_gw_auction_ctx_t gw_auction_ctx = nullptr;
    gt_hemi_bargaining_ctx_t hemi_bargain_ctx = nullptr;

    void SetUp() override {
        // Create game theory system
        nimcp_gt_config_t gt_config = nimcp_gt_default_config();
        gt_config.enable_statistics = true;
        gt_config.enable_history = true;
        gt_system = nimcp_gt_create(&gt_config);
    }

    void TearDown() override {
        // Clean up in reverse order
        if (hemi_bargain_ctx) {
            gt_hemi_destroy(hemi_bargain_ctx);
            hemi_bargain_ctx = nullptr;
        }
        if (gw_auction_ctx) {
            gt_gw_destroy(gw_auction_ctx);
            gw_auction_ctx = nullptr;
        }
        if (credit_system) {
            nimcp_credit_destroy(credit_system);
            credit_system = nullptr;
        }
        if (bargaining) {
            nimcp_bargaining_destroy(bargaining);
            bargaining = nullptr;
        }
        if (auction) {
            nimcp_auction_destroy(auction);
            auction = nullptr;
        }
        if (workspace) {
            global_workspace_destroy(workspace);
            workspace = nullptr;
        }
        if (hemi_brain) {
            hemispheric_brain_destroy(hemi_brain);
            hemi_brain = nullptr;
        }
        if (gt_system) {
            nimcp_gt_destroy(gt_system);
            gt_system = nullptr;
        }
    }

    // Helper: Generate random content for module
    void generate_content(float* content, uint32_t dim, float seed) {
        for (uint32_t i = 0; i < dim; i++) {
            content[i] = sinf(seed + i * 0.1f) * 0.5f + 0.5f;
        }
    }

    // Helper: Simulate processing time
    void simulate_work(int ms = 10) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

    // Helper: Coalition value function for credit assignment tests
    static float test_coalition_value(uint32_t coalition, uint32_t num_players, void* user_data) {
        (void)user_data;
        // Superadditive value function: larger coalitions are more valuable
        int count = 0;
        for (uint32_t i = 0; i < num_players; i++) {
            if (coalition & (1u << i)) {
                count++;
            }
        }
        // Value = count^1.5 (superadditive)
        return powf((float)count, 1.5f);
    }
};

//=============================================================================
// 1. AUCTION PIPELINE E2E TESTS
//=============================================================================

/**
 * TEST: AuctionPipeline_SecondPriceAuction
 *
 * WHAT: Test second-price (Vickrey) auction for workspace access
 * WHY:  Verify strategyproof auction mechanism works correctly
 * EXPECT: Highest bidder wins, pays second-highest price
 */
TEST_F(GameTheoryPipelineE2E, AuctionPipeline_SecondPriceAuction) {
    // Create second-price auction
    nimcp_auction_config_t config = nimcp_auction_default_config();
    config.type = NIMCP_AUCTION_SECOND_PRICE;
    config.reserve_price = 0.1f;
    config.max_bidders = NUM_MODULES;

    auction = nimcp_auction_create(&config);
    ASSERT_NE(auction, nullptr) << "Failed to create second-price auction";

    // Verify auction is strategyproof
    EXPECT_TRUE(nimcp_auction_is_strategyproof(NIMCP_AUCTION_SECOND_PRICE));

    // Submit bids from simulated cognitive modules
    float bids[NUM_MODULES] = {0.5f, 0.8f, 0.3f, 0.7f};  // Module 1 should win
    nimcp_player_id_t bidders[NUM_MODULES] = {1, 2, 3, 4};

    for (uint32_t i = 0; i < NUM_MODULES; i++) {
        nimcp_error_t err = nimcp_auction_bid(auction, bidders[i], bids[i], 0);
        EXPECT_EQ(err, NIMCP_SUCCESS) << "Failed to submit bid for module " << i;
    }

    // Verify bid count
    EXPECT_EQ(nimcp_auction_get_bid_count(auction), NUM_MODULES);

    // Verify highest bid tracking
    EXPECT_FLOAT_EQ(nimcp_auction_get_current_highest(auction), 0.8f);

    // Resolve auction
    nimcp_auction_result_t result;
    nimcp_error_t resolve_err = nimcp_auction_resolve(auction, &result);
    EXPECT_EQ(resolve_err, NIMCP_SUCCESS) << "Failed to resolve auction";

    // Verify winner is highest bidder (module 2 bid 0.8)
    EXPECT_EQ(result.winner_id, 2u);
    EXPECT_FLOAT_EQ(result.winning_bid, 0.8f);

    // Second-price: winner pays second-highest (0.7)
    EXPECT_FLOAT_EQ(result.payment, 0.7f);
    EXPECT_FLOAT_EQ(result.second_highest_bid, 0.7f);

    // Verify completion state
    EXPECT_EQ(result.final_state, NIMCP_AUCTION_STATE_COMPLETED);
}

/**
 * TEST: AuctionPipeline_VCGMechanism
 *
 * WHAT: Test VCG mechanism for multi-item auction
 * WHY:  Verify incentive-compatible multi-item allocation
 * EXPECT: Efficient allocation with externality-based payments
 */
TEST_F(GameTheoryPipelineE2E, AuctionPipeline_VCGMechanism) {
    // Create VCG auction for multiple items
    nimcp_auction_config_t config = nimcp_auction_default_config();
    config.type = NIMCP_AUCTION_VCG;
    config.num_items = 2;
    config.reserve_price = 0.05f;
    config.max_bidders = NUM_MODULES;

    auction = nimcp_auction_create(&config);
    ASSERT_NE(auction, nullptr) << "Failed to create VCG auction";

    // VCG is strategyproof
    EXPECT_TRUE(nimcp_auction_is_strategyproof(NIMCP_AUCTION_VCG));

    // Submit bids with private valuations (truthful bidding is optimal)
    struct {
        nimcp_player_id_t id;
        float bid;
        float valuation;
        uint32_t item;
    } vcg_bids[] = {
        {1, 0.9f, 0.9f, 0},  // Module 1 bids for item 0
        {2, 0.7f, 0.7f, 0},  // Module 2 bids for item 0
        {3, 0.8f, 0.8f, 1},  // Module 3 bids for item 1
        {4, 0.5f, 0.5f, 1}   // Module 4 bids for item 1
    };

    for (const auto& bid : vcg_bids) {
        nimcp_error_t err = nimcp_auction_bid_vcg(auction, bid.id, bid.bid,
                                                   bid.valuation, bid.item);
        EXPECT_EQ(err, NIMCP_SUCCESS) << "Failed to submit VCG bid";
    }

    // Resolve VCG auction
    nimcp_vcg_result_t vcg_result;
    nimcp_error_t err = nimcp_auction_resolve_vcg(auction, &vcg_result);
    EXPECT_EQ(err, NIMCP_SUCCESS) << "Failed to resolve VCG auction";

    // Item 0 winner should be module 1 (0.9 > 0.7)
    EXPECT_EQ(vcg_result.winners[0], 1u);
    // Item 1 winner should be module 3 (0.8 > 0.5)
    EXPECT_EQ(vcg_result.winners[1], 3u);

    // Verify efficient allocation
    EXPECT_TRUE(vcg_result.is_efficient);

    // Total welfare should be sum of winner valuations
    EXPECT_GT(vcg_result.total_welfare, 1.5f);
}

//=============================================================================
// 2. MULTI-ROUND AUCTION E2E TESTS
//=============================================================================

/**
 * TEST: MultiRoundAuction_CompetitionDynamics
 *
 * WHAT: Run multiple auction rounds simulating GW competition
 * WHY:  Verify auction mechanism works over time
 * EXPECT: Winners correctly determined each round
 */
TEST_F(GameTheoryPipelineE2E, MultiRoundAuction_CompetitionDynamics) {
    uint32_t wins[NUM_MODULES] = {0};
    float total_payments[NUM_MODULES] = {0.0f};

    for (uint32_t round = 0; round < NUM_AUCTION_ROUNDS; round++) {
        // Create fresh auction for each round
        nimcp_auction_config_t config = nimcp_auction_default_config();
        config.type = NIMCP_AUCTION_SECOND_PRICE;
        config.reserve_price = 0.1f;

        nimcp_auction_t round_auction = nimcp_auction_create(&config);
        ASSERT_NE(round_auction, nullptr) << "Failed to create auction round " << round;

        // Generate dynamic bids (vary based on round)
        for (uint32_t m = 0; m < NUM_MODULES; m++) {
            // Bid varies based on module and round (simulates changing salience)
            float bid = 0.3f + 0.4f * sinf((float)(round * NUM_MODULES + m) * 0.5f);
            bid = std::max(0.1f, std::min(1.0f, bid));  // Clamp to valid range

            nimcp_error_t err = nimcp_auction_bid(round_auction, m, bid, 0);
            EXPECT_EQ(err, NIMCP_SUCCESS);
        }

        // Resolve round
        nimcp_auction_result_t result;
        nimcp_error_t err = nimcp_auction_resolve(round_auction, &result);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        if (result.final_state == NIMCP_AUCTION_STATE_COMPLETED) {
            ASSERT_LT(result.winner_id, NUM_MODULES);
            wins[result.winner_id]++;
            total_payments[result.winner_id] += result.payment;
        }

        nimcp_auction_destroy(round_auction);
    }

    // Verify that wins are distributed (not all to one module)
    uint32_t modules_with_wins = 0;
    for (uint32_t m = 0; m < NUM_MODULES; m++) {
        if (wins[m] > 0) {
            modules_with_wins++;
        }
    }
    EXPECT_GE(modules_with_wins, 2u) << "Expected wins distributed across modules";

    // Print summary
    std::cout << "[GameTheory E2E] Multi-round auction results:\n";
    for (uint32_t m = 0; m < NUM_MODULES; m++) {
        std::cout << "  Module " << m << ": " << wins[m] << " wins, "
                  << total_payments[m] << " total payment\n";
    }
}

//=============================================================================
// 3. HEMISPHERIC BARGAINING E2E TESTS
//=============================================================================

/**
 * TEST: HemisphericBargaining_NashSolution
 *
 * WHAT: Test Nash bargaining solution for resource allocation
 * WHY:  Verify fair division between hemispheres
 * EXPECT: Pareto optimal, individually rational allocation
 */
TEST_F(GameTheoryPipelineE2E, HemisphericBargaining_NashSolution) {
    // Create bargaining context for 2 players (hemispheres)
    nimcp_bargaining_config_t config = nimcp_bargaining_default_config(2);
    config.type = NIMCP_BARGAINING_NASH;
    config.disagreement_payoffs[0] = 0.1f;  // Left hemisphere disagreement
    config.disagreement_payoffs[1] = 0.1f;  // Right hemisphere disagreement
    config.bargaining_powers[0] = 0.5f;      // Equal power
    config.bargaining_powers[1] = 0.5f;
    config.convergence_threshold = CONVERGENCE_EPSILON;

    bargaining = nimcp_bargaining_create(&config);
    ASSERT_NE(bargaining, nullptr) << "Failed to create bargaining context";

    // Set feasible set (Pareto frontier)
    nimcp_feasible_point_t feasible_points[] = {
        {{1.0f, 0.0f}},   // All to left
        {{0.8f, 0.4f}},   // Mostly left
        {{0.6f, 0.6f}},   // Equal (on frontier)
        {{0.4f, 0.8f}},   // Mostly right
        {{0.0f, 1.0f}}    // All to right
    };

    nimcp_error_t err = nimcp_bargaining_set_feasible_set(
        bargaining, feasible_points, 5);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Compute Nash bargaining solution
    nimcp_bargaining_outcome_t outcome;
    err = nimcp_bargaining_compute_nash_solution(bargaining, &outcome);
    EXPECT_EQ(err, NIMCP_SUCCESS) << "Failed to compute Nash solution";

    // Verify outcome properties
    EXPECT_EQ(outcome.state, NIMCP_BARGAINING_STATE_AGREED);
    EXPECT_TRUE(outcome.is_pareto_optimal) << "Nash solution should be Pareto optimal";
    EXPECT_TRUE(outcome.is_individually_rational) << "Nash solution should be IR";

    // With equal bargaining power and symmetric disagreement,
    // allocation should be approximately equal
    float allocation_diff = fabsf(outcome.allocations[0] - outcome.allocations[1]);
    EXPECT_LT(allocation_diff, 0.3f) << "With equal power, allocations should be similar";

    // Nash product should be positive
    EXPECT_GT(outcome.nash_product, 0.0f);

    std::cout << "[GameTheory E2E] Nash bargaining outcome:\n";
    std::cout << "  Left allocation: " << outcome.allocations[0] << "\n";
    std::cout << "  Right allocation: " << outcome.allocations[1] << "\n";
    std::cout << "  Nash product: " << outcome.nash_product << "\n";
}

/**
 * TEST: HemisphericBargaining_AsymmetricPower
 *
 * WHAT: Test bargaining with unequal bargaining power
 * WHY:  Verify power asymmetry affects allocation
 * EXPECT: Higher power hemisphere gets larger share
 */
TEST_F(GameTheoryPipelineE2E, HemisphericBargaining_AsymmetricPower) {
    // Create bargaining with asymmetric power
    nimcp_bargaining_config_t config = nimcp_bargaining_default_config(2);
    config.type = NIMCP_BARGAINING_NASH;
    config.disagreement_payoffs[0] = 0.1f;
    config.disagreement_payoffs[1] = 0.1f;
    config.bargaining_powers[0] = 0.7f;  // Left has more power
    config.bargaining_powers[1] = 0.3f;

    bargaining = nimcp_bargaining_create(&config);
    ASSERT_NE(bargaining, nullptr);

    // Set feasible set
    nimcp_feasible_point_t feasible_points[] = {
        {{1.0f, 0.0f}},
        {{0.7f, 0.5f}},
        {{0.5f, 0.7f}},
        {{0.0f, 1.0f}}
    };

    nimcp_error_t err = nimcp_bargaining_set_feasible_set(
        bargaining, feasible_points, 4);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    nimcp_bargaining_outcome_t outcome;
    err = nimcp_bargaining_compute_nash_solution(bargaining, &outcome);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Left (higher power) should get more
    EXPECT_GT(outcome.allocations[0], outcome.allocations[1])
        << "Higher power player should get larger allocation";
}

//=============================================================================
// 4. CREDIT ASSIGNMENT E2E TESTS
//=============================================================================

/**
 * TEST: CreditAssignment_ShapleyValue
 *
 * WHAT: Compute Shapley values for cognitive module contributions
 * WHY:  Verify fair credit attribution
 * EXPECT: Credits satisfy efficiency and symmetry
 */
TEST_F(GameTheoryPipelineE2E, CreditAssignment_ShapleyValue) {
    // Create credit system for 4 players
    nimcp_credit_config_t config = nimcp_credit_default_config(4);
    config.method = NIMCP_CREDIT_SHAPLEY;
    config.cache_coalitions = true;

    credit_system = nimcp_credit_create(&config);
    ASSERT_NE(credit_system, nullptr) << "Failed to create credit system";

    // Compute Shapley values
    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_compute_shapley(
        credit_system,
        test_coalition_value,
        nullptr,
        &result
    );
    EXPECT_EQ(err, NIMCP_SUCCESS) << "Failed to compute Shapley values";

    // Verify efficiency: sum of credits should equal grand coalition value
    float credit_sum = 0.0f;
    for (uint32_t i = 0; i < 4; i++) {
        credit_sum += result.credits[i];
    }

    float grand_coalition_value = result.total_value;
    float efficiency_error = fabsf(credit_sum - grand_coalition_value);
    EXPECT_LT(efficiency_error, 0.01f) << "Shapley values should sum to grand coalition value";

    // Verify symmetry: with symmetric value function, credits should be equal
    float max_credit = result.credits[0];
    float min_credit = result.credits[0];
    for (uint32_t i = 1; i < 4; i++) {
        max_credit = std::max(max_credit, result.credits[i]);
        min_credit = std::min(min_credit, result.credits[i]);
    }
    float symmetry_error = max_credit - min_credit;
    EXPECT_LT(symmetry_error, 0.01f) << "Symmetric players should have equal credits";

    std::cout << "[GameTheory E2E] Shapley credit assignment:\n";
    for (uint32_t i = 0; i < 4; i++) {
        std::cout << "  Player " << i << ": " << result.credits[i] << "\n";
    }
    std::cout << "  Grand coalition value: " << grand_coalition_value << "\n";
}

/**
 * TEST: CreditAssignment_CoreMembership
 *
 * WHAT: Verify Shapley allocation is in the core
 * WHY:  Core stability means no coalition wants to deviate
 * EXPECT: Shapley allocation satisfies core constraints
 */
TEST_F(GameTheoryPipelineE2E, CreditAssignment_CoreMembership) {
    // Create credit system
    nimcp_credit_config_t config = nimcp_credit_default_config(3);
    credit_system = nimcp_credit_create(&config);
    ASSERT_NE(credit_system, nullptr);

    // Compute Shapley values
    nimcp_credit_result_t result;
    nimcp_error_t err = nimcp_credit_compute_shapley(
        credit_system,
        test_coalition_value,
        nullptr,
        &result
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Check if allocation is in the core
    bool in_core = nimcp_credit_is_in_core(
        credit_system,
        result.credits,
        test_coalition_value,
        nullptr
    );

    // For superadditive games with our value function, Shapley is in core
    EXPECT_TRUE(in_core) << "Shapley allocation should be in the core";
}

//=============================================================================
// 5. FULL CYCLE INTEGRATION E2E TESTS
//=============================================================================

/**
 * TEST: FullCycle_AuctionBroadcastCredit
 *
 * WHAT: Complete cycle: modules bid -> auction -> broadcast -> credit
 * WHY:  Verify full game theory pipeline integration
 * EXPECT: All stages work together correctly
 */
TEST_F(GameTheoryPipelineE2E, FullCycle_AuctionBroadcastCredit) {
    // Phase 1: Create Global Workspace
    global_workspace_config_t ws_config = global_workspace_default_config();
    ws_config.capacity_dim = CONTENT_DIM;
    ws_config.ignition_threshold = 0.5f;
    workspace = global_workspace_create_custom(&ws_config);
    ASSERT_NE(workspace, nullptr) << "Failed to create workspace";

    // Subscribe modules to workspace
    for (uint32_t m = 0; m < NUM_MODULES; m++) {
        cognitive_module_t mod = static_cast<cognitive_module_t>(MODULE_WORKING_MEMORY + m);
        bool subscribed = global_workspace_subscribe(workspace, mod);
        EXPECT_TRUE(subscribed) << "Failed to subscribe module " << m;
    }

    // Phase 2: Create auction context for workspace
    gt_gw_config_t gw_config = gt_gw_default_config();
    gw_config.strategy = GT_GW_STRATEGY_SECOND_PRICE;
    gw_config.reserve_price = 0.3f;
    gw_config.enable_budget_constraints = true;
    gw_config.initial_budget = 10.0f;

    gw_auction_ctx = gt_gw_create(workspace, &gw_config);
    ASSERT_NE(gw_auction_ctx, nullptr) << "Failed to create GW auction context";
    EXPECT_TRUE(gt_gw_is_auction_active(gw_auction_ctx));

    // Phase 3: Run competition rounds
    uint32_t broadcast_count = 0;
    float content[CONTENT_DIM];

    for (uint32_t round = 0; round < 5; round++) {
        // Each module submits content with varying salience
        cognitive_module_t winner = MODULE_NONE;

        for (uint32_t m = 0; m < NUM_MODULES; m++) {
            cognitive_module_t mod = static_cast<cognitive_module_t>(MODULE_WORKING_MEMORY + m);
            generate_content(content, CONTENT_DIM, (float)(round * NUM_MODULES + m));

            // Bid varies by module and round
            float bid = 0.4f + 0.3f * sinf((float)(round + m) * 0.7f);

            nimcp_error_t err = gt_gw_bid(gw_auction_ctx, mod, content, CONTENT_DIM, bid);
            EXPECT_EQ(err, NIMCP_SUCCESS) << "Failed to submit bid round " << round;
        }

        // Resolve round
        gt_gw_round_result_t round_result;
        nimcp_error_t err = gt_gw_resolve(gw_auction_ctx, &round_result);
        EXPECT_EQ(err, NIMCP_SUCCESS) << "Failed to resolve round " << round;

        if (round_result.reserve_met) {
            broadcast_count++;
            winner = round_result.winner;

            // Verify winner was determined correctly
            EXPECT_NE(winner, MODULE_NONE) << "Expected valid winner when reserve met";
            EXPECT_GT(round_result.winning_bid, 0.0f);
            EXPECT_GT(round_result.payment, 0.0f);
        }
    }

    EXPECT_GT(broadcast_count, 0u) << "Expected at least one successful broadcast";

    // Phase 4: Credit assignment for contributions
    nimcp_credit_config_t credit_config = nimcp_credit_default_config(NUM_MODULES);
    credit_system = nimcp_credit_create(&credit_config);
    ASSERT_NE(credit_system, nullptr);

    nimcp_credit_result_t credit_result;
    nimcp_error_t err = nimcp_credit_compute_shapley(
        credit_system,
        test_coalition_value,
        nullptr,
        &credit_result
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Verify all modules received some credit
    for (uint32_t m = 0; m < NUM_MODULES; m++) {
        EXPECT_GT(credit_result.credits[m], 0.0f)
            << "Module " << m << " should receive positive credit";
    }

    // Phase 5: Get and verify statistics
    nimcp_game_stats_t stats;
    err = gt_gw_get_stats(gw_auction_ctx, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(stats.auctions_completed, 0u);

    std::cout << "[GameTheory E2E] Full cycle results:\n";
    std::cout << "  Broadcasts: " << broadcast_count << "/5\n";
    std::cout << "  Auctions completed: " << stats.auctions_completed << "\n";
}

/**
 * TEST: FullCycle_HemisphericResourceAllocation
 *
 * WHAT: Complete hemispheric bargaining and credit cycle
 * WHY:  Verify hemispheric game theory integration
 * EXPECT: Resources allocated, credit assigned correctly
 */
TEST_F(GameTheoryPipelineE2E, FullCycle_HemisphericResourceAllocation) {
    // Phase 1: Create hemispheric brain
    hemispheric_brain_config_t hemi_config = hemispheric_brain_default_config();
    hemi_brain = hemispheric_brain_create(&hemi_config);
    ASSERT_NE(hemi_brain, nullptr) << "Failed to create hemispheric brain";

    // Phase 2: Create hemispheric bargaining context
    gt_hemi_config_t gt_config = gt_hemi_default_config();
    gt_config.left_bargaining_power = 0.55f;  // Slight left dominance
    gt_config.right_bargaining_power = 0.45f;
    gt_config.disagreement_left = 0.2f;
    gt_config.disagreement_right = 0.2f;
    gt_config.use_shapley_credit = true;
    gt_config.bargain_type = NIMCP_BARGAINING_NASH;

    hemi_bargain_ctx = gt_hemi_create(hemi_brain, &gt_config);
    ASSERT_NE(hemi_bargain_ctx, nullptr) << "Failed to create hemispheric bargaining context";
    EXPECT_TRUE(gt_hemi_is_active(hemi_bargain_ctx));

    // Phase 3: Negotiate resource allocation
    float total_resources = 100.0f;
    gt_hemi_outcome_t outcome;

    nimcp_error_t err = gt_hemi_negotiate_resources(
        hemi_bargain_ctx,
        total_resources,
        &outcome
    );
    EXPECT_EQ(err, NIMCP_SUCCESS) << "Failed to negotiate resources";

    // Verify negotiation succeeded
    EXPECT_TRUE(outcome.agreement_reached);

    // Left has more power, should get more resources
    EXPECT_GT(outcome.left_allocation, outcome.right_allocation)
        << "Left (higher power) should get more resources";

    // Total allocation should equal input resources
    float total_allocated = outcome.left_allocation + outcome.right_allocation;
    EXPECT_NEAR(total_allocated, total_resources, 1.0f);

    // Phase 4: Process input and compute credit
    float input[CONTENT_DIM];
    float output[CONTENT_DIM];
    generate_content(input, CONTENT_DIM, 42.0f);

    gt_hemi_outcome_t process_outcome;
    err = gt_hemi_process_bargaining(
        hemi_bargain_ctx,
        input, CONTENT_DIM,
        output, CONTENT_DIM,
        &process_outcome
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Phase 5: Compute credit for joint processing
    gt_hemi_credit_t credit;
    err = gt_hemi_compute_credit(
        hemi_bargain_ctx,
        output, CONTENT_DIM,
        &credit
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Verify credit properties
    EXPECT_GT(credit.left_credit, 0.0f);
    EXPECT_GT(credit.right_credit, 0.0f);
    EXPECT_GT(credit.total_value, 0.0f);

    // Check for cooperation synergy
    if (credit.is_superadditive) {
        EXPECT_GT(credit.synergy_bonus, 0.0f)
            << "Superadditive cooperation should have positive synergy";
    }

    // Phase 6: Verify statistics
    nimcp_game_stats_t stats;
    err = gt_hemi_get_stats(hemi_bargain_ctx, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(stats.bargaining_successes, 0u);

    std::cout << "[GameTheory E2E] Hemispheric bargaining results:\n";
    std::cout << "  Left allocation: " << outcome.left_allocation << "\n";
    std::cout << "  Right allocation: " << outcome.right_allocation << "\n";
    std::cout << "  Left credit: " << credit.left_credit << "\n";
    std::cout << "  Right credit: " << credit.right_credit << "\n";
    std::cout << "  Synergy bonus: " << credit.synergy_bonus << "\n";
}

//=============================================================================
// 6. STRESS AND EDGE CASE TESTS
//=============================================================================

/**
 * TEST: StressTest_RapidAuctionCycles
 *
 * WHAT: Rapid auction create/resolve/destroy cycles
 * WHY:  Verify stability under high load
 * EXPECT: No crashes or memory issues
 */
TEST_F(GameTheoryPipelineE2E, StressTest_RapidAuctionCycles) {
    const uint32_t NUM_CYCLES = 50;
    uint32_t successful_cycles = 0;

    for (uint32_t i = 0; i < NUM_CYCLES; i++) {
        nimcp_auction_config_t config = nimcp_auction_default_config();
        config.type = (i % 2 == 0) ? NIMCP_AUCTION_SECOND_PRICE : NIMCP_AUCTION_FIRST_PRICE;

        nimcp_auction_t cycle_auction = nimcp_auction_create(&config);
        if (!cycle_auction) continue;

        // Submit a few bids
        for (uint32_t m = 0; m < 3; m++) {
            float bid = 0.5f + 0.1f * m;
            nimcp_auction_bid(cycle_auction, m, bid, 0);
        }

        // Resolve
        nimcp_auction_result_t result;
        nimcp_error_t err = nimcp_auction_resolve(cycle_auction, &result);

        if (err == NIMCP_SUCCESS && result.final_state == NIMCP_AUCTION_STATE_COMPLETED) {
            successful_cycles++;
        }

        nimcp_auction_destroy(cycle_auction);
    }

    EXPECT_EQ(successful_cycles, NUM_CYCLES) << "All cycles should succeed";
}

/**
 * TEST: EdgeCase_ReservePriceNotMet
 *
 * WHAT: Test auction when no bids meet reserve price
 * WHY:  Verify graceful handling of no-winner case
 * EXPECT: Auction state indicates no winner
 */
TEST_F(GameTheoryPipelineE2E, EdgeCase_ReservePriceNotMet) {
    nimcp_auction_config_t config = nimcp_auction_default_config();
    config.type = NIMCP_AUCTION_SECOND_PRICE;
    config.reserve_price = 0.9f;  // High reserve

    auction = nimcp_auction_create(&config);
    ASSERT_NE(auction, nullptr);

    // Submit bids below reserve
    nimcp_auction_bid(auction, 1, 0.3f, 0);
    nimcp_auction_bid(auction, 2, 0.5f, 0);
    nimcp_auction_bid(auction, 3, 0.7f, 0);

    nimcp_auction_result_t result;
    nimcp_error_t err = nimcp_auction_resolve(auction, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Verify no winner when reserve not met
    EXPECT_EQ(result.final_state, NIMCP_AUCTION_STATE_NO_WINNER);
    EXPECT_EQ(result.winner_id, NIMCP_GT_INVALID_PLAYER);
}

/**
 * TEST: EdgeCase_SingleBidder
 *
 * WHAT: Test auction with only one bidder
 * WHY:  Verify single-bidder case handled correctly
 * EXPECT: Single bidder wins, pays reserve price
 */
TEST_F(GameTheoryPipelineE2E, EdgeCase_SingleBidder) {
    nimcp_auction_config_t config = nimcp_auction_default_config();
    config.type = NIMCP_AUCTION_SECOND_PRICE;
    config.reserve_price = 0.1f;

    auction = nimcp_auction_create(&config);
    ASSERT_NE(auction, nullptr);

    // Single bid above reserve
    nimcp_auction_bid(auction, 1, 0.8f, 0);

    nimcp_auction_result_t result;
    nimcp_error_t err = nimcp_auction_resolve(auction, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(result.winner_id, 1u);
    EXPECT_FLOAT_EQ(result.winning_bid, 0.8f);
    // In second-price with one bidder, payment should be reserve price
    EXPECT_FLOAT_EQ(result.payment, config.reserve_price);
}

//=============================================================================
// 7. UTILITY AND HELPER TESTS
//=============================================================================

/**
 * TEST: Utility_FairnessIndex
 *
 * WHAT: Test Jain's fairness index computation
 * WHY:  Verify fairness metric works correctly
 * EXPECT: Correct fairness values for known allocations
 */
TEST_F(GameTheoryPipelineE2E, Utility_FairnessIndex) {
    // Perfectly fair allocation (all equal)
    float equal_alloc[] = {0.25f, 0.25f, 0.25f, 0.25f};
    float fairness_equal = nimcp_compute_fairness_index(equal_alloc, 4);
    EXPECT_NEAR(fairness_equal, 1.0f, 0.001f) << "Equal allocation should have fairness 1.0";

    // Completely unfair (all to one)
    float unfair_alloc[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float fairness_unfair = nimcp_compute_fairness_index(unfair_alloc, 4);
    EXPECT_NEAR(fairness_unfair, 0.25f, 0.01f) << "Single-holder allocation should have fairness 1/n";

    // Moderate fairness
    float moderate_alloc[] = {0.4f, 0.3f, 0.2f, 0.1f};
    float fairness_moderate = nimcp_compute_fairness_index(moderate_alloc, 4);
    EXPECT_GT(fairness_moderate, 0.25f);
    EXPECT_LT(fairness_moderate, 1.0f);
}

/**
 * TEST: Utility_NashProduct
 *
 * WHAT: Test Nash product computation
 * WHY:  Verify Nash objective function
 * EXPECT: Correct Nash product values
 */
TEST_F(GameTheoryPipelineE2E, Utility_NashProduct) {
    float allocation[] = {0.6f, 0.4f};
    float disagreement[] = {0.1f, 0.1f};
    float powers[] = {0.5f, 0.5f};

    float nash_product = nimcp_compute_nash_product(
        allocation, disagreement, powers, 2);

    // Nash product = (0.6-0.1)^0.5 * (0.4-0.1)^0.5
    // = 0.5^0.5 * 0.3^0.5 = sqrt(0.5) * sqrt(0.3) = sqrt(0.15)
    float expected = sqrtf(0.5f) * sqrtf(0.3f);
    EXPECT_NEAR(nash_product, expected, 0.01f);
}

/**
 * TEST: Utility_TypeNames
 *
 * WHAT: Test type-to-string conversion functions
 * WHY:  Verify utility functions work
 * EXPECT: Non-null, meaningful strings
 */
TEST_F(GameTheoryPipelineE2E, Utility_TypeNames) {
    // Game type names
    EXPECT_NE(nimcp_game_type_name(NIMCP_GAME_AUCTION), nullptr);
    EXPECT_NE(nimcp_game_type_name(NIMCP_GAME_BARGAINING), nullptr);

    // Auction type names
    EXPECT_NE(nimcp_auction_type_name(NIMCP_AUCTION_SECOND_PRICE), nullptr);
    EXPECT_NE(nimcp_auction_type_name(NIMCP_AUCTION_VCG), nullptr);

    // Bargaining type names
    EXPECT_NE(nimcp_bargaining_type_name(NIMCP_BARGAINING_NASH), nullptr);
    EXPECT_NE(nimcp_bargaining_type_name(NIMCP_BARGAINING_KALAI_SMORODINSKY), nullptr);

    // Credit method names
    EXPECT_NE(nimcp_credit_method_name(NIMCP_CREDIT_SHAPLEY), nullptr);
    EXPECT_NE(nimcp_credit_method_name(NIMCP_CREDIT_BANZHAF), nullptr);
}
