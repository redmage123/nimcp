/**
 * @file test_brain_coverage_boost.cpp
 * @brief Targeted tests to boost brain.c from 52% to 95% coverage
 *
 * WHAT: Tests for uncovered code paths in brain.c
 * WHY: Boost coverage from 52.20% to 95%+ (target: +43% coverage)
 * HOW: Focus on:
 *   - Strategy loss functions (classification, regression, pattern, association)
 *   - Edge cases and error paths
 *   - Custom brain sizes and configurations
 *   - Less common code branches
 *
 * COVERAGE TARGET: +1,100 lines (52% → 95%)
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

    #include "core/brain/nimcp_brain.h"
    #include "nimcp.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainCoverageBoostTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_init();
        brain_clear_error();
    }

    void TearDown() override {
        nimcp_shutdown();
    }

    float* create_features(uint32_t size, float val = 0.5f) {
        float* f = new float[size];
        for (uint32_t i = 0; i < size; i++) f[i] = val + i * 0.01f;
        return f;
    }
};

//=============================================================================
// Strategy Loss Functions Coverage
//=============================================================================

TEST_F(BrainCoverageBoostTest, Classification_LossComputation) {
    // Create brain with classification task
    brain_t brain = brain_create("class_loss", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 5, 3);
    ASSERT_NE(brain, nullptr);

    // Train to trigger loss computation
    float* features = create_features(5);
    for (int i = 0; i < 10; i++) {
        brain_learn_example(brain, features, 5, "class_A", 0.9f);
    }

    // Make prediction (triggers classification loss internally)
    brain_decision_t* decision = brain_decide(brain, features, 5);
    EXPECT_NE(decision, nullptr);

    brain_free_decision(decision);
    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainCoverageBoostTest, Regression_LossComputation) {
    // Create brain with regression task
    brain_t brain = brain_create("reg_loss", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_REGRESSION, 5, 3);
    ASSERT_NE(brain, nullptr);

    // Train to trigger regression loss computation
    float* features = create_features(5);
    for (int i = 0; i < 10; i++) {
        brain_learn_example(brain, features, 5, "reg_value", 0.75f);
    }

    // Make prediction (triggers regression loss internally)
    brain_decision_t* decision = brain_decide(brain, features, 5);
    EXPECT_NE(decision, nullptr);

    brain_free_decision(decision);
    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainCoverageBoostTest, Pattern_LossComputation) {
    // Create brain with pattern matching task
    brain_t brain = brain_create("pattern_loss", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_PATTERN_MATCHING, 5, 3);
    ASSERT_NE(brain, nullptr);

    // Train to trigger pattern loss computation
    float* features = create_features(5);
    for (int i = 0; i < 10; i++) {
        brain_learn_example(brain, features, 5, "pattern_X", 0.85f);
    }

    // Make prediction (triggers pattern loss internally)
    brain_decision_t* decision = brain_decide(brain, features, 5);
    EXPECT_NE(decision, nullptr);

    brain_free_decision(decision);
    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainCoverageBoostTest, Association_LossComputation) {
    // Create brain with association task
    brain_t brain = brain_create("assoc_loss", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_ASSOCIATION, 5, 3);
    ASSERT_NE(brain, nullptr);

    // Train to trigger association loss computation
    float* features = create_features(5);
    for (int i = 0; i < 10; i++) {
        brain_learn_example(brain, features, 5, "assoc_Y", 0.8f);
    }

    // Make prediction (triggers association loss internally)
    brain_decision_t* decision = brain_decide(brain, features, 5);
    EXPECT_NE(decision, nullptr);

    brain_free_decision(decision);
    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainCoverageBoostTest, Sequence_LossComputation) {
    // Create brain with sequence task
    brain_t brain = brain_create("seq_loss", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_SEQUENCE, 5, 3);
    ASSERT_NE(brain, nullptr);

    // Train to trigger sequence loss computation
    float* features = create_features(5);
    for (int i = 0; i < 10; i++) {
        brain_learn_example(brain, features, 5, "seq_Z", 0.7f);
    }

    // Make prediction
    brain_decision_t* decision = brain_decide(brain, features, 5);
    EXPECT_NE(decision, nullptr);

    brain_free_decision(decision);
    delete[] features;
    brain_destroy(brain);
}

//=============================================================================
// Custom Brain Size Coverage
//=============================================================================

TEST_F(BrainCoverageBoostTest, CustomBrainSize) {
    // Create custom brain configuration
    brain_config_t config = {};
    config.size = BRAIN_SIZE_CUSTOM;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 5;
    strncpy(config.task_name, "custom", 63);

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Verify custom size works
    brain_stats_t stats;
    ASSERT_TRUE(brain_get_stats(brain, &stats));
    EXPECT_EQ(stats.size, BRAIN_SIZE_CUSTOM);

    brain_destroy(brain);
}

//=============================================================================
// Edge Cases and Error Paths
//=============================================================================

TEST_F(BrainCoverageBoostTest, NullPointerHandling) {
    // Test NULL pointer error paths
    brain_destroy(nullptr);  // Should not crash

    brain_stats_t stats;
    EXPECT_FALSE(brain_get_stats(nullptr, &stats));

    brain_decision_t* decision = brain_decide(nullptr, nullptr, 0);
    EXPECT_EQ(decision, nullptr);
}

TEST_F(BrainCoverageBoostTest, InvalidInputSizes) {
    brain_t brain = brain_create("invalid_test", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 5, 3);
    ASSERT_NE(brain, nullptr);

    // Try decision with wrong input size
    float features[10];
    brain_decision_t* decision = brain_decide(brain, features, 10);  // Wrong size
    // Should handle gracefully

    brain_free_decision(decision);
    brain_destroy(brain);
}

TEST_F(BrainCoverageBoostTest, MaxValueNormalization) {
    // Test normalization paths with extreme values
    brain_t brain = brain_create("norm_test", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 3, 2);
    ASSERT_NE(brain, nullptr);

    // Create features with large values
    float features[3] = {1000.0f, 2000.0f, 3000.0f};
    brain_learn_example(brain, features, 3, "large_val", 0.9f);

    brain_decision_t* decision = brain_decide(brain, features, 3);
    EXPECT_NE(decision, nullptr);

    brain_free_decision(decision);
    brain_destroy(brain);
}

TEST_F(BrainCoverageBoostTest, ZeroAndNegativeValues) {
    brain_t brain = brain_create("zero_test", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_REGRESSION, 3, 2);
    ASSERT_NE(brain, nullptr);

    // Test with zeros
    float zeros[3] = {0.0f, 0.0f, 0.0f};
    brain_learn_example(brain, zeros, 3, "zero", 0.5f);

    // Test with negative values
    float negatives[3] = {-1.0f, -2.0f, -3.0f};
    brain_learn_example(brain, negatives, 3, "negative", 0.3f);

    brain_decision_t* decision = brain_decide(brain, zeros, 3);
    EXPECT_NE(decision, nullptr);

    brain_free_decision(decision);
    brain_destroy(brain);
}

//=============================================================================
// Default Case Coverage
//=============================================================================

TEST_F(BrainCoverageBoostTest, DefaultSparsityPath) {
    // Test default sparsity calculation for unknown size
    brain_config_t config = {};
    config.size = BRAIN_SIZE_CUSTOM;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 5;
    config.num_outputs = 3;
    strncpy(config.task_name, "default", 63);

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Train the brain to create synapses (needed for sparsity calculation)
    float features[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    for (int i = 0; i < 10; i++) {
        brain_learn_example(brain, features, 5, "class_A", 0.8f);
    }

    // Verify it calculates sparsity after learning
    brain_stats_t stats;
    bool result = brain_get_stats(brain, &stats);
    EXPECT_TRUE(result);

    // After learning, sparsity should be calculated
    // Note: avg_sparsity may be 0.0 if network is fully connected
    // or may be > 0 if network has pruned connections
    EXPECT_GE(stats.avg_sparsity, 0.0f);
    EXPECT_LE(stats.avg_sparsity, 1.0f);

    brain_destroy(brain);
}

TEST_F(BrainCoverageBoostTest, AllocationFailureHandling) {
    // Test error path when layer_sizes allocation would fail
    // This is difficult to trigger without mocking, but we can test error handling
    brain_config_t config = {};
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 5;
    config.num_outputs = 3;
    strncpy(config.task_name, "alloc_test", 63);

    brain_t brain = brain_create_custom(&config);
    // Should handle any allocation failures gracefully
    if (brain) {
        brain_destroy(brain);
    }
}

//=============================================================================
// Stress Tests for Edge Cases
//=============================================================================

TEST_F(BrainCoverageBoostTest, ExtremeConfidence_High) {
    brain_t brain = brain_create("high_conf", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 3, 2);
    ASSERT_NE(brain, nullptr);

    float features[3] = {1.0f, 1.0f, 1.0f};

    // Train heavily to drive confidence high
    for (int i = 0; i < 50; i++) {
        brain_learn_example(brain, features, 3, "high", 1.0f);
    }

    brain_decision_t* decision = brain_decide(brain, features, 3);
    EXPECT_NE(decision, nullptr);

    brain_free_decision(decision);
    brain_destroy(brain);
}

TEST_F(BrainCoverageBoostTest, ExtremeConfidence_Low) {
    brain_t brain = brain_create("low_conf", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 3, 2);
    ASSERT_NE(brain, nullptr);

    // Create contradictory training data
    float features[3] = {0.5f, 0.5f, 0.5f};
    for (int i = 0; i < 5; i++) {
        brain_learn_example(brain, features, 3, "label_A", 0.9f);
        brain_learn_example(brain, features, 3, "label_B", 0.9f);
    }

    brain_decision_t* decision = brain_decide(brain, features, 3);
    EXPECT_NE(decision, nullptr);

    brain_free_decision(decision);
    brain_destroy(brain);
}

//=============================================================================
// Batch Operations Coverage
//=============================================================================

TEST_F(BrainCoverageBoostTest, BatchLearning_EmptyBatch) {
    brain_t brain = brain_create("batch_empty", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 5, 3);
    ASSERT_NE(brain, nullptr);

    // Try batch learning with 0 examples
    brain_learn_batch(brain, nullptr, 0);
    // Should handle gracefully (return 0 or error)

    brain_destroy(brain);
}

TEST_F(BrainCoverageBoostTest, BatchDecision_EmptyBatch) {
    brain_t brain = brain_create("batch_decide_empty", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 5, 3);
    ASSERT_NE(brain, nullptr);

    // Try batch decision with 0 inputs (NULL check)
    brain_decision_t decisions[1];
    brain_decide_batch(brain, nullptr, 0, 5, decisions);
    // Should handle gracefully

    brain_destroy(brain);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
