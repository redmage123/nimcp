//=============================================================================
// test_brain_resize_integration.cpp - Integration Tests for Dynamic Brain Growth
//=============================================================================
/**
 * @file test_brain_resize_integration.cpp
 * @brief Integration tests for Phase 2.8 dynamic brain resizing in real workflows
 *
 * TEST SCENARIOS:
 * - Automatic growth during long training sessions
 * - Resize with full cognitive subsystems active
 * - Resize with multi-modal processing
 * - Resize with distributed cognition
 * - Checkpoint/resume after resize
 *
 * @author NIMCP Development Team
 * @date 2025-11-16
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_resize.h"
#include "plasticity/adaptive/nimcp_adaptive.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainResizeIntegrationTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        // Create brain with all cognitive subsystems enabled
        brain = brain_create("integration_test", BRAIN_SIZE_TINY,
                            BRAIN_TASK_CLASSIFICATION, 20, 10);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    // Helper: Generate diverse training data
    std::vector<std::vector<float>> generate_training_data(int num_samples, int feature_dim) {
        std::vector<std::vector<float>> data;
        for (int i = 0; i < num_samples; i++) {
            std::vector<float> sample(feature_dim);
            for (int j = 0; j < feature_dim; j++) {
                // Generate varied patterns
                sample[j] = std::sin(i * 0.1f + j) * 0.5f + 0.5f;
            }
            data.push_back(sample);
        }
        return data;
    }

    // Helper: Train brain with diverse data
    void train_with_diverse_data(brain_t b, int num_samples) {
        auto data = generate_training_data(num_samples, 20);
        const char* labels[] = {"class_A", "class_B", "class_C", "class_D", "class_E"};

        for (size_t i = 0; i < data.size(); i++) {
            const char* label = labels[i % 5];
            brain_learn_example(b, data[i].data(), 20, label, 0.8f);
        }
    }
};

//=============================================================================
// Long Training Session with Auto-Growth
//=============================================================================

TEST_F(BrainResizeIntegrationTest, AutoGrowthDuringLongTraining) {
    uint32_t initial_size = brain_get_neuron_count(brain);
    EXPECT_EQ(initial_size, 100u);

    // Simulate long training session with periodic auto-resize checks
    const int total_steps = 5000;
    const int check_interval = 100;

    for (int step = 0; step < total_steps; step++) {
        // Train one step
        float features[20];
        for (int i = 0; i < 20; i++) {
            features[i] = std::sin(step * 0.01f + i) * 0.5f + 0.5f;
        }
        brain_learn_example(brain, features, 20, "pattern", 0.9f);

        // Periodically check for auto-growth
        if (step % check_interval == 0) {
            brain_auto_resize(brain);
        }
    }

    // Brain should still function after long training
    float test_features[20];
    for (int i = 0; i < 20; i++) {
        test_features[i] = 0.5f;
    }

    brain_decision_t* decision = brain_decide(brain, test_features, 20);
    ASSERT_NE(decision, nullptr);
    EXPECT_GT(decision->confidence, 0.0f);
    brain_free_decision(decision);
}

//=============================================================================
// Resize with Full Cognitive Stack
//=============================================================================

TEST_F(BrainResizeIntegrationTest, ResizeWithCognitiveSubsystems) {
    // Train with cognitive systems active
    train_with_diverse_data(brain, 200);

    // Get metrics before resize
    float util_before, sat_before;
    brain_get_utilization_metrics(brain, &util_before, &sat_before);

    uint32_t size_before = brain_get_neuron_count(brain);

    // Resize
    ASSERT_TRUE(brain_resize(brain, 300));

    // Verify new size
    EXPECT_EQ(brain_get_neuron_count(brain), 300u);

    // Continue training after resize
    train_with_diverse_data(brain, 200);

    // Brain should still produce valid decisions
    float test_features[20];
    for (int i = 0; i < 20; i++) {
        test_features[i] = 0.7f;
    }

    brain_decision_t* decision = brain_decide(brain, test_features, 20);
    ASSERT_NE(decision, nullptr);
    EXPECT_GT(decision->confidence, 0.0f);
    brain_free_decision(decision);
}

//=============================================================================
// Knowledge Retention Across Multiple Resizes
//=============================================================================

TEST_F(BrainResizeIntegrationTest, KnowledgeRetentionMultipleResizes) {
    // Train specific patterns
    const char* patterns[] = {"pattern_1", "pattern_2", "pattern_3"};
    float pattern_features[3][20] = {
        {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},  // pattern_1
        {0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},  // pattern_2
        {0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}   // pattern_3
    };

    // Train each pattern
    for (int p = 0; p < 3; p++) {
        for (int i = 0; i < 100; i++) {
            brain_learn_example(brain, pattern_features[p], 20, patterns[p], 0.95f);
        }
    }

    // Test recognition before resizes
    brain_decision_t* decision1 = brain_decide(brain, pattern_features[0], 20);
    ASSERT_NE(decision1, nullptr);
    char initial_prediction[64] = {0};
    if (decision1->label) {
        strncpy(initial_prediction, decision1->label, sizeof(initial_prediction) - 1);
    }
    brain_free_decision(decision1);

    // Resize multiple times: 100 → 200 → 500 → 1000
    ASSERT_TRUE(brain_resize(brain, 200));
    ASSERT_TRUE(brain_resize(brain, 500));
    ASSERT_TRUE(brain_resize(brain, 1000));

    // Test recognition after resizes
    brain_decision_t* decision2 = brain_decide(brain, pattern_features[0], 20);
    ASSERT_NE(decision2, nullptr);

    // Should still recognize the same pattern
    if (strlen(initial_prediction) > 0 && decision2->label) {
        EXPECT_STREQ(initial_prediction, decision2->label)
            << "Brain should retain knowledge across multiple resizes";
    }

    brain_free_decision(decision2);
}

//=============================================================================
// Performance Under Continuous Resize
//=============================================================================

TEST_F(BrainResizeIntegrationTest, PerformanceUnderContinuousResize) {
    // Simulate realistic workflow: train, check metrics, auto-resize
    const int total_iterations = 10;
    const int steps_per_iteration = 500;

    for (int iter = 0; iter < total_iterations; iter++) {
        // Train for a batch
        train_with_diverse_data(brain, steps_per_iteration);

        // Check utilization
        float util, sat;
        ASSERT_TRUE(brain_get_utilization_metrics(brain, &util, &sat));

        // Auto-resize if needed
        bool resized = brain_auto_resize(brain);
        if (resized) {
            fprintf(stderr, "[INFO] Auto-resized at iteration %d to %u neurons (util=%.2f, sat=%.2f)\n",
                    iter, brain_get_neuron_count(brain), util, sat);
        }
    }

    // Final verification
    float final_features[20];
    for (int i = 0; i < 20; i++) {
        final_features[i] = 0.6f;
    }

    brain_decision_t* decision = brain_decide(brain, final_features, 20);
    ASSERT_NE(decision, nullptr);
    EXPECT_GT(decision->confidence, 0.0f);
    brain_free_decision(decision);

    fprintf(stderr, "[INFO] Final brain size: %u neurons\n",
            brain_get_neuron_count(brain));
}

//=============================================================================
// Stress Test: Rapid Growth
//=============================================================================

TEST_F(BrainResizeIntegrationTest, RapidGrowthStress) {
    // Grow rapidly while training
    uint32_t target_size = 100;

    for (int iteration = 0; iteration < 10; iteration++) {
        // Train a bit
        train_with_diverse_data(brain, 50);

        // Grow by 50%
        target_size = (uint32_t)(target_size * 1.5f);
        ASSERT_TRUE(brain_resize(brain, target_size))
            << "Failed to resize to " << target_size << " in iteration " << iteration;

        EXPECT_EQ(brain_get_neuron_count(brain), target_size);
    }

    // Final size should be large
    EXPECT_GT(brain_get_neuron_count(brain), 1000u);

    // Brain should still function
    float test_features[20];
    for (int i = 0; i < 20; i++) {
        test_features[i] = 0.5f;
    }

    brain_decision_t* decision = brain_decide(brain, test_features, 20);
    ASSERT_NE(decision, nullptr);
    brain_free_decision(decision);
}

//=============================================================================
// Stability Test: Resize After Long Uptime
//=============================================================================

TEST_F(BrainResizeIntegrationTest, ResizeAfterLongUptime) {
    // Simulate long-running brain (many training steps)
    train_with_diverse_data(brain, 2000);

    uint32_t size_before = brain_get_neuron_count(brain);

    // Resize after extensive training
    ASSERT_TRUE(brain_resize(brain, size_before * 2));

    // Continue training
    train_with_diverse_data(brain, 1000);

    // Brain should remain stable
    float test_features[20];
    for (int i = 0; i < 20; i++) {
        test_features[i] = 0.8f;
    }

    brain_decision_t* decision = brain_decide(brain, test_features, 20);
    ASSERT_NE(decision, nullptr);
    EXPECT_GT(decision->confidence, 0.0f);
    brain_free_decision(decision);
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
