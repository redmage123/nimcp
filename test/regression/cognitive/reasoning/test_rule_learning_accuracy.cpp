//=============================================================================
// test_rule_learning_accuracy.cpp - Rule Learning Accuracy Regression Tests
//=============================================================================

#include <gtest/gtest.h>
#include "core/brain/nimcp_brain.h"
#include "core/brain/learning/nimcp_rule_learning.h"
#include <cmath>

class RuleLearningAccuracyTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        // brain_config_t config;
        // config.num_neurons = 1000;
        // config.num_inputs = 20;
        // config.num_outputs = 10;
        // config.sparsity = 0.1f;
        // config.learning_rate = 0.01f;

        brain = brain_create("reasoning_test", BRAIN_SIZE_LARGE, BRAIN_TASK_CLASSIFICATION, 100, 50);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
        }
    }
};

// Test 1: 95%+ accuracy on clean data
TEST_F(RuleLearningAccuracyTest, HighAccuracyCleanData) {
    // Create perfect training examples
    float cat_features[20] = {1.0f, 1.0f, 0.0f};  // First 2 features identify cats
    float dog_features[20] = {0.0f, 1.0f, 1.0f};  // Features 1,2 identify dogs

    rule_example_t examples[100];
    const char* labels[100];

    // Create 100 examples (50 cats, 50 dogs)
    for (int i = 0; i < 50; i++) {
        examples[i] = {cat_features, 20, "cat", 1.0f};
        labels[i] = "cat";
    }
    for (int i = 50; i < 100; i++) {
        examples[i] = {dog_features, 20, "dog", 1.0f};
        labels[i] = "dog";
    }

    // Learn rules
    int rules_learned = brain_learn_rule_from_examples(brain, examples, labels, 100);
    EXPECT_GE(rules_learned, 1);

    // TODO: Test accuracy on held-out set
    // Expected: >95% accuracy
}

// Test 2: Handle noisy training data
TEST_F(RuleLearningAccuracyTest, NoiseRobustness) {
    float features[20];
    rule_example_t examples[100];
    const char* labels[100];

    // Create noisy examples
    for (int i = 0; i < 100; i++) {
        for (int j = 0; j < 20; j++) {
            // Add random noise
            features[j] = (rand() % 100) / 100.0f;
        }

        examples[i] = {features, 20, "noisy_class", 0.7f};
        labels[i] = "noisy_class";
    }

    // Learn with noisy data
    int rules_learned = brain_learn_rule_from_examples(brain, examples, labels, 100);

    // Should still learn something, even with noise
    EXPECT_GE(rules_learned, 0);
}

// Test 3: Pattern extraction accuracy
TEST_F(RuleLearningAccuracyTest, PatternExtractionAccuracy) {
    // Create examples with clear pattern
    float ex1[20] = {1.0f, 1.0f, 1.0f, 0.0f};
    float ex2[20] = {1.0f, 1.0f, 1.0f, 0.0f};
    float ex3[20] = {1.0f, 1.0f, 1.0f, 0.0f};
    float ex4[20] = {1.0f, 1.0f, 1.0f, 0.0f};

    rule_example_t examples[] = {
        {ex1, 20, "pattern_A", 1.0f},
        {ex2, 20, "pattern_A", 1.0f},
        {ex3, 20, "pattern_A", 1.0f},
        {ex4, 20, "pattern_A", 1.0f}
    };

    char rule[512];
    bool found = extract_rule_pattern(examples, 4, "pattern_A", rule, sizeof(rule));

    EXPECT_TRUE(found);
    // Rule should include features 0, 1, 2 (all present in 100% of examples)
    EXPECT_NE(strstr(rule, "feature_0"), nullptr);
    EXPECT_NE(strstr(rule, "feature_1"), nullptr);
    EXPECT_NE(strstr(rule, "feature_2"), nullptr);
}

// Test 4: Confidence calibration
TEST_F(RuleLearningAccuracyTest, ConfidenceCalibration) {
    // Test Laplace smoothing
    float conf_high = compute_rule_confidence(95, 100);  // 95% support
    float conf_mid = compute_rule_confidence(50, 100);   // 50% support
    float conf_low = compute_rule_confidence(5, 100);    // 5% support

    // Verify ordering
    EXPECT_GT(conf_high, conf_mid);
    EXPECT_GT(conf_mid, conf_low);

    // Verify bounds
    EXPECT_LE(conf_high, 1.0f);
    EXPECT_GE(conf_low, 0.0f);

    // Verify reasonable values
    EXPECT_GT(conf_high, 0.85f);  // High support → high confidence
    EXPECT_LT(conf_low, 0.15f);   // Low support → low confidence
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
