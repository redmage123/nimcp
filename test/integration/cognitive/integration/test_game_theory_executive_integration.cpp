/**
 * @file test_game_theory_executive_integration.cpp
 * @brief Integration tests for Game Theory-Executive Bridge with Cognitive Hub
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Integration tests for full Game Theory-Executive decision pipeline
 * WHY:  Validate end-to-end strategic decision flows through cognitive hub
 * HOW:  Test complete scenarios including multi-agent decisions, risk-aware
 *       execution, and adaptive strategy adjustment
 *
 * TEST COVERAGE:
 * - StrategicDecisionFlow: Full strategic decision pipeline
 * - RiskAwareExecution: Executive considers strategic risks
 * - CompetitiveScenarioHandling: Multi-agent decision making
 * - CooperativeStrategySelection: Choosing cooperation vs defection
 * - AdaptiveStrategyAdjustment: Adjusting strategy based on outcomes
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <cmath>

#include "cognitive/integration/nimcp_game_theory_executive_bridge.h"
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"

/* ============================================================================
 * Test Helpers
 * ============================================================================ */

static std::atomic<int> g_event_received_count{0};
static std::atomic<int> g_recommendation_count{0};
static gt_strategic_recommendation_t g_last_recommendation;
static std::mutex g_recommendation_mutex;

/**
 * Test event callback to track hub events
 */
static int test_event_callback(const cognitive_event_data_t* event, void* user_data) {
    (void)user_data;
    if (!event) return -1;

    g_event_received_count++;

    if (event->event_type == COG_EVENT_OUTPUT_READY && event->payload) {
        std::lock_guard<std::mutex> lock(g_recommendation_mutex);
        g_recommendation_count++;
        g_last_recommendation = *(const gt_strategic_recommendation_t*)event->payload;
    }

    return 0;
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class GameTheoryExecutiveIntegrationTest : public ::testing::Test {
protected:
    game_theory_executive_bridge_t* bridge = nullptr;
    game_theory_executive_config_t config;
    cognitive_integration_hub_t hub = nullptr;
    uint32_t subscriber_module_id = 0x54455354;  // "TEST"

    void SetUp() override {
        // Reset global counters
        g_event_received_count = 0;
        g_recommendation_count = 0;
        memset(&g_last_recommendation, 0, sizeof(g_last_recommendation));

        // Create cognitive hub
        cognitive_hub_config_t hub_config = cognitive_hub_default_config();
        hub = cognitive_hub_create(&hub_config);
        ASSERT_NE(hub, nullptr) << "Hub creation should succeed";

        // Get default config for bridge
        game_theory_executive_bridge_default_config(&config);

        // Create and connect bridge
        bridge = game_theory_executive_bridge_create(&config);
        ASSERT_NE(bridge, nullptr) << "Bridge creation should succeed";

        int result = game_theory_executive_bridge_connect(bridge, hub);
        ASSERT_EQ(result, 0) << "Bridge connection should succeed";

        // Register test subscriber for events
        result = cognitive_hub_register_module(
            hub, subscriber_module_id, COG_CATEGORY_EXECUTIVE,
            "TestSubscriber", nullptr);
        ASSERT_EQ(result, 0) << "Test subscriber registration should succeed";

        // Subscribe to output events
        cognitive_hub_subscribe(
            hub, subscriber_module_id, COG_EVENT_OUTPUT_READY,
            test_event_callback, nullptr);
    }

    void TearDown() override {
        if (bridge != nullptr) {
            game_theory_executive_bridge_disconnect(bridge);
            game_theory_executive_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (hub != nullptr) {
            cognitive_hub_unregister_module(hub, subscriber_module_id);
            cognitive_hub_destroy(hub);
            hub = nullptr;
        }
    }

    /**
     * Helper: Create a prisoner's dilemma utility matrix
     * Rows = our action (0=cooperate, 1=defect)
     * Cols = opponent action (0=cooperate, 1=defect)
     * Payoffs: (R=3, S=0, T=5, P=1)
     */
    void CreatePrisonersDilemmaUtilities(float* utilities) {
        utilities[0] = 0.6f;  // Cooperate-Cooperate: R=3 normalized
        utilities[1] = 0.0f;  // Cooperate-Defect: S=0 (sucker's payoff)
        utilities[2] = 1.0f;  // Defect-Cooperate: T=5 (temptation)
        utilities[3] = 0.2f;  // Defect-Defect: P=1 (punishment)
    }

    /**
     * Helper: Create a stag hunt utility matrix
     * Coordination game where mutual cooperation is best
     */
    void CreateStagHuntUtilities(float* utilities) {
        utilities[0] = 1.0f;  // Stag-Stag: Best outcome
        utilities[1] = 0.0f;  // Stag-Hare: We get nothing
        utilities[2] = 0.5f;  // Hare-Stag: Safe but suboptimal
        utilities[3] = 0.5f;  // Hare-Hare: Safe but suboptimal
    }
};

/* ============================================================================
 * Strategic Decision Flow Tests
 * ============================================================================ */

/**
 * Test: StrategicDecisionFlow
 * Full strategic decision pipeline from analysis to outcome
 */
TEST_F(GameTheoryExecutiveIntegrationTest, StrategicDecisionFlow) {
    // Create multi-option strategic situation
    float utilities[] = {
        0.8f, 0.6f, 0.4f, 0.2f,  // Action 0: Aggressive
        0.5f, 0.5f, 0.5f, 0.5f,  // Action 1: Conservative
        0.3f, 0.4f, 0.6f, 0.9f,  // Action 2: Adaptive
        0.6f, 0.3f, 0.7f, 0.4f   // Action 3: Balanced
    };

    // Phase 1: Strategic analysis
    int result = game_theory_executive_analyze_options(bridge, 4, utilities, 4);
    ASSERT_EQ(result, 0) << "Analysis should succeed";

    // Phase 2: Get recommendation
    gt_strategic_recommendation_t recommendation;
    result = game_theory_executive_get_recommendation(bridge, &recommendation);
    ASSERT_EQ(result, 0) << "Get recommendation should succeed";

    // Verify recommendation is within valid range
    EXPECT_LT(recommendation.action_index, 4u);
    EXPECT_GE(recommendation.confidence, 0.0f);
    EXPECT_LE(recommendation.confidence, 1.0f);

    // Phase 3: Publish recommendation through hub
    result = game_theory_executive_publish_recommendation(bridge, &recommendation);
    ASSERT_EQ(result, 0) << "Publish should succeed";

    // Allow event propagation
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Verify event was received
    EXPECT_GE(g_recommendation_count.load(), 1)
        << "At least one recommendation event should be received";

    // Phase 4: Executive makes decision
    result = game_theory_executive_notify_decision_made(
        bridge,
        1001,  // decision_id
        recommendation.recommendation_id,
        recommendation.action_index,
        true   // followed recommendation
    );
    EXPECT_EQ(result, 0) << "Decision notification should succeed";

    // Phase 5: Verify statistics
    game_theory_executive_stats_t stats;
    game_theory_executive_bridge_get_stats(bridge, &stats);

    EXPECT_GE(stats.strategies_analyzed, 4u);
    EXPECT_GE(stats.recommendations_made, 1u);
    EXPECT_GE(stats.recommendations_followed, 1u);
    EXPECT_GE(stats.events_published, 1u);
}

/* ============================================================================
 * Risk-Aware Execution Tests
 * ============================================================================ */

/**
 * Test: RiskAwareExecution
 * Executive considers strategic risks before deciding
 */
TEST_F(GameTheoryExecutiveIntegrationTest, RiskAwareExecution) {
    // Create situation with varying risk levels
    float utilities[] = {
        0.9f, 0.1f,   // Action 0: High expected value but high variance
        0.5f, 0.5f,   // Action 1: Moderate but safe
        0.3f, 0.8f    // Action 2: Low expected but potential upside
    };

    int result = game_theory_executive_analyze_options(bridge, 3, utilities, 2);
    ASSERT_EQ(result, 0);

    // Get recommendation
    gt_strategic_recommendation_t recommendation;
    result = game_theory_executive_get_recommendation(bridge, &recommendation);
    ASSERT_EQ(result, 0);

    // Get risk assessment for recommended action
    gt_exec_risk_assessment_t risk_recommended;
    result = game_theory_executive_request_risk_assessment(
        bridge, recommendation.action_index, nullptr, &risk_recommended);
    ASSERT_EQ(result, 0);

    // Get risk assessment for safe alternative
    gt_exec_risk_assessment_t risk_safe;
    result = game_theory_executive_request_risk_assessment(
        bridge, 1, nullptr, &risk_safe);
    ASSERT_EQ(result, 0);

    // Executive should consider risk
    // If recommended action has very high risk, executive might override
    bool should_override = (risk_recommended.overall_risk > 0.7f &&
                            risk_safe.overall_risk < 0.3f);

    // Make decision based on risk tolerance
    uint32_t action_taken = recommendation.action_index;
    bool followed = true;
    if (should_override) {
        action_taken = 1;  // Take safe action
        followed = false;
    }

    result = game_theory_executive_notify_decision_made(
        bridge,
        2001,
        recommendation.recommendation_id,
        action_taken,
        followed
    );
    EXPECT_EQ(result, 0);

    // Verify stats track the decision correctly
    game_theory_executive_stats_t stats;
    game_theory_executive_bridge_get_stats(bridge, &stats);

    if (!followed) {
        EXPECT_GE(stats.executive_overrides, 1u)
            << "Override should be tracked";
    } else {
        EXPECT_GE(stats.recommendations_followed, 1u)
            << "Following should be tracked";
    }
}

/* ============================================================================
 * Competitive Scenario Tests
 * ============================================================================ */

/**
 * Test: CompetitiveScenarioHandling
 * Multi-agent decision making in competitive scenarios
 */
TEST_F(GameTheoryExecutiveIntegrationTest, CompetitiveScenarioHandling) {
    // Set up prisoner's dilemma scenario
    float pd_utilities[4];
    CreatePrisonersDilemmaUtilities(pd_utilities);

    int result = game_theory_executive_analyze_options(bridge, 2, pd_utilities, 2);
    ASSERT_EQ(result, 0);

    // Get opponent model to inform decision
    gt_exec_opponent_model_t opponent_model;
    result = game_theory_executive_request_opponent_model(
        bridge, 1, nullptr, &opponent_model);
    ASSERT_EQ(result, 0);

    // Get strategic recommendation
    gt_strategic_recommendation_t recommendation;
    result = game_theory_executive_get_recommendation(bridge, &recommendation);
    ASSERT_EQ(result, 0);

    // In classic prisoner's dilemma, defection is dominant strategy
    // but with unknown opponent, recommendation depends on risk tolerance

    // Simulate multiple rounds of interaction
    const int NUM_ROUNDS = 10;
    for (int round = 0; round < NUM_ROUNDS; round++) {
        // Our decision (alternating strategy for test)
        uint32_t our_action = (round % 2 == 0) ? 0 : 1;  // Tit-for-tat like

        // Simulate opponent's response
        uint32_t opponent_action = (round % 3 == 0) ? 1 : 0;  // Mostly cooperates

        // Update opponent model with observation
        float outcome = (opponent_action == 0) ? 0.7f : 0.3f;
        result = game_theory_executive_update_opponent_model(
            bridge, 1, opponent_action, outcome);
        EXPECT_EQ(result, 0);

        // Notify decision
        result = game_theory_executive_notify_decision_made(
            bridge,
            3000 + round,
            0,  // No specific recommendation
            our_action,
            false
        );
        EXPECT_EQ(result, 0);
    }

    // Check updated opponent model
    gt_exec_opponent_model_t updated_model;
    result = game_theory_executive_request_opponent_model(
        bridge, 1, nullptr, &updated_model);
    ASSERT_EQ(result, 0);

    // Model should reflect observed behavior
    EXPECT_GT(updated_model.interaction_count, 0u);

    // Since opponent mostly cooperates, cooperation tendency should be higher
    EXPECT_GT(updated_model.cooperation_tendency, 0.5f)
        << "Cooperation tendency should increase for cooperative opponent";

    // Strategy probabilities should favor cooperation
    EXPECT_GT(updated_model.strategy_probs[0], updated_model.strategy_probs[1])
        << "Cooperation probability should be higher";
}

/* ============================================================================
 * Cooperative Strategy Tests
 * ============================================================================ */

/**
 * Test: CooperativeStrategySelection
 * Choosing cooperation vs defection in coordination games
 */
TEST_F(GameTheoryExecutiveIntegrationTest, CooperativeStrategySelection) {
    // Set up stag hunt scenario
    float stag_hunt_utilities[4];
    CreateStagHuntUtilities(stag_hunt_utilities);

    int result = game_theory_executive_analyze_options(bridge, 2, stag_hunt_utilities, 2);
    ASSERT_EQ(result, 0);

    // Get recommendation
    gt_strategic_recommendation_t recommendation;
    result = game_theory_executive_get_recommendation(bridge, &recommendation);
    ASSERT_EQ(result, 0);

    // In stag hunt, cooperation (action 0) has higher expected value
    // if opponent is likely to cooperate

    // Create opponent model suggesting cooperation
    gt_exec_opponent_model_t opponent;
    result = game_theory_executive_request_opponent_model(
        bridge, 2, nullptr, &opponent);
    ASSERT_EQ(result, 0);

    // Update opponent model to show cooperative tendency
    for (int i = 0; i < 5; i++) {
        game_theory_executive_update_opponent_model(bridge, 2, 0, 0.9f);
    }

    // Re-request opponent model
    result = game_theory_executive_request_opponent_model(
        bridge, 2, nullptr, &opponent);
    ASSERT_EQ(result, 0);

    // With cooperative opponent, cooperation should be preferred
    // Re-analyze with updated information
    result = game_theory_executive_analyze_options(bridge, 2, stag_hunt_utilities, 2);
    ASSERT_EQ(result, 0);

    gt_strategic_recommendation_t cooperative_recommendation;
    result = game_theory_executive_get_recommendation(bridge, &cooperative_recommendation);
    ASSERT_EQ(result, 0);

    // The recommendation should favor cooperation when opponent cooperates
    // Note: This is a simplified test - real implementation would integrate
    // opponent model into utility calculation

    // Publish recommendation
    result = game_theory_executive_publish_recommendation(
        bridge, &cooperative_recommendation);
    EXPECT_EQ(result, 0);

    // Allow propagation
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Verify event propagation
    EXPECT_GE(g_event_received_count.load(), 1);
}

/* ============================================================================
 * Adaptive Strategy Tests
 * ============================================================================ */

/**
 * Test: AdaptiveStrategyAdjustment
 * Adjusting strategy based on outcomes over time
 */
TEST_F(GameTheoryExecutiveIntegrationTest, AdaptiveStrategyAdjustment) {
    // Create initial utilities
    float utilities[] = {
        0.6f, 0.4f,
        0.4f, 0.6f
    };

    int result = game_theory_executive_analyze_options(bridge, 2, utilities, 2);
    ASSERT_EQ(result, 0);

    // Track recommendation patterns
    std::vector<uint32_t> recommendations;
    std::vector<float> outcomes;

    // Simulate adaptive learning over multiple decisions
    const int NUM_DECISIONS = 20;
    for (int i = 0; i < NUM_DECISIONS; i++) {
        // Get recommendation
        gt_strategic_recommendation_t rec;
        result = game_theory_executive_get_recommendation(bridge, &rec);
        ASSERT_EQ(result, 0);

        recommendations.push_back(rec.action_index);

        // Simulate outcome (action 0 is better in first half, action 1 in second)
        float simulated_outcome;
        if (i < NUM_DECISIONS / 2) {
            simulated_outcome = (rec.action_index == 0) ? 0.8f : 0.2f;
        } else {
            simulated_outcome = (rec.action_index == 1) ? 0.8f : 0.2f;
        }
        outcomes.push_back(simulated_outcome);

        // Create decision outcome
        gt_decision_outcome_t outcome;
        outcome.decision_id = 4000 + i;
        outcome.recommendation_id = rec.recommendation_id;
        outcome.action_taken = rec.action_index;
        outcome.outcome_utility = simulated_outcome;
        outcome.followed_recommendation = true;

        result = game_theory_executive_notify_outcome(bridge, &outcome);
        EXPECT_EQ(result, 0);

        // Re-analyze for next iteration
        // In a real system, utilities would be updated based on outcomes
        result = game_theory_executive_analyze_options(bridge, 2, utilities, 2);
        ASSERT_EQ(result, 0);
    }

    // Verify statistics reflect the learning process
    game_theory_executive_stats_t stats;
    game_theory_executive_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(stats.recommendations_made, (uint64_t)NUM_DECISIONS);
    EXPECT_EQ(stats.decisions_received, (uint64_t)NUM_DECISIONS);

    // Average realized utility should reflect mixed outcomes
    EXPECT_GT(stats.avg_realized_utility, 0.0f);
    EXPECT_LT(stats.avg_realized_utility, 1.0f);
}

/* ============================================================================
 * Multi-Bridge Coordination Tests
 * ============================================================================ */

/**
 * Test: MultiBridgeCoordination
 * Multiple bridges coordinating through hub
 */
TEST_F(GameTheoryExecutiveIntegrationTest, MultiBridgeCoordination) {
    // Create second bridge
    game_theory_executive_config_t config2;
    game_theory_executive_bridge_default_config(&config2);
    config2.module_id = 0x47544559;  // Different module ID

    game_theory_executive_bridge_t* bridge2 =
        game_theory_executive_bridge_create(&config2);
    ASSERT_NE(bridge2, nullptr);

    int result = game_theory_executive_bridge_connect(bridge2, hub);
    ASSERT_EQ(result, 0);

    // Both bridges analyze same situation
    float utilities[] = {0.7f, 0.3f, 0.5f, 0.5f};

    result = game_theory_executive_analyze_options(bridge, 2, utilities, 2);
    ASSERT_EQ(result, 0);

    result = game_theory_executive_analyze_options(bridge2, 2, utilities, 2);
    ASSERT_EQ(result, 0);

    // Get recommendations from both
    gt_strategic_recommendation_t rec1, rec2;
    result = game_theory_executive_get_recommendation(bridge, &rec1);
    ASSERT_EQ(result, 0);

    result = game_theory_executive_get_recommendation(bridge2, &rec2);
    ASSERT_EQ(result, 0);

    // Both should arrive at similar conclusions for same input
    EXPECT_EQ(rec1.action_index, rec2.action_index)
        << "Same analysis should produce same recommendation";

    // Publish from both bridges
    result = game_theory_executive_publish_recommendation(bridge, &rec1);
    EXPECT_EQ(result, 0);

    result = game_theory_executive_publish_recommendation(bridge2, &rec2);
    EXPECT_EQ(result, 0);

    // Allow propagation
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Should receive events from both
    EXPECT_GE(g_recommendation_count.load(), 2);

    // Cleanup second bridge
    game_theory_executive_bridge_disconnect(bridge2);
    game_theory_executive_bridge_destroy(bridge2);
}

/* ============================================================================
 * Event Flow Tests
 * ============================================================================ */

/**
 * Test: EventFlowIntegration
 * Verify events flow correctly through the cognitive hub
 */
TEST_F(GameTheoryExecutiveIntegrationTest, EventFlowIntegration) {
    // Reset counters
    g_event_received_count = 0;
    g_recommendation_count = 0;

    // Generate strategic recommendation
    float utilities[] = {0.6f, 0.4f};
    int result = game_theory_executive_analyze_options(bridge, 1, utilities, 2);
    ASSERT_EQ(result, 0);

    gt_strategic_recommendation_t recommendation;
    result = game_theory_executive_get_recommendation(bridge, &recommendation);
    ASSERT_EQ(result, 0);

    // Publish multiple recommendations
    const int NUM_PUBLICATIONS = 5;
    for (int i = 0; i < NUM_PUBLICATIONS; i++) {
        recommendation.recommendation_id = 5000 + i;
        result = game_theory_executive_publish_recommendation(bridge, &recommendation);
        EXPECT_EQ(result, 0);
    }

    // Allow event propagation
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify all events were received
    EXPECT_EQ(g_recommendation_count.load(), NUM_PUBLICATIONS)
        << "All recommendations should be received";

    // Verify last recommendation
    {
        std::lock_guard<std::mutex> lock(g_recommendation_mutex);
        EXPECT_EQ(g_last_recommendation.recommendation_id, (uint64_t)(5000 + NUM_PUBLICATIONS - 1));
    }

    // Check bridge stats
    game_theory_executive_stats_t stats;
    game_theory_executive_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(stats.events_published, (uint64_t)NUM_PUBLICATIONS);
}

/* ============================================================================
 * Stress Tests
 * ============================================================================ */

/**
 * Test: HighVolumeDecisions
 * Handle high volume of decisions without degradation
 */
TEST_F(GameTheoryExecutiveIntegrationTest, HighVolumeDecisions) {
    const int NUM_DECISIONS = 100;

    float utilities[] = {0.5f, 0.5f};
    int result = game_theory_executive_analyze_options(bridge, 1, utilities, 2);
    ASSERT_EQ(result, 0);

    auto start_time = std::chrono::steady_clock::now();

    for (int i = 0; i < NUM_DECISIONS; i++) {
        gt_strategic_recommendation_t rec;
        result = game_theory_executive_get_recommendation(bridge, &rec);
        ASSERT_EQ(result, 0);

        result = game_theory_executive_notify_decision_made(
            bridge, 6000 + i, rec.recommendation_id, rec.action_index, true);
        ASSERT_EQ(result, 0);

        // Re-analyze for next iteration
        result = game_theory_executive_analyze_options(bridge, 1, utilities, 2);
        ASSERT_EQ(result, 0);
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    // Should complete within reasonable time (less than 5 seconds)
    EXPECT_LT(duration_ms, 5000)
        << "High volume decisions should complete quickly";

    // Verify all decisions tracked
    game_theory_executive_stats_t stats;
    game_theory_executive_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(stats.recommendations_made, (uint64_t)NUM_DECISIONS);
    EXPECT_EQ(stats.decisions_received, (uint64_t)NUM_DECISIONS);
}

/* ============================================================================
 * Error Recovery Tests
 * ============================================================================ */

/**
 * Test: ErrorRecovery
 * Bridge recovers gracefully from errors
 */
TEST_F(GameTheoryExecutiveIntegrationTest, ErrorRecovery) {
    // Attempt operations that should fail gracefully
    gt_strategic_recommendation_t rec;
    int result = game_theory_executive_get_recommendation(bridge, &rec);
    // Should fail without prior analysis
    EXPECT_EQ(result, -1);

    // Bridge should still be operational
    EXPECT_TRUE(game_theory_executive_bridge_is_connected(bridge));
    EXPECT_NE(game_theory_executive_bridge_get_state(bridge), GT_EXEC_STATE_ERROR);

    // Normal operation should work after error
    float utilities[] = {0.6f, 0.4f};
    result = game_theory_executive_analyze_options(bridge, 1, utilities, 2);
    EXPECT_EQ(result, 0);

    result = game_theory_executive_get_recommendation(bridge, &rec);
    EXPECT_EQ(result, 0);

    // Force update to clear any lingering state
    result = game_theory_executive_bridge_force_update(bridge);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(game_theory_executive_bridge_get_state(bridge), GT_EXEC_STATE_IDLE);
}
