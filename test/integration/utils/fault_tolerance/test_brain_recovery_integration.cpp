/**
 * @file test_brain_recovery_integration.cpp
 * @brief Integration tests for brain-driven fault tolerance system
 *
 * Tests the complete workflow:
 * Error Detection → Diagnostics → Brain Analysis → Strategy Selection →
 * Runtime Adaptation → Recovery Execution → Learning
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_brain_recovery_integration.h"
#include "utils/fault_tolerance/nimcp_runtime_adaptation.h"
#include "utils/fault_tolerance/nimcp_diagnostics.h"
#include "utils/fault_tolerance/nimcp_recovery.h"
#include "core/brain/nimcp_brain.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainRecoveryIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;
    brain_recovery_context_t recovery_ctx;
    runtime_adaptation_context_t adaptation_ctx;

    void SetUp() override {
        // Create test brain
        brain = brain_create("test_brain", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);

        // Initialize recovery systems
        recovery_ctx = brain_recovery_init(brain);
        ASSERT_NE(recovery_ctx, nullptr);

        adaptation_ctx = runtime_adaptation_create(brain);
        ASSERT_NE(adaptation_ctx, nullptr);
    }

    void TearDown() override {
        runtime_adaptation_destroy(adaptation_ctx);
        brain_recovery_shutdown(recovery_ctx);
        brain_destroy(brain);
    }

    // Helper: Create test diagnosis
    diagnostic_result_t* create_nan_diagnosis() {
        auto* diag = (diagnostic_result_t*)calloc(1, sizeof(diagnostic_result_t));
        diag->error_type = ERROR_TYPE_NAN_DETECTED;
        diag->severity = DIAG_SEVERITY_ERROR;
        strncpy(diag->root_cause, "NaN detected in layer 3 weights",
                sizeof(diag->root_cause) - 1);
        strncpy(diag->symptoms, "Training divergence, NaN propagation",
                sizeof(diag->symptoms) - 1);
        diag->confidence = 0.95f;
        return diag;
    }

    diagnostic_result_t* create_memory_diagnosis() {
        auto* diag = (diagnostic_result_t*)calloc(1, sizeof(diagnostic_result_t));
        diag->error_type = ERROR_TYPE_OUT_OF_MEMORY;
        diag->severity = DIAG_SEVERITY_CRITICAL;
        strncpy(diag->root_cause, "Memory allocation failed",
                sizeof(diag->root_cause) - 1);
        strncpy(diag->symptoms, "OOM during batch processing",
                sizeof(diag->symptoms) - 1);
        diag->confidence = 1.0f;
        return diag;
    }
};

//=============================================================================
// Initialization Tests
//=============================================================================

TEST_F(BrainRecoveryIntegrationTest, Initialization) {
    EXPECT_NE(recovery_ctx, nullptr);
    EXPECT_NE(adaptation_ctx, nullptr);

    // Check initial state
    recovery_history_stats_t stats;
    EXPECT_TRUE(brain_recovery_get_stats(recovery_ctx, &stats));
    EXPECT_EQ(stats.total_recoveries, 0u);
    EXPECT_EQ(stats.total_patterns_learned, 0u);
}

//=============================================================================
// Strategy Selection Tests
//=============================================================================

TEST_F(BrainRecoveryIntegrationTest, NovelFailureStrategySelection) {
    // Create diagnosis for novel failure
    diagnostic_result_t* diagnosis = create_nan_diagnosis();

    // Brain should select strategy despite never seeing this before
    health_status_snapshot_t health;
    memset(&health, 0, sizeof(health));
    health.score = 70.0f;
    health.status = HEALTH_GOOD;

    brain_recovery_decision_t* decision =
        brain_recovery_select_strategy(recovery_ctx, diagnosis, &health);

    ASSERT_NE(decision, nullptr);

    // Verify decision structure
    EXPECT_NE(decision->selected_strategy, nullptr);
    EXPECT_GT(decision->confidence, 0.0f);
    EXPECT_GT(decision->predicted_success_prob, 0.0f);
    EXPECT_TRUE(decision->is_novel_situation);
    EXPECT_NE(decision->reasoning[0], '\0');

    // Cleanup
    brain_recovery_free_decision(decision);
    free(diagnosis);
}

TEST_F(BrainRecoveryIntegrationTest, LearnedPatternStrategySelection) {
    // First occurrence: Novel failure
    diagnostic_result_t* diagnosis1 = create_nan_diagnosis();
    health_status_snapshot_t health;
    memset(&health, 0, sizeof(health));
    health.score = 70.0f;

    brain_recovery_decision_t* decision1 =
        brain_recovery_select_strategy(recovery_ctx, diagnosis1, &health);
    ASSERT_NE(decision1, nullptr);

    // Simulate successful recovery
    recovery_result_t result1;
    result1.status = RECOVERY_SUCCESS;
    result1.tier = decision1->selected_strategy->tier;
    result1.action = decision1->selected_strategy->primary;
    result1.time_us = 100;

    // Brain learns from outcome
    brain_recovery_learn_outcome(recovery_ctx, decision1, &result1);

    // Second occurrence: Should use learned pattern
    diagnostic_result_t* diagnosis2 = create_nan_diagnosis();
    brain_recovery_decision_t* decision2 =
        brain_recovery_select_strategy(recovery_ctx, diagnosis2, &health);

    ASSERT_NE(decision2, nullptr);

    // Verify learned pattern is used
    EXPECT_FALSE(decision2->is_novel_situation);
    EXPECT_GT(decision2->confidence, decision1->confidence);

    // Cleanup
    brain_recovery_free_decision(decision1);
    brain_recovery_free_decision(decision2);
    free(diagnosis1);
    free(diagnosis2);
}

//=============================================================================
// Learning Tests
//=============================================================================

TEST_F(BrainRecoveryIntegrationTest, OutcomeLearning) {
    diagnostic_result_t* diagnosis = create_nan_diagnosis();
    health_status_snapshot_t health;
    memset(&health, 0, sizeof(health));
    health.score = 70.0f;

    // Get initial pattern count
    recovery_history_stats_t stats_before;
    brain_recovery_get_stats(recovery_ctx, &stats_before);

    // Execute recovery and learn
    brain_recovery_decision_t* decision =
        brain_recovery_select_strategy(recovery_ctx, diagnosis, &health);

    recovery_result_t result;
    result.status = RECOVERY_SUCCESS;
    result.tier = RECOVERY_TIER_TACTICAL;
    result.action = RECOVERY_ACTION_REDUCE_LR;
    result.time_us = 50;

    brain_recovery_learn_outcome(recovery_ctx, decision, &result);

    // Verify learning occurred
    recovery_history_stats_t stats_after;
    brain_recovery_get_stats(recovery_ctx, &stats_after);

    EXPECT_GT(stats_after.total_patterns_learned,
              stats_before.total_patterns_learned);

    // Cleanup
    brain_recovery_free_decision(decision);
    free(diagnosis);
}

TEST_F(BrainRecoveryIntegrationTest, SuccessProbabilityPrediction) {
    // Create strategy
    recovery_strategy_t strategy;
    strategy.tier = RECOVERY_TIER_TACTICAL;
    strategy.primary = RECOVERY_ACTION_REDUCE_LR;

    diagnostic_result_t* diagnosis = create_nan_diagnosis();

    // Predict success (should use base rate initially)
    float prob = brain_recovery_predict_success(
        recovery_ctx, &strategy, diagnosis);

    EXPECT_GT(prob, 0.0f);
    EXPECT_LE(prob, 1.0f);

    // Learn successful outcome
    health_status_snapshot_t health;
    memset(&health, 0, sizeof(health));
    brain_recovery_decision_t* decision =
        brain_recovery_select_strategy(recovery_ctx, diagnosis, &health);
    decision->selected_strategy->tier = strategy.tier;
    decision->selected_strategy->primary = strategy.primary;

    recovery_result_t result;
    result.status = RECOVERY_SUCCESS;
    result.tier = strategy.tier;
    result.action = strategy.primary;

    brain_recovery_learn_outcome(recovery_ctx, decision, &result);

    // Predict again (should be higher after learning success)
    diagnostic_result_t* diagnosis2 = create_nan_diagnosis();
    float prob_after = brain_recovery_predict_success(
        recovery_ctx, &strategy, diagnosis2);

    EXPECT_GE(prob_after, prob);

    // Cleanup
    brain_recovery_free_decision(decision);
    free(diagnosis);
    free(diagnosis2);
}

//=============================================================================
// Runtime Adaptation Tests
//=============================================================================

TEST_F(BrainRecoveryIntegrationTest, ParameterAdjustment) {
    // Get initial learning rate
    float initial_lr = runtime_adaptation_get_parameter(
        adaptation_ctx, RUNTIME_PARAM_LEARNING_RATE);

    EXPECT_GT(initial_lr, 0.0f);

    // Adjust learning rate
    bool success = runtime_adaptation_set_parameter(
        adaptation_ctx,
        RUNTIME_PARAM_LEARNING_RATE,
        initial_lr * 0.5f,
        "NaN detected - reducing LR");

    EXPECT_TRUE(success);

    // Verify adjustment
    float new_lr = runtime_adaptation_get_parameter(
        adaptation_ctx, RUNTIME_PARAM_LEARNING_RATE);

    EXPECT_FLOAT_EQ(new_lr, initial_lr * 0.5f);
}

TEST_F(BrainRecoveryIntegrationTest, FeatureToggling) {
    // Initially disabled
    EXPECT_FALSE(runtime_adaptation_is_feature_enabled(
        adaptation_ctx, RUNTIME_FEATURE_GRADIENT_CLIPPING));

    // Enable feature
    bool success = runtime_adaptation_enable_feature(
        adaptation_ctx,
        RUNTIME_FEATURE_GRADIENT_CLIPPING,
        "NaN detected - enabling clipping");

    EXPECT_TRUE(success);

    // Verify enabled
    EXPECT_TRUE(runtime_adaptation_is_feature_enabled(
        adaptation_ctx, RUNTIME_FEATURE_GRADIENT_CLIPPING));
}

TEST_F(BrainRecoveryIntegrationTest, AdaptationPolicy) {
    // Apply NaN detection policy
    bool success = runtime_adaptation_policy_nan_detected(adaptation_ctx);
    EXPECT_TRUE(success);

    // Verify policy effects
    EXPECT_TRUE(runtime_adaptation_is_feature_enabled(
        adaptation_ctx, RUNTIME_FEATURE_NAN_DETECTION));

    float lr = runtime_adaptation_get_parameter(
        adaptation_ctx, RUNTIME_PARAM_LEARNING_RATE);
    // Should be reduced from default
    EXPECT_LT(lr, 0.01f);
}

//=============================================================================
// Parameter Suggestion Tests
//=============================================================================

TEST_F(BrainRecoveryIntegrationTest, ParameterSuggestions) {
    diagnostic_result_t* diagnosis = create_nan_diagnosis();

    parameter_adjustment_t suggestions[10];
    uint32_t count = brain_recovery_suggest_parameters(
        recovery_ctx, diagnosis, suggestions, 10);

    // Should suggest at least one adjustment
    EXPECT_GT(count, 0u);

    if (count > 0) {
        // Verify suggestion structure
        EXPECT_NE(suggestions[0].parameter_name, nullptr);
        EXPECT_GT(suggestions[0].confidence, 0.0f);
        EXPECT_NE(suggestions[0].rationale[0], '\0');
    }

    free(diagnosis);
}

//=============================================================================
// End-to-End Recovery Workflow Tests
//=============================================================================

TEST_F(BrainRecoveryIntegrationTest, CompleteNaNRecoveryWorkflow) {
    // Step 1: Error occurs (simulate NaN detection)
    diagnostic_result_t* diagnosis = create_nan_diagnosis();

    // Step 2: Brain analyzes and selects strategy
    health_status_snapshot_t health;
    memset(&health, 0, sizeof(health));
    health.score = 70.0f;

    brain_recovery_decision_t* decision =
        brain_recovery_select_strategy(recovery_ctx, diagnosis, &health);
    ASSERT_NE(decision, nullptr);

    // Step 3: Apply runtime adaptations based on suggestions
    parameter_adjustment_t suggestions[10];
    uint32_t count = brain_recovery_suggest_parameters(
        recovery_ctx, diagnosis, suggestions, 10);

    for (uint32_t i = 0; i < count && i < 3; i++) {
        // Apply top 3 suggestions
        if (strcmp(suggestions[i].parameter_name, "learning_rate") == 0) {
            runtime_adaptation_set_parameter(
                adaptation_ctx,
                RUNTIME_PARAM_LEARNING_RATE,
                suggestions[i].suggested_value,
                suggestions[i].rationale);
        }
    }

    // Step 4: Execute recovery (simulated)
    recovery_result_t result;
    result.status = RECOVERY_SUCCESS;
    result.tier = decision->selected_strategy->tier;
    result.action = decision->selected_strategy->primary;
    result.time_us = 100;

    // Step 5: Brain learns from outcome
    brain_recovery_learn_outcome(recovery_ctx, decision, &result);

    // Step 6: Verify learning
    recovery_history_stats_t stats;
    brain_recovery_get_stats(recovery_ctx, &stats);

    EXPECT_EQ(stats.total_patterns_learned, 1u);
    EXPECT_GT(stats.success_rate, 0.0f);

    // Cleanup
    brain_recovery_free_decision(decision);
    free(diagnosis);
}

TEST_F(BrainRecoveryIntegrationTest, MultipleRecoveriesImproveAccuracy) {
    const int num_recoveries = 5;
    float prediction_errors[num_recoveries];

    for (int i = 0; i < num_recoveries; i++) {
        diagnostic_result_t* diagnosis = create_nan_diagnosis();
        health_status_snapshot_t health;
        memset(&health, 0, sizeof(health));
        health.score = 70.0f;

        // Get decision
        brain_recovery_decision_t* decision =
            brain_recovery_select_strategy(recovery_ctx, diagnosis, &health);

        // Record prediction
        float predicted = decision->predicted_success_prob;

        // Execute (assume success)
        recovery_result_t result;
        result.status = RECOVERY_SUCCESS;
        result.tier = decision->selected_strategy->tier;
        result.action = decision->selected_strategy->primary;

        // Learn
        brain_recovery_learn_outcome(recovery_ctx, decision, &result);

        // Calculate prediction error
        prediction_errors[i] = fabsf(predicted - 1.0f);

        // Cleanup
        brain_recovery_free_decision(decision);
        free(diagnosis);
    }

    // Verify prediction improves over time
    // Later predictions should be closer to 1.0 (success)
    EXPECT_LT(prediction_errors[num_recoveries - 1], prediction_errors[0]);
}

//=============================================================================
// Statistics and Reporting Tests
//=============================================================================

TEST_F(BrainRecoveryIntegrationTest, StatisticsTracking) {
    // Execute several recoveries
    for (int i = 0; i < 3; i++) {
        diagnostic_result_t* diagnosis = create_nan_diagnosis();
        health_status_snapshot_t health;
        memset(&health, 0, sizeof(health));

        brain_recovery_decision_t* decision =
            brain_recovery_select_strategy(recovery_ctx, diagnosis, &health);

        recovery_result_t result;
        result.status = (i % 2 == 0) ? RECOVERY_SUCCESS : RECOVERY_FAILED;
        result.tier = RECOVERY_TIER_TACTICAL;
        result.action = RECOVERY_ACTION_REDUCE_LR;

        brain_recovery_learn_outcome(recovery_ctx, decision, &result);

        brain_recovery_free_decision(decision);
        free(diagnosis);
    }

    // Check statistics
    recovery_history_stats_t stats;
    EXPECT_TRUE(brain_recovery_get_stats(recovery_ctx, &stats));

    EXPECT_GT(stats.total_patterns_learned, 0u);
    EXPECT_GT(stats.success_rate, 0.0f);
    EXPECT_LT(stats.success_rate, 1.0f);
}

TEST_F(BrainRecoveryIntegrationTest, PatternRetrieval) {
    // Learn some patterns
    for (int i = 0; i < 3; i++) {
        diagnostic_result_t* diagnosis = create_nan_diagnosis();
        health_status_snapshot_t health;
        memset(&health, 0, sizeof(health));

        brain_recovery_decision_t* decision =
            brain_recovery_select_strategy(recovery_ctx, diagnosis, &health);

        recovery_result_t result;
        result.status = RECOVERY_SUCCESS;
        result.tier = RECOVERY_TIER_TACTICAL;
        result.action = RECOVERY_ACTION_REDUCE_LR;

        brain_recovery_learn_outcome(recovery_ctx, decision, &result);

        brain_recovery_free_decision(decision);
        free(diagnosis);
    }

    // Retrieve patterns
    recovery_pattern_t patterns[10];
    uint32_t count = brain_recovery_get_patterns(recovery_ctx, patterns, 10);

    EXPECT_GT(count, 0u);

    if (count > 0) {
        EXPECT_NE(patterns[0].failure_signature[0], '\0');
        EXPECT_GT(patterns[0].occurrence_count, 0u);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
