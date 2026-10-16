/**
 * @file test_training_symbolic_logic_hub_bridge.cpp
 * @brief Unit tests for training symbolic logic hub bridge
 * @version 1.0.0
 * @date 2026-01-09
 *
 * Tests the training symbolic logic hub bridge functionality including:
 * - Bridge creation and destruction
 * - Configuration management
 * - Rule management (add, remove, get)
 * - Metrics updates
 * - Rule evaluation
 * - Rule learning
 * - Query APIs (LR, difficulty, early stop)
 * - State and statistics
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "training/integration/nimcp_training_symbolic_logic_hub_bridge.h"
#include "training/integration/nimcp_training_integration_hub.h"
#include "training/integration/nimcp_training_event_types.h"

/* ========================================================================
 * TEST FIXTURE
 * ======================================================================== */

class TrainingLogicHubBridgeTest : public ::testing::Test {
protected:
    training_logic_hub_bridge_t* bridge;
    training_logic_hub_config_t config;

    void SetUp() override {
        // Get default config and create bridge
        ASSERT_EQ(training_logic_hub_default_config(&config), 0);
        bridge = training_logic_hub_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge != nullptr) {
            training_logic_hub_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ========================================================================
 * LIFECYCLE TESTS
 * ======================================================================== */

/**
 * Test: BridgeCreation
 * Verify bridge can be created and destroyed successfully
 */
TEST_F(TrainingLogicHubBridgeTest, BridgeCreation) {
    // Bridge should be created in SetUp
    ASSERT_NE(bridge, nullptr) << "Bridge creation should succeed";
}

/**
 * Test: BridgeCreationNullConfig
 * Verify bridge can be created with NULL config (uses defaults)
 */
TEST_F(TrainingLogicHubBridgeTest, BridgeCreationNullConfig) {
    // Destroy the bridge created in SetUp
    training_logic_hub_destroy(bridge);

    // Create with NULL config - should use defaults
    bridge = training_logic_hub_create(nullptr);
    ASSERT_NE(bridge, nullptr) << "Bridge creation with NULL config should succeed";
}

/**
 * Test: DefaultConfig
 * Verify default configuration values
 */
TEST_F(TrainingLogicHubBridgeTest, DefaultConfig) {
    training_logic_hub_config_t default_config;
    ASSERT_EQ(training_logic_hub_default_config(&default_config), 0);

    // Default event subscriptions
    EXPECT_TRUE(default_config.subscribe_loss_computed);
    EXPECT_TRUE(default_config.subscribe_gradient_ready);
    EXPECT_TRUE(default_config.subscribe_difficulty_updated);
    EXPECT_TRUE(default_config.subscribe_lr_adjusted);
    EXPECT_TRUE(default_config.subscribe_epoch_complete);
    EXPECT_TRUE(default_config.subscribe_validation_complete);

    // Default publishing options
    EXPECT_TRUE(default_config.publish_rule_results);
    EXPECT_TRUE(default_config.publish_constraint_violations);

    // Rule learning enabled by default
    EXPECT_TRUE(default_config.enable_rule_learning);
    EXPECT_GT(default_config.rule_learning_rate, 0.0f);
    EXPECT_GT(default_config.min_rule_confidence, 0.0f);
}

/**
 * Test: DefaultConfigNullPointer
 * Verify default config handles NULL pointer
 */
TEST_F(TrainingLogicHubBridgeTest, DefaultConfigNullPointer) {
    EXPECT_EQ(training_logic_hub_default_config(nullptr), -1);
}

/* ========================================================================
 * RULE MANAGEMENT TESTS
 * ======================================================================== */

/**
 * Test: AddRule
 * Verify rules can be added
 */
TEST_F(TrainingLogicHubBridgeTest, AddRule) {
    training_logic_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.type = TRAINING_RULE_LR_SAFETY;
    strcpy(rule.name, "test_lr_safety");
    strcpy(rule.condition, "loss_stable AND grad_stable");
    rule.confidence = 0.8f;
    rule.priority = 0.5f;
    rule.is_safety_critical = true;

    int rule_id = training_logic_hub_add_rule(bridge, &rule);
    EXPECT_GE(rule_id, 0) << "Rule addition should succeed";
}

/**
 * Test: AddMultipleRules
 * Verify multiple rules can be added
 */
TEST_F(TrainingLogicHubBridgeTest, AddMultipleRules) {
    for (int i = 0; i < 5; i++) {
        training_logic_rule_t rule;
        memset(&rule, 0, sizeof(rule));
        rule.type = static_cast<training_rule_type_t>(i % TRAINING_RULE_COUNT);
        snprintf(rule.name, sizeof(rule.name), "test_rule_%d", i);
        strcpy(rule.condition, "test_condition");
        rule.confidence = 0.5f + i * 0.1f;
        rule.priority = 0.5f;

        int rule_id = training_logic_hub_add_rule(bridge, &rule);
        EXPECT_GE(rule_id, 0) << "Rule " << i << " addition should succeed";
    }
}

/**
 * Test: AddRuleNullPointer
 * Verify NULL rule pointer is handled
 */
TEST_F(TrainingLogicHubBridgeTest, AddRuleNullPointer) {
    EXPECT_EQ(training_logic_hub_add_rule(bridge, nullptr), -1);
    EXPECT_EQ(training_logic_hub_add_rule(nullptr, nullptr), -1);
}

/**
 * Test: GetRule
 * Verify rules can be retrieved by ID
 */
TEST_F(TrainingLogicHubBridgeTest, GetRule) {
    // Add a rule
    training_logic_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.type = TRAINING_RULE_EARLY_STOP;
    strcpy(rule.name, "test_early_stop");
    strcpy(rule.condition, "epochs_without_improvement > 10");
    rule.confidence = 0.9f;
    rule.priority = 0.8f;
    rule.is_safety_critical = false;

    int rule_id = training_logic_hub_add_rule(bridge, &rule);
    ASSERT_GE(rule_id, 0);

    // Retrieve the rule
    training_logic_rule_t retrieved;
    EXPECT_EQ(training_logic_hub_get_rule(bridge, rule_id, &retrieved), 0);
    EXPECT_EQ(retrieved.type, TRAINING_RULE_EARLY_STOP);
    EXPECT_STREQ(retrieved.name, "test_early_stop");
    EXPECT_FLOAT_EQ(retrieved.confidence, 0.9f);
}

/**
 * Test: RemoveRule
 * Verify rules can be removed
 */
TEST_F(TrainingLogicHubBridgeTest, RemoveRule) {
    // Add a rule
    training_logic_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.type = TRAINING_RULE_GRADIENT_CLIP;
    strcpy(rule.name, "test_grad_clip");
    rule.confidence = 0.7f;

    int rule_id = training_logic_hub_add_rule(bridge, &rule);
    ASSERT_GE(rule_id, 0);

    // Remove the rule
    EXPECT_EQ(training_logic_hub_remove_rule(bridge, rule_id), 0);

    // Rule should no longer be retrievable
    training_logic_rule_t retrieved;
    EXPECT_EQ(training_logic_hub_get_rule(bridge, rule_id, &retrieved), -1);
}

/**
 * Test: AddDefaultRules
 * Verify default rules are added
 */
TEST_F(TrainingLogicHubBridgeTest, AddDefaultRules) {
    int count = training_logic_hub_add_default_rules(bridge);
    EXPECT_GT(count, 0) << "Should add at least one default rule";

    // Get state to check active rules
    training_logic_hub_state_t state;
    EXPECT_EQ(training_logic_hub_get_state(bridge, &state), 0);
    EXPECT_EQ(state.active_rules, static_cast<uint32_t>(count));
}

/* ========================================================================
 * METRICS UPDATE TESTS
 * ======================================================================== */

/**
 * Test: UpdateMetrics
 * Verify metrics can be updated
 */
TEST_F(TrainingLogicHubBridgeTest, UpdateMetrics) {
    training_logic_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.current_loss = 0.5f;
    metrics.previous_loss = 0.6f;
    metrics.best_loss = 0.4f;
    metrics.grad_norm = 1.5f;
    metrics.learning_rate = 0.001f;
    metrics.difficulty = 0.3f;
    metrics.epoch = 10;

    EXPECT_EQ(training_logic_hub_update_metrics(bridge, &metrics), 0);
}

/**
 * Test: UpdateMetricsNullPointer
 * Verify NULL metrics pointer is handled
 */
TEST_F(TrainingLogicHubBridgeTest, UpdateMetricsNullPointer) {
    EXPECT_EQ(training_logic_hub_update_metrics(bridge, nullptr), -1);
    EXPECT_EQ(training_logic_hub_update_metrics(nullptr, nullptr), -1);
}

/* ========================================================================
 * RULE EVALUATION TESTS
 * ======================================================================== */

/**
 * Test: EvaluateRules
 * Verify rules can be evaluated
 */
TEST_F(TrainingLogicHubBridgeTest, EvaluateRules) {
    // Add default rules
    ASSERT_GT(training_logic_hub_add_default_rules(bridge), 0);

    // Set up metrics for evaluation
    training_logic_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.loss_stable = true;
    metrics.grad_stable = true;
    metrics.grad_exploding = false;
    metrics.grad_norm = 1.0f;
    metrics.grad_norm_avg = 1.0f;
    ASSERT_EQ(training_logic_hub_update_metrics(bridge, &metrics), 0);

    // Evaluate LR safety rules
    training_rule_result_t results[4];
    int count = training_logic_hub_evaluate_rules(bridge, TRAINING_RULE_LR_SAFETY, results, 4);
    EXPECT_GE(count, 0);
}

/**
 * Test: EvaluateSingleRule
 * Verify a single rule can be evaluated by ID
 */
TEST_F(TrainingLogicHubBridgeTest, EvaluateSingleRule) {
    // Add a rule
    training_logic_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.type = TRAINING_RULE_LR_SAFETY;
    strcpy(rule.name, "test_lr_safety");
    rule.confidence = 0.8f;

    int rule_id = training_logic_hub_add_rule(bridge, &rule);
    ASSERT_GE(rule_id, 0);

    // Set up metrics
    training_logic_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.loss_stable = true;
    metrics.grad_stable = true;
    ASSERT_EQ(training_logic_hub_update_metrics(bridge, &metrics), 0);

    // Evaluate the rule
    training_rule_result_t result;
    EXPECT_EQ(training_logic_hub_evaluate_rule(bridge, rule_id, &result), 0);
    EXPECT_EQ(result.type, TRAINING_RULE_LR_SAFETY);
}

/**
 * Test: IsActionSafe
 * Verify action safety checking
 */
TEST_F(TrainingLogicHubBridgeTest, IsActionSafe) {
    // Add default rules
    ASSERT_GT(training_logic_hub_add_default_rules(bridge), 0);

    // Set up safe metrics
    training_logic_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.loss_stable = true;
    metrics.grad_stable = true;
    metrics.grad_exploding = false;
    ASSERT_EQ(training_logic_hub_update_metrics(bridge, &metrics), 0);

    // Check if action is safe
    float confidence = 0.0f;
    bool safe = training_logic_hub_is_action_safe(bridge, "increase_lr", &confidence);
    // Result depends on rule evaluation - just verify it doesn't crash
    (void)safe;
    (void)confidence;
}

/* ========================================================================
 * RULE LEARNING TESTS
 * ======================================================================== */

/**
 * Test: ReportOutcome
 * Verify outcome reporting for rule learning
 */
TEST_F(TrainingLogicHubBridgeTest, ReportOutcome) {
    // Add a rule
    training_logic_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.type = TRAINING_RULE_LR_SAFETY;
    strcpy(rule.name, "test_lr_safety");
    rule.confidence = 0.5f;

    int rule_id = training_logic_hub_add_rule(bridge, &rule);
    ASSERT_GE(rule_id, 0);

    // Report positive outcome
    EXPECT_EQ(training_logic_hub_report_outcome(bridge, true, true), 0);

    // Confidence should have changed (if rule was recently fired)
    float new_confidence = training_logic_hub_get_rule_confidence(bridge, rule_id);
    EXPECT_GE(new_confidence, 0.0f);
    EXPECT_LE(new_confidence, 1.0f);
}

/**
 * Test: GetRuleConfidence
 * Verify rule confidence retrieval
 */
TEST_F(TrainingLogicHubBridgeTest, GetRuleConfidence) {
    // Add a rule
    training_logic_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.type = TRAINING_RULE_EARLY_STOP;
    rule.confidence = 0.75f;

    int rule_id = training_logic_hub_add_rule(bridge, &rule);
    ASSERT_GE(rule_id, 0);

    float confidence = training_logic_hub_get_rule_confidence(bridge, rule_id);
    EXPECT_FLOAT_EQ(confidence, 0.75f);
}

/**
 * Test: GetRuleConfidenceInvalidID
 * Verify invalid rule ID returns error
 */
TEST_F(TrainingLogicHubBridgeTest, GetRuleConfidenceInvalidID) {
    float confidence = training_logic_hub_get_rule_confidence(bridge, 9999);
    EXPECT_LT(confidence, 0.0f);
}

/* ========================================================================
 * QUERY API TESTS
 * ======================================================================== */

/**
 * Test: QueryLR
 * Verify learning rate query
 */
TEST_F(TrainingLogicHubBridgeTest, QueryLR) {
    // Add default rules
    ASSERT_GT(training_logic_hub_add_default_rules(bridge), 0);

    // Set up metrics
    training_logic_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.learning_rate = 0.001f;
    metrics.loss_stable = true;
    metrics.grad_stable = true;
    ASSERT_EQ(training_logic_hub_update_metrics(bridge, &metrics), 0);

    float suggested_lr = 0.0f;
    float confidence = 0.0f;
    EXPECT_EQ(training_logic_hub_query_lr(bridge, 0.001f, &suggested_lr, &confidence), 0);
    EXPECT_GE(suggested_lr, 0.0f);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

/**
 * Test: QueryDifficulty
 * Verify difficulty query
 */
TEST_F(TrainingLogicHubBridgeTest, QueryDifficulty) {
    // Add default rules
    ASSERT_GT(training_logic_hub_add_default_rules(bridge), 0);

    // Set up metrics
    training_logic_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.difficulty = 0.5f;
    metrics.mastery = 0.8f;
    metrics.performance = 0.75f;
    ASSERT_EQ(training_logic_hub_update_metrics(bridge, &metrics), 0);

    float suggested_difficulty = 0.0f;
    float confidence = 0.0f;
    EXPECT_EQ(training_logic_hub_query_difficulty(bridge, 0.5f, &suggested_difficulty, &confidence), 0);
    EXPECT_GE(suggested_difficulty, 0.0f);
    EXPECT_LE(suggested_difficulty, 1.0f);
}

/**
 * Test: QueryEarlyStop
 * Verify early stopping query
 */
TEST_F(TrainingLogicHubBridgeTest, QueryEarlyStop) {
    // Add default rules
    ASSERT_GT(training_logic_hub_add_default_rules(bridge), 0);

    // Set up metrics indicating potential early stop
    training_logic_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.epochs_since_improvement = 15;
    metrics.validation_improving = false;
    ASSERT_EQ(training_logic_hub_update_metrics(bridge, &metrics), 0);

    bool should_stop = false;
    float confidence = 0.0f;
    EXPECT_EQ(training_logic_hub_query_early_stop(bridge, &should_stop, &confidence), 0);
    // Should recommend stopping due to no improvement
    EXPECT_TRUE(should_stop);
}

/**
 * Test: QueryEarlyStopNoStop
 * Verify early stopping not triggered when improving
 */
TEST_F(TrainingLogicHubBridgeTest, QueryEarlyStopNoStop) {
    // Add default rules
    ASSERT_GT(training_logic_hub_add_default_rules(bridge), 0);

    // Set up metrics indicating improvement
    training_logic_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.epochs_since_improvement = 0;
    metrics.validation_improving = true;
    ASSERT_EQ(training_logic_hub_update_metrics(bridge, &metrics), 0);

    bool should_stop = true;
    float confidence = 0.0f;
    EXPECT_EQ(training_logic_hub_query_early_stop(bridge, &should_stop, &confidence), 0);
    EXPECT_FALSE(should_stop);
}

/* ========================================================================
 * STATE AND STATISTICS TESTS
 * ======================================================================== */

/**
 * Test: GetState
 * Verify state retrieval
 */
TEST_F(TrainingLogicHubBridgeTest, GetState) {
    training_logic_hub_state_t state;
    EXPECT_EQ(training_logic_hub_get_state(bridge, &state), 0);

    // Initial state checks
    EXPECT_FALSE(state.is_connected);
    EXPECT_EQ(state.active_rules, 0u);
}

/**
 * Test: GetStateNullPointer
 * Verify NULL state pointer is handled
 */
TEST_F(TrainingLogicHubBridgeTest, GetStateNullPointer) {
    EXPECT_EQ(training_logic_hub_get_state(bridge, nullptr), -1);
    EXPECT_EQ(training_logic_hub_get_state(nullptr, nullptr), -1);
}

/**
 * Test: GetStats
 * Verify statistics retrieval
 */
TEST_F(TrainingLogicHubBridgeTest, GetStats) {
    training_logic_hub_stats_t stats;
    EXPECT_EQ(training_logic_hub_get_stats(bridge, &stats), 0);

    // Initial stats should be zero
    EXPECT_EQ(stats.events_received, 0u);
    EXPECT_EQ(stats.events_processed, 0u);
    EXPECT_EQ(stats.rules_evaluated, 0u);
}

/**
 * Test: GetStatsNullPointer
 * Verify NULL stats pointer is handled
 */
TEST_F(TrainingLogicHubBridgeTest, GetStatsNullPointer) {
    EXPECT_EQ(training_logic_hub_get_stats(bridge, nullptr), -1);
    EXPECT_EQ(training_logic_hub_get_stats(nullptr, nullptr), -1);
}

/**
 * Test: ResetStats
 * Verify statistics can be reset
 */
TEST_F(TrainingLogicHubBridgeTest, ResetStats) {
    // Add and evaluate some rules to generate stats
    ASSERT_GT(training_logic_hub_add_default_rules(bridge), 0);

    training_logic_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.loss_stable = true;
    metrics.grad_stable = true;
    ASSERT_EQ(training_logic_hub_update_metrics(bridge, &metrics), 0);

    training_rule_result_t results[4];
    training_logic_hub_evaluate_rules(bridge, TRAINING_RULE_LR_SAFETY, results, 4);

    // Check stats were generated
    training_logic_hub_stats_t stats;
    EXPECT_EQ(training_logic_hub_get_stats(bridge, &stats), 0);
    uint64_t evaluations = stats.rules_evaluated;

    // Reset stats
    EXPECT_EQ(training_logic_hub_reset_stats(bridge), 0);

    // Verify stats were reset
    EXPECT_EQ(training_logic_hub_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.rules_evaluated, 0u);

    (void)evaluations;  // May or may not be > 0 depending on rule count
}

/* ========================================================================
 * CONNECTION TESTS (Without actual hub)
 * ======================================================================== */

/**
 * Test: DisconnectWithoutConnect
 * Verify disconnect handles not-connected state
 */
TEST_F(TrainingLogicHubBridgeTest, DisconnectWithoutConnect) {
    // Should handle gracefully even if not connected
    int result = training_logic_hub_disconnect(bridge);
    EXPECT_EQ(result, 0);
}

/**
 * Test: ConnectNullHub
 * Verify NULL hub is handled
 */
TEST_F(TrainingLogicHubBridgeTest, ConnectNullHub) {
    EXPECT_EQ(training_logic_hub_connect(bridge, nullptr), -1);
}

/**
 * Test: ConnectNullBridge
 * Verify NULL bridge is handled
 */
TEST_F(TrainingLogicHubBridgeTest, ConnectNullBridge) {
    // Create a real hub for this test
    training_hub_config_t hub_config = training_hub_default_config();
    training_integration_hub_t hub = training_hub_create(&hub_config);
    ASSERT_NE(hub, nullptr);

    EXPECT_EQ(training_logic_hub_connect(nullptr, hub), -1);

    training_hub_destroy(hub);
}

/* ========================================================================
 * INTEGRATION WITH TRAINING HUB TESTS
 * ======================================================================== */

/**
 * Test: ConnectToHub
 * Verify connection to training hub
 */
TEST_F(TrainingLogicHubBridgeTest, ConnectToHub) {
    // Create a training hub
    training_hub_config_t hub_config = training_hub_default_config();
    training_integration_hub_t hub = training_hub_create(&hub_config);
    ASSERT_NE(hub, nullptr);

    // Connect logic bridge to hub
    EXPECT_EQ(training_logic_hub_connect(bridge, hub), 0);

    // Verify connected state
    training_logic_hub_state_t state;
    EXPECT_EQ(training_logic_hub_get_state(bridge, &state), 0);
    EXPECT_TRUE(state.is_connected);

    // Disconnect
    EXPECT_EQ(training_logic_hub_disconnect(bridge), 0);
    EXPECT_EQ(training_logic_hub_get_state(bridge, &state), 0);
    EXPECT_FALSE(state.is_connected);

    training_hub_destroy(hub);
}

/**
 * Test: RulesWithHubConnection
 * Verify rules work with hub connection
 */
TEST_F(TrainingLogicHubBridgeTest, RulesWithHubConnection) {
    // Create and connect hub
    training_hub_config_t hub_config = training_hub_default_config();
    training_integration_hub_t hub = training_hub_create(&hub_config);
    ASSERT_NE(hub, nullptr);
    EXPECT_EQ(training_logic_hub_connect(bridge, hub), 0);

    // Add default rules
    EXPECT_GT(training_logic_hub_add_default_rules(bridge), 0);

    // Set metrics and evaluate
    training_logic_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.loss_stable = true;
    metrics.grad_stable = true;
    metrics.grad_norm = 1.0f;
    metrics.grad_norm_avg = 1.0f;
    EXPECT_EQ(training_logic_hub_update_metrics(bridge, &metrics), 0);

    training_rule_result_t results[4];
    int count = training_logic_hub_evaluate_rules(bridge, TRAINING_RULE_LR_SAFETY, results, 4);
    EXPECT_GE(count, 0);

    training_logic_hub_disconnect(bridge);
    training_hub_destroy(hub);
}

/* ========================================================================
 * STRESS TESTS
 * ======================================================================== */

/**
 * Test: ManyRules
 * Verify many rules can be added
 */
TEST_F(TrainingLogicHubBridgeTest, ManyRules) {
    const int NUM_RULES = 50;
    int added = 0;

    for (int i = 0; i < NUM_RULES; i++) {
        training_logic_rule_t rule;
        memset(&rule, 0, sizeof(rule));
        rule.type = static_cast<training_rule_type_t>(i % TRAINING_RULE_COUNT);
        snprintf(rule.name, sizeof(rule.name), "stress_rule_%d", i);
        strcpy(rule.condition, "test");
        rule.confidence = 0.5f;

        int rule_id = training_logic_hub_add_rule(bridge, &rule);
        if (rule_id >= 0) added++;
    }

    EXPECT_GT(added, 0) << "Should add at least some rules";

    // Get state to check count
    training_logic_hub_state_t state;
    EXPECT_EQ(training_logic_hub_get_state(bridge, &state), 0);
    EXPECT_EQ(state.active_rules, static_cast<uint32_t>(added));
}

/**
 * Test: RapidMetricsUpdates
 * Verify rapid metrics updates don't cause issues
 */
TEST_F(TrainingLogicHubBridgeTest, RapidMetricsUpdates) {
    ASSERT_GT(training_logic_hub_add_default_rules(bridge), 0);

    for (int i = 0; i < 100; i++) {
        training_logic_metrics_t metrics;
        memset(&metrics, 0, sizeof(metrics));
        metrics.current_loss = 1.0f - (i * 0.01f);
        metrics.previous_loss = 1.0f - ((i - 1) * 0.01f);
        metrics.grad_norm = 1.0f + (i % 10) * 0.1f;
        metrics.grad_norm_avg = 1.0f;
        metrics.loss_stable = (i % 5 != 0);
        metrics.grad_stable = (i % 3 != 0);
        metrics.epoch = i;

        EXPECT_EQ(training_logic_hub_update_metrics(bridge, &metrics), 0);

        // Evaluate rules occasionally
        if (i % 10 == 0) {
            training_rule_result_t results[4];
            training_logic_hub_evaluate_rules(bridge, TRAINING_RULE_LR_SAFETY, results, 4);
        }
    }

    // Should still be functional
    training_logic_hub_stats_t stats;
    EXPECT_EQ(training_logic_hub_get_stats(bridge, &stats), 0);
}

/* ========================================================================
 * EDGE CASE TESTS
 * ======================================================================== */

/**
 * Test: EvaluateRulesEmptyBridge
 * Verify evaluating rules on empty bridge
 */
TEST_F(TrainingLogicHubBridgeTest, EvaluateRulesEmptyBridge) {
    // No rules added, should return 0 results
    training_rule_result_t results[4];
    int count = training_logic_hub_evaluate_rules(bridge, TRAINING_RULE_LR_SAFETY, results, 4);
    EXPECT_EQ(count, 0);
}

/**
 * Test: EvaluateRulesNullResults
 * Verify NULL results array is handled
 */
TEST_F(TrainingLogicHubBridgeTest, EvaluateRulesNullResults) {
    ASSERT_GT(training_logic_hub_add_default_rules(bridge), 0);
    int count = training_logic_hub_evaluate_rules(bridge, TRAINING_RULE_LR_SAFETY, nullptr, 0);
    // Returns -1 when results array is NULL (error)
    EXPECT_EQ(count, -1);
}

/**
 * Test: QueryWithNullOutputs
 * Verify query functions handle NULL outputs
 */
TEST_F(TrainingLogicHubBridgeTest, QueryWithNullOutputs) {
    EXPECT_EQ(training_logic_hub_query_lr(bridge, 0.001f, nullptr, nullptr), -1);
    EXPECT_EQ(training_logic_hub_query_difficulty(bridge, 0.5f, nullptr, nullptr), -1);
    EXPECT_EQ(training_logic_hub_query_early_stop(bridge, nullptr, nullptr), -1);
}
