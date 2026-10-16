/**
 * @file test_training_symbolic_logic_hub_regression.cpp
 * @brief Regression tests for training symbolic logic hub bridge
 * @version 1.0.0
 * @date 2026-01-09
 *
 * Tests for regressions in:
 * - Rule evaluation correctness
 * - Confidence learning stability
 * - Memory management
 * - Numerical stability
 * - Edge cases and boundary conditions
 * - Performance characteristics
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <limits>
#include <chrono>

#include "training/integration/nimcp_training_symbolic_logic_hub_bridge.h"
#include "training/integration/nimcp_training_integration_hub.h"
#include "training/integration/nimcp_training_event_types.h"

/* ========================================================================
 * Test Fixture
 * ======================================================================== */

class TrainingLogicHubRegressionTest : public ::testing::Test {
protected:
    training_logic_hub_bridge_t* bridge;
    training_logic_hub_config_t config;

    void SetUp() override {
        ASSERT_EQ(training_logic_hub_default_config(&config), 0);
        bridge = training_logic_hub_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            training_logic_hub_destroy(bridge);
            bridge = nullptr;
        }
    }

    // Helper to create and initialize metrics
    training_logic_metrics_t create_metrics(
        float loss = 0.5f,
        float grad_norm = 1.0f,
        bool stable = true,
        uint32_t epochs_no_improve = 0)
    {
        training_logic_metrics_t metrics;
        memset(&metrics, 0, sizeof(metrics));
        metrics.current_loss = loss;
        metrics.previous_loss = loss + 0.1f;
        metrics.best_loss = loss - 0.1f;
        metrics.loss_stable = stable;
        metrics.grad_norm = grad_norm;
        metrics.grad_norm_avg = grad_norm;
        metrics.grad_stable = stable;
        metrics.grad_exploding = grad_norm > 10.0f;
        metrics.grad_vanishing = grad_norm < 0.001f;
        metrics.learning_rate = 0.001f;
        metrics.difficulty = 0.5f;
        metrics.mastery = 0.7f;
        metrics.performance = 0.75f;
        metrics.epochs_since_improvement = epochs_no_improve;
        metrics.validation_loss = loss;
        metrics.validation_improving = epochs_no_improve == 0;
        return metrics;
    }
};

/* ========================================================================
 * Rule Evaluation Correctness Tests
 * ======================================================================== */

/**
 * Regression: LR Safety Rule Consistency
 * Verify LR safety rules produce consistent results
 */
TEST_F(TrainingLogicHubRegressionTest, LRSafetyRuleConsistency) {
    ASSERT_GT(training_logic_hub_add_default_rules(bridge), 0);

    // Test with same metrics multiple times
    auto metrics = create_metrics(0.5f, 1.0f, true, 0);
    ASSERT_EQ(training_logic_hub_update_metrics(bridge, &metrics), 0);

    training_rule_result_t results1[4];
    int count1 = training_logic_hub_evaluate_rules(
        bridge, TRAINING_RULE_LR_SAFETY, results1, 4);

    training_rule_result_t results2[4];
    int count2 = training_logic_hub_evaluate_rules(
        bridge, TRAINING_RULE_LR_SAFETY, results2, 4);

    // Results should be identical
    EXPECT_EQ(count1, count2);
    for (int i = 0; i < std::min(count1, count2); i++) {
        EXPECT_EQ(results1[i].satisfied, results2[i].satisfied);
        EXPECT_FLOAT_EQ(results1[i].confidence, results2[i].confidence);
    }
}

/**
 * Regression: Gradient Clip Rule Threshold
 * Verify gradient clipping triggers at correct threshold
 */
TEST_F(TrainingLogicHubRegressionTest, GradientClipRuleThreshold) {
    ASSERT_GT(training_logic_hub_add_default_rules(bridge), 0);

    // Test at various gradient norms
    struct TestCase {
        float grad_norm;
        float grad_avg;
        bool should_clip;
    };

    TestCase cases[] = {
        {1.0f, 1.0f, false},    // Normal
        {5.0f, 1.0f, false},    // 5x average - borderline
        {10.0f, 1.0f, true},    // 10x average - should clip
        {100.0f, 1.0f, true},   // 100x average - definitely clip
        {0.1f, 1.0f, false},    // Below average
    };

    for (const auto& tc : cases) {
        auto metrics = create_metrics(0.5f, tc.grad_norm, tc.grad_norm <= tc.grad_avg * 5, 0);
        metrics.grad_norm_avg = tc.grad_avg;
        metrics.grad_exploding = tc.grad_norm > tc.grad_avg * 10.0f;
        ASSERT_EQ(training_logic_hub_update_metrics(bridge, &metrics), 0);

        training_rule_result_t results[4];
        int count = training_logic_hub_evaluate_rules(
            bridge, TRAINING_RULE_GRADIENT_CLIP, results, 4);

        bool clip_triggered = false;
        for (int i = 0; i < count; i++) {
            if (results[i].satisfied) {
                clip_triggered = true;
                break;
            }
        }

        EXPECT_EQ(clip_triggered, tc.should_clip)
            << "Gradient norm " << tc.grad_norm << " with avg " << tc.grad_avg;
    }
}

/**
 * Regression: Early Stop Rule Patience
 * Verify early stopping respects patience parameter
 */
TEST_F(TrainingLogicHubRegressionTest, EarlyStopRulePatience) {
    ASSERT_GT(training_logic_hub_add_default_rules(bridge), 0);

    // Test at various epochs without improvement
    for (uint32_t epochs = 0; epochs <= 20; epochs += 2) {
        auto metrics = create_metrics(0.5f, 1.0f, true, epochs);
        ASSERT_EQ(training_logic_hub_update_metrics(bridge, &metrics), 0);

        bool should_stop = false;
        float confidence = 0.0f;
        EXPECT_EQ(training_logic_hub_query_early_stop(bridge, &should_stop, &confidence), 0);

        // Default patience is typically 10 epochs
        if (epochs > 10) {
            EXPECT_TRUE(should_stop)
                << "Should stop after " << epochs << " epochs without improvement";
        } else if (epochs <= 5) {
            EXPECT_FALSE(should_stop)
                << "Should not stop after only " << epochs << " epochs";
        }
    }
}

/* ========================================================================
 * Confidence Learning Stability Tests
 * ======================================================================== */

/**
 * Regression: Confidence Bounds
 * Verify confidence stays within [0, 1]
 */
TEST_F(TrainingLogicHubRegressionTest, ConfidenceBounds) {
    // Add rule with extreme confidence
    training_logic_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.type = TRAINING_RULE_LR_SAFETY;
    strcpy(rule.name, "test_bounds_rule");
    rule.confidence = 0.99f;  // Near maximum
    rule.priority = 0.5f;

    int rule_id = training_logic_hub_add_rule(bridge, &rule);
    ASSERT_GE(rule_id, 0);

    // Many positive outcomes - should not exceed 1.0
    for (int i = 0; i < 100; i++) {
        training_logic_hub_report_outcome(bridge, true, true);
    }

    float conf = training_logic_hub_get_rule_confidence(bridge, rule_id);
    EXPECT_LE(conf, 1.0f) << "Confidence should not exceed 1.0";
    EXPECT_GE(conf, 0.0f) << "Confidence should not be negative";

    // Many negative outcomes - should not go below min_confidence
    for (int i = 0; i < 1000; i++) {
        training_logic_hub_report_outcome(bridge, false, false);
    }

    conf = training_logic_hub_get_rule_confidence(bridge, rule_id);
    EXPECT_GE(conf, config.min_rule_confidence)
        << "Confidence should not go below minimum";
}

/**
 * Regression: Confidence Decay Stability
 * Verify confidence changes remain numerically stable under repeated outcomes
 */
TEST_F(TrainingLogicHubRegressionTest, ConfidenceDecayStability) {
    training_logic_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.type = TRAINING_RULE_CHECKPOINT_TRIGGER;
    strcpy(rule.name, "decay_test_rule");
    rule.confidence = 0.8f;

    int rule_id = training_logic_hub_add_rule(bridge, &rule);
    ASSERT_GE(rule_id, 0);

    float initial_conf = training_logic_hub_get_rule_confidence(bridge, rule_id);
    EXPECT_FLOAT_EQ(initial_conf, 0.8f);

    // Report many negative training outcomes (loss worsened, validation worsened)
    for (int i = 0; i < 100; i++) {
        int result = training_logic_hub_report_outcome(bridge, false, false);
        EXPECT_EQ(result, 0) << "Report outcome should succeed";
    }

    // Confidence should remain within valid bounds after many negative outcomes
    float final_conf = training_logic_hub_get_rule_confidence(bridge, rule_id);
    EXPECT_GE(final_conf, config.min_rule_confidence)
        << "Confidence should not go below minimum";
    EXPECT_LE(final_conf, 1.0f)
        << "Confidence should not exceed 1.0";

    // Test positive outcomes don't cause overflow
    for (int i = 0; i < 100; i++) {
        int result = training_logic_hub_report_outcome(bridge, true, true);
        EXPECT_EQ(result, 0) << "Report outcome should succeed";
    }

    float after_positive = training_logic_hub_get_rule_confidence(bridge, rule_id);
    EXPECT_GE(after_positive, config.min_rule_confidence)
        << "Confidence should stay within bounds";
    EXPECT_LE(after_positive, 1.0f)
        << "Confidence should not exceed maximum";
}

/* ========================================================================
 * Numerical Stability Tests
 * ======================================================================== */

/**
 * Regression: Extreme Loss Values
 * Verify handling of extreme loss values
 */
TEST_F(TrainingLogicHubRegressionTest, ExtremeLossValues) {
    ASSERT_GT(training_logic_hub_add_default_rules(bridge), 0);

    struct TestCase {
        float loss;
        const char* description;
    };

    TestCase cases[] = {
        {0.0f, "Zero loss"},
        {1e-10f, "Very small loss"},
        {1e10f, "Very large loss"},
        {std::numeric_limits<float>::min(), "Minimum float"},
        {std::numeric_limits<float>::max() / 2, "Large float"},
    };

    for (const auto& tc : cases) {
        auto metrics = create_metrics(tc.loss, 1.0f, true, 0);
        int result = training_logic_hub_update_metrics(bridge, &metrics);
        EXPECT_EQ(result, 0) << "Failed for: " << tc.description;

        // Query should not crash
        float suggested_lr = 0.0f;
        float confidence = 0.0f;
        result = training_logic_hub_query_lr(bridge, 0.001f, &suggested_lr, &confidence);
        EXPECT_EQ(result, 0) << "Query failed for: " << tc.description;
        EXPECT_FALSE(std::isnan(suggested_lr)) << "NaN LR for: " << tc.description;
        EXPECT_FALSE(std::isinf(suggested_lr)) << "Inf LR for: " << tc.description;
    }
}

/**
 * Regression: Extreme Gradient Values
 * Verify handling of extreme gradient norms
 */
TEST_F(TrainingLogicHubRegressionTest, ExtremeGradientValues) {
    ASSERT_GT(training_logic_hub_add_default_rules(bridge), 0);

    float extreme_grads[] = {0.0f, 1e-15f, 1e-6f, 1e6f, 1e15f};

    for (float grad : extreme_grads) {
        auto metrics = create_metrics(0.5f, grad, grad > 0.0001f && grad < 1000.0f, 0);
        int result = training_logic_hub_update_metrics(bridge, &metrics);
        EXPECT_EQ(result, 0) << "Failed for gradient: " << grad;

        training_rule_result_t results[4];
        int count = training_logic_hub_evaluate_rules(
            bridge, TRAINING_RULE_GRADIENT_CLIP, results, 4);
        EXPECT_GE(count, 0) << "Evaluation failed for gradient: " << grad;

        for (int i = 0; i < count; i++) {
            EXPECT_FALSE(std::isnan(results[i].confidence))
                << "NaN confidence for gradient: " << grad;
        }
    }
}

/**
 * Regression: NaN and Inf Handling
 * Verify NaN/Inf values are handled gracefully
 */
TEST_F(TrainingLogicHubRegressionTest, NaNInfHandling) {
    ASSERT_GT(training_logic_hub_add_default_rules(bridge), 0);

    // Test NaN handling
    auto metrics = create_metrics(0.5f, 1.0f, true, 0);
    metrics.current_loss = std::numeric_limits<float>::quiet_NaN();
    int result = training_logic_hub_update_metrics(bridge, &metrics);
    // Should either succeed with NaN handling or fail gracefully
    (void)result;

    // Test Inf handling
    metrics = create_metrics(0.5f, 1.0f, true, 0);
    metrics.grad_norm = std::numeric_limits<float>::infinity();
    result = training_logic_hub_update_metrics(bridge, &metrics);
    (void)result;

    // Queries should not crash
    bool should_stop = false;
    float confidence = 0.0f;
    training_logic_hub_query_early_stop(bridge, &should_stop, &confidence);
    // Just verify no crash
}

/* ========================================================================
 * Memory Management Tests
 * ======================================================================== */

/**
 * Regression: Max Rules Limit
 * Verify behavior at max rules limit
 */
TEST_F(TrainingLogicHubRegressionTest, MaxRulesLimit) {
    int added = 0;
    int failed = 0;

    for (int i = 0; i < TRAINING_LOGIC_MAX_RULES + 10; i++) {
        training_logic_rule_t rule;
        memset(&rule, 0, sizeof(rule));
        rule.type = static_cast<training_rule_type_t>(i % TRAINING_RULE_COUNT);
        snprintf(rule.name, sizeof(rule.name), "max_test_rule_%d", i);
        rule.confidence = 0.5f;

        int rule_id = training_logic_hub_add_rule(bridge, &rule);
        if (rule_id >= 0) {
            added++;
        } else {
            failed++;
        }
    }

    // Should have added up to max, then started failing
    EXPECT_EQ(added, TRAINING_LOGIC_MAX_RULES);
    EXPECT_EQ(failed, 10);

    // Verify existing rules still work
    training_rule_result_t results[4];
    int count = training_logic_hub_evaluate_rules(
        bridge, TRAINING_RULE_LR_SAFETY, results, 4);
    EXPECT_GE(count, 0);
}

/**
 * Regression: Add Remove Cycles
 * Verify rule management stability over multiple cycles
 */
TEST_F(TrainingLogicHubRegressionTest, AddRemoveCycles) {
    // Test a few cycles of adding and removing rules
    int successful_adds = 0;
    int successful_removes = 0;

    for (int cycle = 0; cycle < 5; cycle++) {
        // Add a rule
        training_logic_rule_t rule;
        memset(&rule, 0, sizeof(rule));
        rule.type = TRAINING_RULE_CUSTOM;
        snprintf(rule.name, sizeof(rule.name), "cycle_%d_rule", cycle);
        rule.confidence = 0.5f;

        int rule_id = training_logic_hub_add_rule(bridge, &rule);
        if (rule_id >= 0) {
            successful_adds++;

            // Try to remove the rule
            if (training_logic_hub_remove_rule(bridge, rule_id) == 0) {
                successful_removes++;
            }
        }
    }

    // At least some operations should succeed
    EXPECT_GT(successful_adds, 0) << "Should be able to add rules";

    // Verify bridge is still functional
    training_logic_hub_state_t state;
    EXPECT_EQ(training_logic_hub_get_state(bridge, &state), 0);
}

/**
 * Regression: Create Destroy Cycles
 * Verify no memory leaks in create/destroy cycles
 */
TEST_F(TrainingLogicHubRegressionTest, CreateDestroyCycles) {
    // Destroy the bridge from SetUp
    training_logic_hub_destroy(bridge);
    bridge = nullptr;

    for (int cycle = 0; cycle < 50; cycle++) {
        training_logic_hub_config_t cfg;
        ASSERT_EQ(training_logic_hub_default_config(&cfg), 0);

        training_logic_hub_bridge_t* b = training_logic_hub_create(&cfg);
        ASSERT_NE(b, nullptr) << "Failed to create in cycle " << cycle;

        // Add some rules
        training_logic_hub_add_default_rules(b);

        // Update metrics
        auto metrics = create_metrics(0.5f, 1.0f, true, 0);
        training_logic_hub_update_metrics(b, &metrics);

        // Evaluate rules
        training_rule_result_t results[4];
        training_logic_hub_evaluate_rules(b, TRAINING_RULE_LR_SAFETY, results, 4);

        training_logic_hub_destroy(b);
    }

    // Recreate bridge for TearDown
    ASSERT_EQ(training_logic_hub_default_config(&config), 0);
    bridge = training_logic_hub_create(&config);
    ASSERT_NE(bridge, nullptr);
}

/* ========================================================================
 * Performance Regression Tests
 * ======================================================================== */

/**
 * Regression: Rule Evaluation Performance
 * Verify rule evaluation meets performance expectations
 */
TEST_F(TrainingLogicHubRegressionTest, RuleEvaluationPerformance) {
    ASSERT_GT(training_logic_hub_add_default_rules(bridge), 0);

    auto metrics = create_metrics(0.5f, 1.0f, true, 0);
    ASSERT_EQ(training_logic_hub_update_metrics(bridge, &metrics), 0);

    const int NUM_ITERATIONS = 1000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        training_rule_result_t results[8];
        training_logic_hub_evaluate_rules(bridge, TRAINING_RULE_LR_SAFETY, results, 8);
        training_logic_hub_evaluate_rules(bridge, TRAINING_RULE_GRADIENT_CLIP, results, 8);
        training_logic_hub_evaluate_rules(bridge, TRAINING_RULE_EARLY_STOP, results, 8);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Average should be less than 100 microseconds per iteration
    double avg_us = static_cast<double>(duration.count()) / NUM_ITERATIONS;
    EXPECT_LT(avg_us, 100.0)
        << "Rule evaluation took " << avg_us << " microseconds average";
}

/**
 * Regression: Metrics Update Performance
 * Verify metrics update is fast
 */
TEST_F(TrainingLogicHubRegressionTest, MetricsUpdatePerformance) {
    const int NUM_ITERATIONS = 10000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        auto metrics = create_metrics(0.5f + i * 0.0001f, 1.0f, true, 0);
        training_logic_hub_update_metrics(bridge, &metrics);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Average should be less than 10 microseconds
    double avg_us = static_cast<double>(duration.count()) / NUM_ITERATIONS;
    EXPECT_LT(avg_us, 10.0)
        << "Metrics update took " << avg_us << " microseconds average";
}

/**
 * Regression: Query Performance
 * Verify query APIs are fast
 */
TEST_F(TrainingLogicHubRegressionTest, QueryPerformance) {
    ASSERT_GT(training_logic_hub_add_default_rules(bridge), 0);

    auto metrics = create_metrics(0.5f, 1.0f, true, 5);
    ASSERT_EQ(training_logic_hub_update_metrics(bridge, &metrics), 0);

    const int NUM_ITERATIONS = 1000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        float suggested = 0.0f;
        float conf = 0.0f;
        bool stop = false;

        training_logic_hub_query_lr(bridge, 0.001f, &suggested, &conf);
        training_logic_hub_query_difficulty(bridge, 0.5f, &suggested, &conf);
        training_logic_hub_query_early_stop(bridge, &stop, &conf);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Average should be less than 50 microseconds per iteration (3 queries)
    double avg_us = static_cast<double>(duration.count()) / NUM_ITERATIONS;
    EXPECT_LT(avg_us, 50.0)
        << "Queries took " << avg_us << " microseconds average";
}

/* ========================================================================
 * Edge Case Regression Tests
 * ======================================================================== */

/**
 * Regression: Empty Bridge Operations
 * Verify operations on empty bridge don't crash
 */
TEST_F(TrainingLogicHubRegressionTest, EmptyBridgeOperations) {
    // No rules added

    // All operations should handle empty state gracefully
    training_rule_result_t results[4];
    EXPECT_EQ(training_logic_hub_evaluate_rules(
        bridge, TRAINING_RULE_LR_SAFETY, results, 4), 0);

    float suggested = 0.0f;
    float conf = 0.0f;
    EXPECT_EQ(training_logic_hub_query_lr(bridge, 0.001f, &suggested, &conf), 0);

    bool stop = false;
    EXPECT_EQ(training_logic_hub_query_early_stop(bridge, &stop, &conf), 0);

    EXPECT_EQ(training_logic_hub_report_outcome(bridge, true, true), 0);

    training_logic_hub_stats_t stats;
    EXPECT_EQ(training_logic_hub_get_stats(bridge, &stats), 0);
}

/**
 * Regression: Duplicate Rule Names
 * Verify handling of duplicate rule names
 */
TEST_F(TrainingLogicHubRegressionTest, DuplicateRuleNames) {
    training_logic_rule_t rule1;
    memset(&rule1, 0, sizeof(rule1));
    rule1.type = TRAINING_RULE_LR_SAFETY;
    strcpy(rule1.name, "duplicate_name");
    rule1.confidence = 0.5f;

    int id1 = training_logic_hub_add_rule(bridge, &rule1);
    EXPECT_GE(id1, 0);

    // Add another rule with same name
    training_logic_rule_t rule2;
    memset(&rule2, 0, sizeof(rule2));
    rule2.type = TRAINING_RULE_GRADIENT_CLIP;
    strcpy(rule2.name, "duplicate_name");
    rule2.confidence = 0.7f;

    int id2 = training_logic_hub_add_rule(bridge, &rule2);
    // Should still succeed (or fail gracefully) - names don't have to be unique
    (void)id2;

    // First rule should still be retrievable
    training_logic_rule_t retrieved;
    EXPECT_EQ(training_logic_hub_get_rule(bridge, id1, &retrieved), 0);
    EXPECT_EQ(retrieved.type, TRAINING_RULE_LR_SAFETY);
}

/**
 * Regression: Zero Metrics
 * Verify handling when all metrics are zero
 */
TEST_F(TrainingLogicHubRegressionTest, ZeroMetrics) {
    ASSERT_GT(training_logic_hub_add_default_rules(bridge), 0);

    training_logic_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    EXPECT_EQ(training_logic_hub_update_metrics(bridge, &metrics), 0);

    // All queries should handle zero metrics
    float suggested = 0.0f;
    float conf = 0.0f;
    EXPECT_EQ(training_logic_hub_query_lr(bridge, 0.001f, &suggested, &conf), 0);
    EXPECT_FALSE(std::isnan(suggested));

    EXPECT_EQ(training_logic_hub_query_difficulty(bridge, 0.5f, &suggested, &conf), 0);
    EXPECT_FALSE(std::isnan(suggested));

    bool stop = false;
    EXPECT_EQ(training_logic_hub_query_early_stop(bridge, &stop, &conf), 0);
}

/**
 * Regression: Action Safety Unknown Action
 * Verify handling of unknown action names
 */
TEST_F(TrainingLogicHubRegressionTest, UnknownActionSafety) {
    ASSERT_GT(training_logic_hub_add_default_rules(bridge), 0);

    auto metrics = create_metrics(0.5f, 1.0f, true, 0);
    ASSERT_EQ(training_logic_hub_update_metrics(bridge, &metrics), 0);

    float conf = 0.0f;

    // Unknown actions should be handled gracefully
    training_logic_hub_is_action_safe(bridge, "unknown_action_xyz", &conf);
    training_logic_hub_is_action_safe(bridge, "", &conf);
    training_logic_hub_is_action_safe(bridge, nullptr, &conf);
    // No crashes expected
}

/**
 * Regression: Invalid Rule ID Operations
 * Verify handling of invalid rule IDs
 */
TEST_F(TrainingLogicHubRegressionTest, InvalidRuleIDOperations) {
    // Operations on non-existent rule IDs
    training_logic_rule_t rule;
    EXPECT_EQ(training_logic_hub_get_rule(bridge, 9999, &rule), -1);
    EXPECT_EQ(training_logic_hub_remove_rule(bridge, 9999), -1);
    EXPECT_LT(training_logic_hub_get_rule_confidence(bridge, 9999), 0.0f);

    // Negative IDs
    EXPECT_EQ(training_logic_hub_get_rule(bridge, -1, &rule), -1);
    EXPECT_EQ(training_logic_hub_remove_rule(bridge, -1), -1);

    // Evaluate non-existent rule
    training_rule_result_t result;
    EXPECT_EQ(training_logic_hub_evaluate_rule(bridge, 9999, &result), -1);
}
