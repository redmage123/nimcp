/**
 * @file test_game_theory_executive_bridge.cpp
 * @brief Unit tests for Game Theory-Executive Cognitive Hub Bridge module
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Comprehensive tests for Game Theory-Executive bidirectional integration
 * WHY:  Ensure strategic reasoning integrates correctly with executive function
 * HOW:  Test lifecycle, connection, strategy analysis, risk assessment, opponent modeling
 *
 * TEST COVERAGE:
 * - Bridge Creation/Destruction
 * - Hub Connection/Disconnection
 * - Configuration Validation
 * - Strategic Analysis Request
 * - Risk Assessment Flow
 * - Decision Notification
 * - Opponent Model Request
 * - Executive Override
 * - Statistics Tracking
 * - Thread Safety
 * - Null Handling
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <atomic>
#include <vector>
#include <cmath>

#include "cognitive/integration/nimcp_game_theory_executive_bridge.h"
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class GameTheoryExecutiveBridgeTest : public ::testing::Test {
protected:
    game_theory_executive_bridge_t* bridge = nullptr;
    game_theory_executive_config_t config;
    cognitive_integration_hub_t hub = nullptr;

    void SetUp() override {
        // Get default config
        int result = game_theory_executive_bridge_default_config(&config);
        ASSERT_EQ(result, 0) << "Default config should succeed";

        // Create bridge
        bridge = game_theory_executive_bridge_create(&config);

        // Create cognitive hub for connection tests
        cognitive_hub_config_t hub_config = cognitive_hub_default_config();
        hub = cognitive_hub_create(&hub_config);
    }

    void TearDown() override {
        if (bridge != nullptr) {
            game_theory_executive_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (hub != nullptr) {
            cognitive_hub_destroy(hub);
            hub = nullptr;
        }
    }
};

/* ============================================================================
 * Bridge Lifecycle Tests
 * ============================================================================ */

/**
 * Test: BridgeCreation
 * Verify bridge can be created and destroyed successfully
 */
TEST_F(GameTheoryExecutiveBridgeTest, BridgeCreation) {
    ASSERT_NE(bridge, nullptr) << "Bridge creation should succeed";

    // Verify not connected initially
    EXPECT_FALSE(game_theory_executive_bridge_is_connected(bridge))
        << "Bridge should not be connected initially";

    // Verify initial state is IDLE
    EXPECT_EQ(game_theory_executive_bridge_get_state(bridge), GT_EXEC_STATE_IDLE)
        << "Initial state should be IDLE";
}

/**
 * Test: BridgeCreationNullConfig
 * Verify bridge can be created with NULL config (uses defaults)
 */
TEST_F(GameTheoryExecutiveBridgeTest, BridgeCreationNullConfig) {
    game_theory_executive_bridge_t* br = game_theory_executive_bridge_create(nullptr);
    ASSERT_NE(br, nullptr) << "Bridge creation with NULL config should succeed";
    game_theory_executive_bridge_destroy(br);
}

/**
 * Test: BridgeDestruction
 * Verify bridge destruction is safe and handles NULL
 */
TEST_F(GameTheoryExecutiveBridgeTest, BridgeDestruction) {
    // Destroy the bridge created in SetUp
    game_theory_executive_bridge_destroy(bridge);
    bridge = nullptr;

    // Destroying NULL should be safe
    game_theory_executive_bridge_destroy(nullptr);
    SUCCEED() << "Destroying NULL bridge should be safe";
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

/**
 * Test: ConfigValidation
 * Verify configuration has expected values
 */
TEST_F(GameTheoryExecutiveBridgeTest, ConfigValidation) {
    game_theory_executive_config_t test_config;
    int result = game_theory_executive_bridge_default_config(&test_config);
    EXPECT_EQ(result, 0) << "Default config should succeed";

    // Verify module ID
    EXPECT_EQ(test_config.module_id, GT_EXEC_DEFAULT_MODULE_ID);

    // Verify weight factors sum to ~1.0
    float weight_sum = test_config.strategic_weight +
                       test_config.risk_assessment_weight +
                       test_config.decision_integration_weight;
    EXPECT_NEAR(weight_sum, 1.0f, 0.01f) << "Weights should sum to 1.0";

    // Verify auto-subscribe options
    EXPECT_TRUE(test_config.auto_subscribe_decision);
    EXPECT_TRUE(test_config.auto_subscribe_attention);

    // Verify query handler enabled
    EXPECT_TRUE(test_config.enable_query_handler);
}

/**
 * Test: DefaultConfig
 * Verify default_config handles NULL gracefully
 */
TEST_F(GameTheoryExecutiveBridgeTest, DefaultConfigNull) {
    int result = game_theory_executive_bridge_default_config(nullptr);
    EXPECT_EQ(result, -1) << "NULL config should fail";
}

/* ============================================================================
 * Hub Connection Tests
 * ============================================================================ */

/**
 * Test: RegisterWithHub
 * Verify bridge can connect to cognitive hub
 */
TEST_F(GameTheoryExecutiveBridgeTest, RegisterWithHub) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = game_theory_executive_bridge_connect(bridge, hub);
    EXPECT_EQ(result, 0) << "Connection should succeed";

    EXPECT_TRUE(game_theory_executive_bridge_is_connected(bridge))
        << "Bridge should be connected after connect()";
}

/**
 * Test: RegisterWithHubNullParams
 * Verify connection handles NULL parameters gracefully
 */
TEST_F(GameTheoryExecutiveBridgeTest, RegisterWithHubNullParams) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // NULL bridge
    int result = game_theory_executive_bridge_connect(nullptr, hub);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    // NULL hub
    result = game_theory_executive_bridge_connect(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL hub should fail";
}

/**
 * Test: RegisterWithHubDuplicate
 * Verify connecting when already connected is handled
 */
TEST_F(GameTheoryExecutiveBridgeTest, RegisterWithHubDuplicate) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // First connection
    int result = game_theory_executive_bridge_connect(bridge, hub);
    EXPECT_EQ(result, 0) << "First connection should succeed";

    // Second connection - should fail
    result = game_theory_executive_bridge_connect(bridge, hub);
    EXPECT_EQ(result, -1) << "Duplicate connection should fail";
}

/**
 * Test: UnregisterFromHub
 * Verify bridge can disconnect cleanly
 */
TEST_F(GameTheoryExecutiveBridgeTest, UnregisterFromHub) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Connect first
    int result = game_theory_executive_bridge_connect(bridge, hub);
    ASSERT_EQ(result, 0) << "Connection required for disconnect test";

    // Disconnect
    result = game_theory_executive_bridge_disconnect(bridge);
    EXPECT_EQ(result, 0) << "Disconnect should succeed";

    EXPECT_FALSE(game_theory_executive_bridge_is_connected(bridge))
        << "Bridge should not be connected after disconnect";
}

/**
 * Test: UnregisterFromHubNull
 * Verify disconnect handles NULL gracefully
 */
TEST_F(GameTheoryExecutiveBridgeTest, UnregisterFromHubNull) {
    int result = game_theory_executive_bridge_disconnect(nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";
}

/* ============================================================================
 * Strategic Analysis Tests
 * ============================================================================ */

/**
 * Test: StrategicAnalysisRequest
 * Verify strategic analysis can be performed
 */
TEST_F(GameTheoryExecutiveBridgeTest, StrategicAnalysisRequest) {
    ASSERT_NE(bridge, nullptr);

    // Create test utilities matrix (3 actions x 2 outcomes)
    float utilities[] = {
        0.8f, 0.2f,   // Action 0: high variance
        0.5f, 0.5f,   // Action 1: consistent
        0.3f, 0.9f    // Action 2: high but risky
    };

    int result = game_theory_executive_analyze_options(bridge, 3, utilities, 2);
    EXPECT_EQ(result, 0) << "Analyze options should succeed";

    // Get recommendation
    gt_strategic_recommendation_t recommendation;
    result = game_theory_executive_get_recommendation(bridge, &recommendation);
    EXPECT_EQ(result, 0) << "Get recommendation should succeed";

    // Verify recommendation has valid values
    EXPECT_GE(recommendation.expected_utility, 0.0f);
    EXPECT_LE(recommendation.expected_utility, 1.0f);
    EXPECT_GE(recommendation.confidence, 0.0f);
    EXPECT_LE(recommendation.confidence, 1.0f);
    EXPECT_LT(recommendation.action_index, 3u);

    // State should be awaiting decision
    EXPECT_EQ(game_theory_executive_bridge_get_state(bridge),
              GT_EXEC_STATE_AWAITING_DECISION);
}

/**
 * Test: StrategicAnalysisNullParams
 * Verify analysis handles NULL parameters
 */
TEST_F(GameTheoryExecutiveBridgeTest, StrategicAnalysisNullParams) {
    ASSERT_NE(bridge, nullptr);

    float utilities[] = {0.5f, 0.5f};

    // NULL bridge
    int result = game_theory_executive_analyze_options(nullptr, 1, utilities, 2);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    // NULL utilities
    result = game_theory_executive_analyze_options(bridge, 1, nullptr, 2);
    EXPECT_EQ(result, -1) << "NULL utilities should fail";

    // Zero actions
    result = game_theory_executive_analyze_options(bridge, 0, utilities, 2);
    EXPECT_EQ(result, -1) << "Zero actions should fail";

    // Zero outcomes
    result = game_theory_executive_analyze_options(bridge, 1, utilities, 0);
    EXPECT_EQ(result, -1) << "Zero outcomes should fail";
}

/**
 * Test: SituationAnalysis
 * Verify situation-based strategic analysis
 */
TEST_F(GameTheoryExecutiveBridgeTest, SituationAnalysis) {
    ASSERT_NE(bridge, nullptr);

    // Create test situation
    float utilities[] = {
        0.7f, 0.3f, 0.5f,
        0.4f, 0.6f, 0.5f
    };

    gt_exec_situation_t situation;
    memset(&situation, 0, sizeof(situation));
    situation.situation_id = 12345;
    situation.situation_type = 1;
    situation.num_actions = 2;
    situation.num_outcomes = 3;
    situation.utilities = utilities;
    situation.urgency = 0.7f;
    situation.deadline_ms = 0;
    situation.context = nullptr;

    gt_strategic_recommendation_t recommendation;
    int result = game_theory_executive_request_strategic_analysis(
        bridge, &situation, &recommendation);
    EXPECT_EQ(result, 0) << "Strategic analysis should succeed";

    // Verify recommendation
    EXPECT_LT(recommendation.action_index, 2u);
    EXPECT_GE(recommendation.confidence, 0.0f);
}

/* ============================================================================
 * Risk Assessment Tests
 * ============================================================================ */

/**
 * Test: RiskAssessmentFlow
 * Verify risk assessment works correctly
 */
TEST_F(GameTheoryExecutiveBridgeTest, RiskAssessmentFlow) {
    ASSERT_NE(bridge, nullptr);

    // First, analyze some options
    float utilities[] = {
        0.9f, 0.1f,   // High variance action
        0.5f, 0.5f    // Low variance action
    };
    game_theory_executive_analyze_options(bridge, 2, utilities, 2);

    // Request risk assessment for action 0
    gt_exec_risk_assessment_t assessment;
    int result = game_theory_executive_request_risk_assessment(
        bridge, 0, nullptr, &assessment);
    EXPECT_EQ(result, 0) << "Risk assessment should succeed";

    // Verify assessment has valid values
    EXPECT_EQ(assessment.action_id, 0u);
    EXPECT_GE(assessment.overall_risk, 0.0f);
    EXPECT_LE(assessment.overall_risk, 1.0f);
    EXPECT_GE(assessment.strategic_risk, 0.0f);
    EXPECT_LE(assessment.strategic_risk, 1.0f);
    EXPECT_GT(assessment.timestamp, 0u);

    // High variance action should have higher risk
    gt_exec_risk_assessment_t assessment2;
    result = game_theory_executive_request_risk_assessment(
        bridge, 1, nullptr, &assessment2);
    EXPECT_EQ(result, 0);

    // Action 0 (high variance) should have higher strategic risk
    EXPECT_GE(assessment.strategic_risk, assessment2.strategic_risk)
        << "High variance action should have higher risk";
}

/**
 * Test: RiskAssessmentNoAnalysis
 * Verify risk assessment with no prior analysis
 */
TEST_F(GameTheoryExecutiveBridgeTest, RiskAssessmentNoAnalysis) {
    ASSERT_NE(bridge, nullptr);

    gt_exec_risk_assessment_t assessment;
    int result = game_theory_executive_request_risk_assessment(
        bridge, 0, nullptr, &assessment);
    EXPECT_EQ(result, 0) << "Risk assessment should succeed even without analysis";

    // Should return moderate default risk
    EXPECT_NEAR(assessment.overall_risk, 0.5f, 0.1f);
}

/* ============================================================================
 * Decision Notification Tests
 * ============================================================================ */

/**
 * Test: DecisionNotification
 * Verify decision notification updates statistics
 */
TEST_F(GameTheoryExecutiveBridgeTest, DecisionNotification) {
    ASSERT_NE(bridge, nullptr);

    // Analyze and get recommendation first
    float utilities[] = {0.6f, 0.4f, 0.5f, 0.5f};
    game_theory_executive_analyze_options(bridge, 2, utilities, 2);

    gt_strategic_recommendation_t recommendation;
    game_theory_executive_get_recommendation(bridge, &recommendation);

    // Get stats before notification
    game_theory_executive_stats_t stats_before;
    game_theory_executive_bridge_get_stats(bridge, &stats_before);

    // Notify of decision (following recommendation)
    int result = game_theory_executive_notify_decision_made(
        bridge,
        1001,  // decision_id
        recommendation.recommendation_id,
        recommendation.action_index,
        true   // followed_recommendation
    );
    EXPECT_EQ(result, 0) << "Decision notification should succeed";

    // Get stats after notification
    game_theory_executive_stats_t stats_after;
    game_theory_executive_bridge_get_stats(bridge, &stats_after);

    EXPECT_GT(stats_after.decisions_received, stats_before.decisions_received)
        << "Decisions received should increment";
    EXPECT_GT(stats_after.recommendations_followed, stats_before.recommendations_followed)
        << "Recommendations followed should increment";
}

/**
 * Test: DecisionNotificationWithOutcome
 * Verify decision notification with outcome
 */
TEST_F(GameTheoryExecutiveBridgeTest, DecisionNotificationWithOutcome) {
    ASSERT_NE(bridge, nullptr);

    // Analyze and get recommendation
    float utilities[] = {0.7f, 0.3f};
    game_theory_executive_analyze_options(bridge, 1, utilities, 2);

    gt_strategic_recommendation_t recommendation;
    game_theory_executive_get_recommendation(bridge, &recommendation);

    // Create outcome
    gt_decision_outcome_t outcome;
    outcome.decision_id = 2001;
    outcome.recommendation_id = recommendation.recommendation_id;
    outcome.action_taken = recommendation.action_index;
    outcome.outcome_utility = 0.8f;
    outcome.followed_recommendation = true;

    int result = game_theory_executive_notify_outcome(bridge, &outcome);
    EXPECT_EQ(result, 0) << "Outcome notification should succeed";

    // State should transition to idle after updating
    EXPECT_EQ(game_theory_executive_bridge_get_state(bridge), GT_EXEC_STATE_IDLE);
}

/* ============================================================================
 * Opponent Model Tests
 * ============================================================================ */

/**
 * Test: OpponentModelRequest
 * Verify opponent model can be requested
 */
TEST_F(GameTheoryExecutiveBridgeTest, OpponentModelRequest) {
    ASSERT_NE(bridge, nullptr);

    gt_exec_opponent_model_t model;
    int result = game_theory_executive_request_opponent_model(
        bridge, 1, nullptr, &model);
    EXPECT_EQ(result, 0) << "Opponent model request should succeed";

    // Verify model has valid initial values
    EXPECT_EQ(model.opponent_id, 1u);
    EXPECT_NEAR(model.cooperation_tendency, 0.5f, 0.01f);
    EXPECT_NEAR(model.aggression_level, 0.5f, 0.01f);
    EXPECT_NEAR(model.predictability, 0.5f, 0.01f);
    EXPECT_GT(model.num_strategies, 0u);

    // Strategy probabilities should sum to 1.0
    float prob_sum = 0.0f;
    for (uint32_t i = 0; i < model.num_strategies; i++) {
        prob_sum += model.strategy_probs[i];
    }
    EXPECT_NEAR(prob_sum, 1.0f, 0.01f);
}

/**
 * Test: OpponentModelUpdate
 * Verify opponent model can be updated
 */
TEST_F(GameTheoryExecutiveBridgeTest, OpponentModelUpdate) {
    ASSERT_NE(bridge, nullptr);

    // Request initial model
    gt_exec_opponent_model_t model_before;
    game_theory_executive_request_opponent_model(bridge, 2, nullptr, &model_before);

    // Update with observation (strategy 0, positive outcome)
    int result = game_theory_executive_update_opponent_model(
        bridge, 2, 0, 0.8f);
    EXPECT_EQ(result, 0) << "Opponent model update should succeed";

    // Request updated model
    gt_exec_opponent_model_t model_after;
    game_theory_executive_request_opponent_model(bridge, 2, nullptr, &model_after);

    // Strategy 0 probability should have increased
    EXPECT_GT(model_after.strategy_probs[0], model_before.strategy_probs[0])
        << "Observed strategy probability should increase";

    // Cooperation tendency should increase with positive outcome
    EXPECT_GT(model_after.cooperation_tendency, model_before.cooperation_tendency)
        << "Cooperation should increase with positive outcome";

    // Interaction count should increment
    EXPECT_GT(model_after.interaction_count, model_before.interaction_count);
}

/**
 * Test: OpponentModelPersistence
 * Verify opponent model persists across requests
 */
TEST_F(GameTheoryExecutiveBridgeTest, OpponentModelPersistence) {
    ASSERT_NE(bridge, nullptr);

    // Create model for opponent 5
    gt_exec_opponent_model_t model1;
    game_theory_executive_request_opponent_model(bridge, 5, nullptr, &model1);

    // Update the model multiple times
    for (int i = 0; i < 5; i++) {
        game_theory_executive_update_opponent_model(bridge, 5, 0, 0.6f);
    }

    // Request again and verify updates persisted
    gt_exec_opponent_model_t model2;
    game_theory_executive_request_opponent_model(bridge, 5, nullptr, &model2);

    EXPECT_GT(model2.strategy_probs[0], model1.strategy_probs[0])
        << "Model updates should persist";
    EXPECT_EQ(model2.interaction_count, 5u)
        << "Interaction count should reflect all updates";
}

/* ============================================================================
 * Executive Override Tests
 * ============================================================================ */

/**
 * Test: ExecutiveOverride
 * Verify executive override is tracked
 */
TEST_F(GameTheoryExecutiveBridgeTest, ExecutiveOverride) {
    ASSERT_NE(bridge, nullptr);

    // Analyze and get recommendation
    float utilities[] = {0.9f, 0.1f, 0.2f, 0.8f};
    game_theory_executive_analyze_options(bridge, 2, utilities, 2);

    gt_strategic_recommendation_t recommendation;
    game_theory_executive_get_recommendation(bridge, &recommendation);

    // Get stats before
    game_theory_executive_stats_t stats_before;
    game_theory_executive_bridge_get_stats(bridge, &stats_before);

    // Notify of decision NOT following recommendation
    uint32_t different_action = (recommendation.action_index + 1) % 2;
    int result = game_theory_executive_notify_decision_made(
        bridge,
        3001,
        recommendation.recommendation_id,
        different_action,
        false  // did not follow recommendation
    );
    EXPECT_EQ(result, 0);

    // Get stats after
    game_theory_executive_stats_t stats_after;
    game_theory_executive_bridge_get_stats(bridge, &stats_after);

    EXPECT_GT(stats_after.executive_overrides, stats_before.executive_overrides)
        << "Executive overrides should increment";
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

/**
 * Test: StatisticsTracking
 * Verify statistics are tracked correctly
 */
TEST_F(GameTheoryExecutiveBridgeTest, StatisticsTracking) {
    ASSERT_NE(bridge, nullptr);

    // Perform some operations
    float utilities[] = {0.5f, 0.5f};
    game_theory_executive_analyze_options(bridge, 1, utilities, 2);

    gt_strategic_recommendation_t recommendation;
    game_theory_executive_get_recommendation(bridge, &recommendation);

    // Request risk assessment
    gt_exec_risk_assessment_t assessment;
    game_theory_executive_request_risk_assessment(bridge, 0, nullptr, &assessment);

    // Request opponent model
    gt_exec_opponent_model_t model;
    game_theory_executive_request_opponent_model(bridge, 1, nullptr, &model);

    // Get stats
    game_theory_executive_stats_t stats;
    int result = game_theory_executive_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0) << "Get stats should succeed";

    // Verify counters
    EXPECT_GE(stats.strategies_analyzed, 1u);
    EXPECT_GE(stats.recommendations_made, 1u);
    EXPECT_GE(stats.risk_assessments, 1u);
    EXPECT_GE(stats.opponent_model_requests, 1u);
}

/**
 * Test: StatisticsTrackingNull
 * Verify get_stats handles NULL parameters
 */
TEST_F(GameTheoryExecutiveBridgeTest, StatisticsTrackingNull) {
    game_theory_executive_stats_t stats;

    int result = game_theory_executive_bridge_get_stats(nullptr, &stats);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = game_theory_executive_bridge_get_stats(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL stats output should fail";
}

/**
 * Test: StatisticsReset
 * Verify statistics can be reset
 */
TEST_F(GameTheoryExecutiveBridgeTest, StatisticsReset) {
    ASSERT_NE(bridge, nullptr);

    // Generate some stats
    float utilities[] = {0.5f, 0.5f};
    game_theory_executive_analyze_options(bridge, 1, utilities, 2);

    gt_strategic_recommendation_t recommendation;
    game_theory_executive_get_recommendation(bridge, &recommendation);

    // Reset stats
    int result = game_theory_executive_bridge_reset_stats(bridge);
    EXPECT_EQ(result, 0) << "Reset stats should succeed";

    // Get stats after reset
    game_theory_executive_stats_t stats;
    game_theory_executive_bridge_get_stats(bridge, &stats);

    // All counters should be zero
    EXPECT_EQ(stats.strategies_analyzed, 0u);
    EXPECT_EQ(stats.recommendations_made, 0u);
    EXPECT_EQ(stats.decisions_received, 0u);
}

/* ============================================================================
 * Thread Safety Tests
 * ============================================================================ */

/**
 * Test: ThreadSafety
 * Basic test for concurrent access to bridge
 */
TEST_F(GameTheoryExecutiveBridgeTest, ThreadSafety) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Connect
    int result = game_theory_executive_bridge_connect(bridge, hub);
    ASSERT_EQ(result, 0);

    const int NUM_THREADS = 4;
    const int ITERATIONS = 50;
    std::atomic<int> completed{0};
    std::vector<std::thread> threads;

    // Create threads that concurrently access the bridge
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, &completed, ITERATIONS]() {
            for (int i = 0; i < ITERATIONS; i++) {
                // Read operations
                game_theory_executive_bridge_is_connected(bridge);
                game_theory_executive_bridge_get_state(bridge);
                game_theory_executive_bridge_get_pending_count(bridge);

                game_theory_executive_stats_t stats;
                game_theory_executive_bridge_get_stats(bridge, &stats);

                // Analysis operations
                float utilities[] = {0.5f, 0.5f};
                game_theory_executive_analyze_options(bridge, 1, utilities, 2);
            }
            completed++;
        });
    }

    // Wait for all threads to complete
    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(completed.load(), NUM_THREADS)
        << "All threads should complete successfully";
}

/* ============================================================================
 * Null Handling Tests
 * ============================================================================ */

/**
 * Test: NullHandling
 * Comprehensive test for NULL parameter handling across all functions
 */
TEST_F(GameTheoryExecutiveBridgeTest, NullHandling) {
    // Lifecycle
    game_theory_executive_bridge_destroy(nullptr);
    EXPECT_EQ(game_theory_executive_bridge_default_config(nullptr), -1);

    // Connection
    EXPECT_EQ(game_theory_executive_bridge_connect(nullptr, hub), -1);
    EXPECT_EQ(game_theory_executive_bridge_connect(bridge, nullptr), -1);
    EXPECT_EQ(game_theory_executive_bridge_disconnect(nullptr), -1);
    EXPECT_FALSE(game_theory_executive_bridge_is_connected(nullptr));

    // Analysis
    float utilities[] = {0.5f, 0.5f};
    EXPECT_EQ(game_theory_executive_analyze_options(nullptr, 1, utilities, 2), -1);
    EXPECT_EQ(game_theory_executive_analyze_options(bridge, 1, nullptr, 2), -1);

    gt_strategic_recommendation_t rec;
    EXPECT_EQ(game_theory_executive_get_recommendation(nullptr, &rec), -1);
    EXPECT_EQ(game_theory_executive_get_recommendation(bridge, nullptr), -1);

    // Risk assessment
    gt_exec_risk_assessment_t assessment;
    EXPECT_EQ(game_theory_executive_request_risk_assessment(nullptr, 0, nullptr, &assessment), -1);
    EXPECT_EQ(game_theory_executive_request_risk_assessment(bridge, 0, nullptr, nullptr), -1);

    // Decision notification
    gt_decision_outcome_t outcome;
    memset(&outcome, 0, sizeof(outcome));
    EXPECT_EQ(game_theory_executive_notify_outcome(nullptr, &outcome), -1);
    EXPECT_EQ(game_theory_executive_notify_outcome(bridge, nullptr), -1);

    // Opponent modeling
    gt_exec_opponent_model_t model;
    EXPECT_EQ(game_theory_executive_request_opponent_model(nullptr, 1, nullptr, &model), -1);
    EXPECT_EQ(game_theory_executive_request_opponent_model(bridge, 1, nullptr, nullptr), -1);
    EXPECT_EQ(game_theory_executive_update_opponent_model(nullptr, 1, 0, 0.5f), -1);

    // Query
    EXPECT_EQ(game_theory_executive_bridge_get_state(nullptr), GT_EXEC_STATE_ERROR);
    EXPECT_EQ(game_theory_executive_bridge_get_module_id(nullptr), 0u);
    EXPECT_EQ(game_theory_executive_bridge_get_pending_count(nullptr), 0u);

    // Stats
    game_theory_executive_stats_t stats;
    EXPECT_EQ(game_theory_executive_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(game_theory_executive_bridge_get_stats(bridge, nullptr), -1);
    EXPECT_EQ(game_theory_executive_bridge_reset_stats(nullptr), -1);

    SUCCEED() << "All NULL handling tests passed";
}

/* ============================================================================
 * Integration Flow Tests
 * ============================================================================ */

/**
 * Test: FullIntegrationFlow
 * Test complete flow: connect, analyze, recommend, notify, disconnect
 */
TEST_F(GameTheoryExecutiveBridgeTest, FullIntegrationFlow) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Connect
    int result = game_theory_executive_bridge_connect(bridge, hub);
    EXPECT_EQ(result, 0) << "Connect should succeed";
    EXPECT_TRUE(game_theory_executive_bridge_is_connected(bridge));

    // Analyze strategic options
    float utilities[] = {
        0.8f, 0.6f, 0.4f,
        0.5f, 0.5f, 0.5f,
        0.3f, 0.7f, 0.9f
    };
    result = game_theory_executive_analyze_options(bridge, 3, utilities, 3);
    EXPECT_EQ(result, 0) << "Analyze should succeed";

    // Get recommendation
    gt_strategic_recommendation_t recommendation;
    result = game_theory_executive_get_recommendation(bridge, &recommendation);
    EXPECT_EQ(result, 0) << "Get recommendation should succeed";

    // Request risk assessment
    gt_exec_risk_assessment_t assessment;
    result = game_theory_executive_request_risk_assessment(
        bridge, recommendation.action_index, nullptr, &assessment);
    EXPECT_EQ(result, 0) << "Risk assessment should succeed";

    // Publish recommendation
    result = game_theory_executive_publish_recommendation(bridge, &recommendation);
    EXPECT_EQ(result, 0) << "Publish should succeed";

    // Notify of decision
    result = game_theory_executive_notify_decision_made(
        bridge,
        4001,
        recommendation.recommendation_id,
        recommendation.action_index,
        true
    );
    EXPECT_EQ(result, 0) << "Decision notification should succeed";

    // Check stats
    game_theory_executive_stats_t stats;
    result = game_theory_executive_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.strategies_analyzed, 3u);
    EXPECT_GE(stats.recommendations_made, 1u);
    EXPECT_GE(stats.risk_assessments, 1u);

    // Disconnect
    result = game_theory_executive_bridge_disconnect(bridge);
    EXPECT_EQ(result, 0) << "Disconnect should succeed";
    EXPECT_FALSE(game_theory_executive_bridge_is_connected(bridge));
}

/**
 * Test: ReconnectAfterDisconnect
 * Verify bridge can reconnect after disconnect
 */
TEST_F(GameTheoryExecutiveBridgeTest, ReconnectAfterDisconnect) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Connect
    int result = game_theory_executive_bridge_connect(bridge, hub);
    EXPECT_EQ(result, 0);

    // Disconnect
    result = game_theory_executive_bridge_disconnect(bridge);
    EXPECT_EQ(result, 0);

    // Reconnect
    result = game_theory_executive_bridge_connect(bridge, hub);
    EXPECT_EQ(result, 0) << "Reconnect should succeed";
    EXPECT_TRUE(game_theory_executive_bridge_is_connected(bridge));
}

/* ============================================================================
 * Configuration Variants Tests
 * ============================================================================ */

/**
 * Test: ConfigVariants
 * Test bridge creation with various config options
 */
TEST_F(GameTheoryExecutiveBridgeTest, ConfigVariants) {
    // Config with logging disabled
    game_theory_executive_config_t config1;
    game_theory_executive_bridge_default_config(&config1);
    config1.enable_logging = true;
    config1.enable_learning = false;

    game_theory_executive_bridge_t* br1 = game_theory_executive_bridge_create(&config1);
    ASSERT_NE(br1, nullptr);
    game_theory_executive_bridge_destroy(br1);

    // Config with mixed strategies disabled
    game_theory_executive_config_t config2;
    game_theory_executive_bridge_default_config(&config2);
    config2.enable_mixed_strategies = false;

    game_theory_executive_bridge_t* br2 = game_theory_executive_bridge_create(&config2);
    ASSERT_NE(br2, nullptr);
    game_theory_executive_bridge_destroy(br2);

    // Config with custom weights
    game_theory_executive_config_t config3;
    game_theory_executive_bridge_default_config(&config3);
    config3.strategic_weight = 0.5f;
    config3.risk_assessment_weight = 0.3f;
    config3.decision_integration_weight = 0.2f;
    config3.risk_tolerance = 0.8f;

    game_theory_executive_bridge_t* br3 = game_theory_executive_bridge_create(&config3);
    ASSERT_NE(br3, nullptr);
    game_theory_executive_bridge_destroy(br3);

    SUCCEED() << "All config variants should work";
}

/**
 * Test: ForceUpdate
 * Verify force update resets stuck states
 */
TEST_F(GameTheoryExecutiveBridgeTest, ForceUpdate) {
    ASSERT_NE(bridge, nullptr);

    // Get recommendation to put bridge in AWAITING_DECISION state
    float utilities[] = {0.5f, 0.5f};
    game_theory_executive_analyze_options(bridge, 1, utilities, 2);

    gt_strategic_recommendation_t recommendation;
    game_theory_executive_get_recommendation(bridge, &recommendation);

    EXPECT_EQ(game_theory_executive_bridge_get_state(bridge),
              GT_EXEC_STATE_AWAITING_DECISION);

    // Force update should reset state
    int result = game_theory_executive_bridge_force_update(bridge);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(game_theory_executive_bridge_get_state(bridge),
              GT_EXEC_STATE_IDLE);
}
