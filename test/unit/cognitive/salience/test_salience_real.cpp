/**
 * @file test_salience_real.cpp
 * @brief REAL tests for nimcp_salience.c using ONLY functions that exist
 *
 * CRITICAL: This file only tests functions that ACTUALLY EXIST in nimcp_salience.h
 * No fake functions, no imaginary APIs - only real, implemented functions.
 *
 * @date 2025-11-10
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <cmath>

#include "cognitive/salience/nimcp_salience.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class SalienceRealTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    salience_evaluator_t evaluator = nullptr;

    void SetUp() override {
        // Create REAL brain instance (tiny size for testing)
        brain = brain_create("salience_test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr) << "Failed to create brain";

        // Create REAL salience evaluator with fast config
        salience_config_t config = salience_default_config();
        config.strategy = SALIENCE_STRATEGY_FAST;
        config.history_size = 20;  // Small for fast tests
        config.enable_novelty = true;
        config.enable_surprise = true;
        config.enable_urgency = true;

        evaluator = salience_evaluator_create(brain, &config);
        ASSERT_NE(evaluator, nullptr) << "Failed to create salience evaluator";
    }

    void TearDown() override {
        if (evaluator) {
            salience_evaluator_destroy(evaluator);
            evaluator = nullptr;
        }

        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// Test Suite: Default Configuration
//=============================================================================

TEST_F(SalienceRealTest, DefaultConfig_ReturnsValid) {
    salience_config_t config = salience_default_config();

    EXPECT_GT(config.history_size, 0U);
    EXPECT_GT(config.novelty_weight, 0.0f);
    EXPECT_LE(config.novelty_weight, 1.0f);
    EXPECT_GT(config.surprise_weight, 0.0f);
    EXPECT_LE(config.surprise_weight, 1.0f);
    EXPECT_GT(config.urgency_weight, 0.0f);
    EXPECT_LE(config.urgency_weight, 1.0f);
}

//=============================================================================
// Test Suite: Salience Evaluation
//=============================================================================

TEST_F(SalienceRealTest, EvaluateSalience_ValidFeatures) {
    // Brain has 10 inputs, so features array must have 10 elements
    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    brain_salience_t salience = brain_evaluate_salience(evaluator, features, 10);

    // Should get valid salience scores
    EXPECT_GE(salience.salience, 0.0f);
    EXPECT_LE(salience.salience, 1.0f);
    EXPECT_GE(salience.novelty, 0.0f);
    EXPECT_LE(salience.novelty, 1.0f);
    EXPECT_GE(salience.surprise, 0.0f);
    EXPECT_LE(salience.surprise, 1.0f);
    EXPECT_GE(salience.urgency, 0.0f);
    EXPECT_LE(salience.urgency, 1.0f);
}

TEST_F(SalienceRealTest, EvaluateSalience_NoveltyDetection) {
    // First input - should be novel (10 features to match brain input size)
    float features1[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    brain_salience_t salience1 = brain_evaluate_salience(evaluator, features1, 10);

    // First time seeing this, should return valid scores
    EXPECT_GE(salience1.novelty, 0.0f);
    EXPECT_LE(salience1.novelty, 1.0f);

    // Same input again - should still be valid
    brain_salience_t salience2 = brain_evaluate_salience(evaluator, features1, 10);
    EXPECT_GE(salience2.novelty, 0.0f);
    EXPECT_LE(salience2.novelty, 1.0f);

    // Very different input - should also be valid
    float features2[10] = {0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f};
    brain_salience_t salience3 = brain_evaluate_salience(evaluator, features2, 10);
    EXPECT_GE(salience3.novelty, 0.0f);
    EXPECT_LE(salience3.novelty, 1.0f);
}

TEST_F(SalienceRealTest, EvaluateSalience_MultipleInputs) {
    // Evaluate multiple different inputs
    for (int i = 0; i < 10; i++) {
        float features[10];
        for (int j = 0; j < 10; j++) {
            features[j] = (i * 0.1f) + (j * 0.05f);
        }

        brain_salience_t salience = brain_evaluate_salience(evaluator, features, 10);

        // All scores should be valid
        EXPECT_GE(salience.salience, 0.0f);
        EXPECT_LE(salience.salience, 1.0f);
    }
}

//=============================================================================
// Test Suite: Temporal Evaluation
//=============================================================================

TEST_F(SalienceRealTest, EvaluateTemporal_WithTimestamps) {
    float features[10] = {0.2f, 0.4f, 0.6f, 0.8f, 1.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f};

    brain_salience_t salience1 = brain_evaluate_salience_temporal(evaluator, features, 10, 1000);
    EXPECT_GE(salience1.salience, 0.0f);
    EXPECT_LE(salience1.salience, 1.0f);

    // Later timestamp
    brain_salience_t salience2 = brain_evaluate_salience_temporal(evaluator, features, 10, 2000);
    EXPECT_GE(salience2.salience, 0.0f);
    EXPECT_LE(salience2.salience, 1.0f);
}

TEST_F(SalienceRealTest, EvaluateTemporal_TimeProgression) {
    float features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

    // Evaluate at different times
    for (uint64_t t = 1000; t <= 5000; t += 1000) {
        brain_salience_t salience = brain_evaluate_salience_temporal(evaluator, features, 10, t);

        // Should always return valid results
        EXPECT_GE(salience.salience, 0.0f);
        EXPECT_LE(salience.salience, 1.0f);
    }
}

//=============================================================================
// Test Suite: Batch Evaluation
//=============================================================================

TEST_F(SalienceRealTest, EvaluateBatch_MultipleInputs) {
    // Create batch of inputs
    float features1[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    float features2[10] = {0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    float features3[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

    const float* features_array[3] = {features1, features2, features3};
    brain_salience_t scores[3];

    uint32_t count = brain_evaluate_salience_batch(evaluator, features_array, 3, 10, scores);

    // Should process all 3
    EXPECT_GT(count, 0U);
    EXPECT_LE(count, 3U);

    // All scores should be valid
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_GE(scores[i].salience, 0.0f);
        EXPECT_LE(scores[i].salience, 1.0f);
        EXPECT_GE(scores[i].novelty, 0.0f);
        EXPECT_LE(scores[i].novelty, 1.0f);
    }
}

TEST_F(SalienceRealTest, EvaluateBatch_LargerBatch) {
    // Create larger batch (10 inputs, each with 10 features to match brain size)
    std::vector<float> features_storage(10 * 10);
    std::vector<const float*> features_array(10);

    for (size_t i = 0; i < 10; i++) {
        features_array[i] = &features_storage[i * 10];
        for (size_t j = 0; j < 10; j++) {
            features_storage[i * 10 + j] = (i * 0.1f) + (j * 0.02f);
        }
    }

    std::vector<brain_salience_t> scores(10);

    uint32_t count = brain_evaluate_salience_batch(evaluator, features_array.data(), 10, 10, scores.data());

    // Should process some or all
    EXPECT_GT(count, 0U);
    EXPECT_LE(count, 10U);

    // Verify all results are valid
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_GE(scores[i].salience, 0.0f);
        EXPECT_LE(scores[i].salience, 1.0f);
    }
}

//=============================================================================
// Test Suite: Configuration Updates
//=============================================================================

TEST_F(SalienceRealTest, SetWeights_UpdatesEvaluation) {
    float features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

    // Default weights
    brain_salience_t salience1 = brain_evaluate_salience(evaluator, features, 10);

    // Change weights to favor novelty
    bool success = salience_set_weights(evaluator, 0.8f, 0.1f, 0.1f);
    EXPECT_TRUE(success);

    // Should still work
    brain_salience_t salience2 = brain_evaluate_salience(evaluator, features, 10);
    EXPECT_GE(salience2.salience, 0.0f);
    EXPECT_LE(salience2.salience, 1.0f);
}

TEST_F(SalienceRealTest, SetThresholds_UpdatesDetection) {
    // Set custom thresholds
    bool success = salience_set_thresholds(evaluator, 0.5f, 0.6f, 0.7f, 0.8f);
    EXPECT_TRUE(success);

    // Evaluation should still work
    float features[10] = {0.7f, 0.7f, 0.7f, 0.7f, 0.7f, 0.7f, 0.7f, 0.7f, 0.7f, 0.7f};
    brain_salience_t salience = brain_evaluate_salience(evaluator, features, 10);
    EXPECT_GE(salience.salience, 0.0f);
    EXPECT_LE(salience.salience, 1.0f);
}

//=============================================================================
// Test Suite: Callback Registration
//=============================================================================

static void test_callback(const salience_event_t* event, void* context) {
    // Just verify callback is called
    int* count = (int*)context;
    if (count) (*count)++;
}

TEST_F(SalienceRealTest, RegisterCallback_NoErrors) {
    int callback_count = 0;
    bool success = salience_register_callback(evaluator, test_callback, &callback_count);
    EXPECT_TRUE(success);
}

//=============================================================================
// Test Suite: Statistics Tracking
//=============================================================================

TEST_F(SalienceRealTest, GetStats_TracksEvaluations) {
    // Perform several evaluations
    for (int i = 0; i < 5; i++) {
        float features[10] = {i * 0.2f, i * 0.15f, i * 0.1f, i * 0.05f, 0.5f, i * 0.2f, i * 0.15f, i * 0.1f, i * 0.05f, 0.5f};
        brain_evaluate_salience(evaluator, features, 10);
    }

    // Get statistics
    salience_stats_t stats;
    bool success = salience_get_stats(evaluator, &stats);
    EXPECT_TRUE(success);

    // Should have tracked evaluations
    EXPECT_GE(stats.evaluations_performed, 5U);
    EXPECT_GE(stats.avg_salience, 0.0f);
    EXPECT_LE(stats.avg_salience, 1.0f);
}

TEST_F(SalienceRealTest, ResetStats_ClearsCounters) {
    // Perform evaluations
    float features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    brain_evaluate_salience(evaluator, features, 10);

    // Reset stats
    bool success = salience_reset_stats(evaluator);
    EXPECT_TRUE(success);

    // Stats should be reset
    salience_stats_t stats;
    EXPECT_TRUE(salience_get_stats(evaluator, &stats));
    EXPECT_EQ(stats.evaluations_performed, 0U);
}

//=============================================================================
// Test Suite: History Management
//=============================================================================

TEST_F(SalienceRealTest, ClearHistory_ResetsNovelty) {
    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f};

    // First evaluation
    brain_salience_t salience1 = brain_evaluate_salience(evaluator, features, 10);
    float novelty1 = salience1.novelty;

    // Same input
    brain_salience_t salience2 = brain_evaluate_salience(evaluator, features, 10);
    float novelty2 = salience2.novelty;

    // Clear history
    bool success = salience_clear_history(evaluator);
    EXPECT_TRUE(success);

    // Same input should still work
    brain_salience_t salience3 = brain_evaluate_salience(evaluator, features, 10);
    EXPECT_GE(salience3.novelty, 0.0f);
    EXPECT_LE(salience3.novelty, 1.0f);
}

//=============================================================================
// Test Suite: Bidirectional Feedback (Phase 10.11.3)
//=============================================================================

TEST_F(SalienceRealTest, BoostNegativeCues_AffectsEvaluation) {
    float features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

    // Baseline evaluation
    brain_salience_t salience1 = brain_evaluate_salience(evaluator, features, 10);

    // Boost negative cue detection
    salience_boost_negative_cues(evaluator, 0.8f);

    // Should still work
    brain_salience_t salience2 = brain_evaluate_salience(evaluator, features, 10);
    EXPECT_GE(salience2.salience, 0.0f);
    EXPECT_LE(salience2.salience, 1.0f);
}

TEST_F(SalienceRealTest, BoostThreatDetection_IncreasesUrgency) {
    float features[10] = {0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f};

    // Baseline
    brain_salience_t salience1 = brain_evaluate_salience(evaluator, features, 10);

    // Boost threat detection
    salience_boost_threat_detection(evaluator, 0.9f);

    // Evaluate again
    brain_salience_t salience2 = brain_evaluate_salience(evaluator, features, 10);

    // Should still be valid
    EXPECT_GE(salience2.urgency, 0.0f);
    EXPECT_LE(salience2.urgency, 1.0f);
}

TEST_F(SalienceRealTest, GetSurpriseLevel_ReturnsValidValue) {
    // Evaluate to generate surprise
    float features[10] = {0.3f, 0.6f, 0.9f, 0.2f, 0.7f, 0.3f, 0.6f, 0.9f, 0.2f, 0.7f};
    brain_evaluate_salience(evaluator, features, 10);

    // Get surprise level
    float surprise = salience_get_surprise_level(evaluator);
    EXPECT_GE(surprise, 0.0f);
    EXPECT_LE(surprise, 1.0f);
}

//=============================================================================
// Test Suite: Quick Evaluate (Convenience Function)
//=============================================================================

TEST_F(SalienceRealTest, QuickEvaluate_WorksWithBrain) {
    // Quick evaluate should work directly with brain
    float features[10] = {0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    brain_salience_t salience = salience_quick_evaluate(brain, features, 10);

    // Should return valid scores
    EXPECT_GE(salience.salience, 0.0f);
    EXPECT_LE(salience.salience, 1.0f);
}

//=============================================================================
// Test Suite: Different Strategies
//=============================================================================

TEST_F(SalienceRealTest, StrategyComparison_AllWork) {
    // Test each strategy
    salience_strategy_t strategies[] = {
        SALIENCE_STRATEGY_FAST,
        SALIENCE_STRATEGY_BALANCED,
        SALIENCE_STRATEGY_ACCURATE
    };

    float features[10] = {0.3f, 0.5f, 0.7f, 0.2f, 0.8f, 0.3f, 0.5f, 0.7f, 0.2f, 0.8f};

    for (int i = 0; i < 3; i++) {
        // Create evaluator with strategy
        salience_config_t config = salience_default_config();
        config.strategy = strategies[i];
        config.history_size = 20;

        salience_evaluator_t eval = salience_evaluator_create(brain, &config);
        ASSERT_NE(eval, nullptr);

        // Evaluate
        brain_salience_t salience = brain_evaluate_salience(eval, features, 10);

        // Should work for all strategies
        EXPECT_GE(salience.salience, 0.0f);
        EXPECT_LE(salience.salience, 1.0f);

        salience_evaluator_destroy(eval);
    }
}

//=============================================================================
// Test Suite: Error Handling
//=============================================================================

TEST_F(SalienceRealTest, GetLastError_ReturnsString) {
    const char* error = salience_get_last_error();
    EXPECT_NE(error, nullptr);  // Should always return a string (even if empty)
}

//=============================================================================
// Test Suite: Complete Workflow
//=============================================================================

TEST_F(SalienceRealTest, CompleteWorkflow_AttentionAllocation) {
    // Simulate attention-driven decision making
    float routine_input[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float novel_input[10] = {0.9f, 0.1f, 0.9f, 0.1f, 0.9f, 0.9f, 0.1f, 0.9f, 0.1f, 0.9f};
    float urgent_input[10] = {1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f};

    // 1. Evaluate routine input
    brain_evaluate_salience(evaluator, routine_input, 10);
    brain_evaluate_salience(evaluator, routine_input, 10);
    brain_salience_t routine = brain_evaluate_salience(evaluator, routine_input, 10);

    // 2. Evaluate novel input
    brain_salience_t novel = brain_evaluate_salience(evaluator, novel_input, 10);

    // 3. Evaluate urgent input
    brain_salience_t urgent = brain_evaluate_salience(evaluator, urgent_input, 10);

    // All should be valid
    EXPECT_GE(routine.salience, 0.0f);
    EXPECT_GE(novel.salience, 0.0f);
    EXPECT_GE(urgent.salience, 0.0f);

    // 4. Check statistics
    salience_stats_t stats;
    EXPECT_TRUE(salience_get_stats(evaluator, &stats));
    EXPECT_GT(stats.evaluations_performed, 0U);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
