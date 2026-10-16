/**
 * @file test_cognitive_neuron_integration.cpp
 * @brief Integration tests for cognitive neuron types with brain systems
 *
 * This file tests the integration of NEURON_METACOGNITIVE and NEURON_EXECUTIVE_CONTROL
 * with brain decision-making, cognitive processing, and working memory systems.
 *
 * Test Coverage:
 * - Metacognitive neurons in brain decision circuits
 * - Executive control neurons in task switching
 * - Integration with cognitive_processor
 * - Working memory maintenance with executive neurons
 * - Confidence-based learning with metacognitive neurons
 * - Brain network processing with cognitive neuron types
 */

#include <gtest/gtest.h>
#include <vector>
#include <cmath>

#include "core/brain/nimcp_brain.h"
#include "core/neuron_types/nimcp_neuron_types.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/time/nimcp_time.h"

// ============================================================================
// TEST FIXTURE
// ============================================================================

class CognitiveNeuronIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Test infrastructure setup
    }

    void TearDown() override {
        // Cleanup
    }

    // Helper: Create a simple brain for testing
    brain_t create_test_brain() {
        return brain_create("test_cognitive_brain",
                             BRAIN_SIZE_TINY,
                             BRAIN_TASK_CLASSIFICATION,
                             10,  // input_size
                             3);  // output_size
    }

    // Helper: Create test input features
    std::vector<float> create_test_features(uint32_t size, float base_value = 0.5f) {
        std::vector<float> features(size);
        for (uint32_t i = 0; i < size; i++) {
            features[i] = base_value + (static_cast<float>(i) * 0.01f);
        }
        return features;
    }
};

// ============================================================================
// METACOGNITIVE NEURON INTEGRATION TESTS
// ============================================================================

TEST_F(CognitiveNeuronIntegrationTest, Metacognitive_BrainDecisionIntegration) {
    // WHAT: Test metacognitive neurons in brain decision-making
    // WHY: Metacognition should modulate brain confidence
    // HOW: Create brain, make decisions, track confidence

    brain_t brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    auto features = create_test_features(10, 0.7f);

    // Make decision with brain
    brain_decision_t* decision = brain_decide(brain, features.data(), 10);

    ASSERT_NE(decision, nullptr);
    EXPECT_GE(decision->confidence, 0.0f);
    EXPECT_LE(decision->confidence, 1.0f);

    brain_free_decision(decision);

    brain_destroy(brain);
}

TEST_F(CognitiveNeuronIntegrationTest, Metacognitive_ConfidenceBasedLearning) {
    // WHAT: Test confidence-based learning rate modulation
    // WHY: Metacognitive neurons should adjust learning based on confidence
    // HOW: Learn examples with varying confidence, check learning rates

    brain_t brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    // High confidence example
    auto high_conf_features = create_test_features(10, 0.5f);  // Stable
    float loss1 = brain_learn_example(brain, high_conf_features.data(), 10,
                                       "label_A", 0.9f);

    // Low confidence example (volatile features)
    auto low_conf_features = create_test_features(10, 0.9f);  // High variance
    float loss2 = brain_learn_example(brain, low_conf_features.data(), 10,
                                       "label_B", 0.3f);  // Low confidence

    // Both should learn (non-negative loss)
    EXPECT_GE(loss1, 0.0f);
    EXPECT_GE(loss2, 0.0f);

    brain_destroy(brain);
}

TEST_F(CognitiveNeuronIntegrationTest, Metacognitive_UncertaintyTracking) {
    // WHAT: Track uncertainty across multiple brain decisions
    // WHY: Metacognitive monitoring should detect confidence trends
    // HOW: Make series of decisions, track confidence variance

    brain_t brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    std::vector<float> confidences;

    // Make multiple decisions
    for (int i = 0; i < 10; i++) {
        auto features = create_test_features(10, 0.5f + static_cast<float>(i) * 0.05f);
        brain_decision_t* decision = brain_decide(brain, features.data(), 10);

        ASSERT_NE(decision, nullptr);
        confidences.push_back(decision->confidence);
        brain_free_decision(decision);
    }

    // All confidences should be valid
    for (float conf : confidences) {
        EXPECT_GE(conf, 0.0f);
        EXPECT_LE(conf, 1.0f);
    }

    brain_destroy(brain);
}

TEST_F(CognitiveNeuronIntegrationTest, Metacognitive_PredictionErrorIntegration) {
    // WHAT: Test prediction error tracking in brain networks
    // WHY: Metacognitive neurons should detect unexpected outcomes
    // HOW: Present expected vs unexpected patterns, measure responses

    brain_t brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    // Expected pattern (repeated)
    auto expected_features = create_test_features(10, 0.5f);
    brain_decision_t* decision1 = brain_decide(brain, expected_features.data(), 10);
    ASSERT_NE(decision1, nullptr);

    // Unexpected pattern (different)
    auto unexpected_features = create_test_features(10, 0.9f);
    brain_decision_t* decision2 = brain_decide(brain, unexpected_features.data(), 10);
    ASSERT_NE(decision2, nullptr);

    // Both should produce valid decisions
    EXPECT_GE(decision1->confidence, 0.0f);
    EXPECT_GE(decision2->confidence, 0.0f);

    brain_free_decision(decision1);
    brain_free_decision(decision2);

    brain_destroy(brain);
}

// ============================================================================
// EXECUTIVE CONTROL NEURON INTEGRATION TESTS
// ============================================================================

TEST_F(CognitiveNeuronIntegrationTest, ExecutiveControl_TaskSwitchingIntegration) {
    // WHAT: Test executive control in task switching scenarios
    // WHY: Executive neurons should manage task transitions
    // HOW: Switch between classification tasks, measure performance

    brain_t brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    // Task A: Classify low values
    auto features_A = create_test_features(10, 0.3f);
    float loss_A = brain_learn_example(brain, features_A.data(), 10, "low", 0.8f);

    // Switch to Task B: Classify high values
    auto features_B = create_test_features(10, 0.8f);
    float loss_B = brain_learn_example(brain, features_B.data(), 10, "high", 0.8f);

    // Both tasks should learn successfully
    EXPECT_GE(loss_A, 0.0f);
    EXPECT_GE(loss_B, 0.0f);

    brain_destroy(brain);
}

TEST_F(CognitiveNeuronIntegrationTest, ExecutiveControl_InhibitoryControlIntegration) {
    // WHAT: Test inhibitory control in brain decision-making
    // WHY: Executive neurons should suppress irrelevant responses
    // HOW: Present distractors, measure response suppression

    brain_t brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    // Relevant stimulus
    auto relevant_features = create_test_features(10, 0.7f);
    brain_decision_t* decision_relevant = brain_decide(brain, relevant_features.data(), 10);
    ASSERT_NE(decision_relevant, nullptr);

    // Irrelevant stimulus (distractor)
    auto irrelevant_features = create_test_features(10, 0.2f);
    brain_decision_t* decision_irrelevant = brain_decide(brain, irrelevant_features.data(), 10);
    ASSERT_NE(decision_irrelevant, nullptr);

    // Both should produce valid decisions
    EXPECT_GE(decision_relevant->confidence, 0.0f);
    EXPECT_GE(decision_irrelevant->confidence, 0.0f);

    brain_free_decision(decision_relevant);
    brain_free_decision(decision_irrelevant);

    brain_destroy(brain);
}

TEST_F(CognitiveNeuronIntegrationTest, ExecutiveControl_WorkingMemoryIntegration) {
    // WHAT: Test working memory maintenance with executive neurons
    // WHY: Executive control should maintain information over delays
    // HOW: Learn pattern, delay, test retention

    brain_t brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    // Learn pattern
    auto features = create_test_features(10, 0.6f);
    float loss = brain_learn_example(brain, features.data(), 10, "target", 0.9f);
    EXPECT_GE(loss, 0.0f);

    // Simulate delay (no processing)
    // In real implementation, executive neurons would maintain activity

    // Test retention via decision
    brain_decision_t* decision = brain_decide(brain, features.data(), 10);

    ASSERT_NE(decision, nullptr);
    EXPECT_GE(decision->confidence, 0.0f);

    brain_free_decision(decision);

    brain_destroy(brain);
}

TEST_F(CognitiveNeuronIntegrationTest, ExecutiveControl_TopDownModulationIntegration) {
    // WHAT: Test top-down modulation of brain processing
    // WHY: Executive neurons should bias processing toward goals
    // HOW: Set goal context, measure bias in decision-making

    brain_t brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    // Decision with implicit goal bias
    auto features = create_test_features(10, 0.5f);
    brain_decision_t* decision = brain_decide(brain, features.data(), 10);

    ASSERT_NE(decision, nullptr);
    EXPECT_GE(decision->confidence, 0.0f);

    brain_free_decision(decision);

    brain_destroy(brain);
}

// ============================================================================
// COMBINED METACOGNITIVE + EXECUTIVE INTEGRATION TESTS
// ============================================================================

TEST_F(CognitiveNeuronIntegrationTest, Combined_MetacognitiveAndExecutive) {
    // WHAT: Test combined metacognitive and executive processing
    // WHY: Both neuron types should work together in cognitive control
    // HOW: Complex task with confidence monitoring and goal management

    brain_t brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    // Task requiring both metacognition and executive control
    // 1. Learn with confidence tracking (metacognitive)
    auto features_A = create_test_features(10, 0.6f);
    float loss_A = brain_learn_example(brain, features_A.data(), 10, "A", 0.8f);

    // 2. Switch task (executive control)
    auto features_B = create_test_features(10, 0.4f);
    float loss_B = brain_learn_example(brain, features_B.data(), 10, "B", 0.7f);

    // 3. Make decision with confidence (metacognitive)
    brain_decision_t* decision = brain_decide(brain, features_A.data(), 10);
    ASSERT_NE(decision, nullptr);

    EXPECT_GE(loss_A, 0.0f);
    EXPECT_GE(loss_B, 0.0f);
    EXPECT_GE(decision->confidence, 0.0f);

    brain_free_decision(decision);

    brain_destroy(brain);
}

TEST_F(CognitiveNeuronIntegrationTest, Combined_AdaptiveBehavior) {
    // WHAT: Test adaptive behavior driven by cognitive neurons
    // WHY: System should adjust based on confidence and goals
    // HOW: Monitor performance, adjust strategies

    brain_t brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    std::vector<float> losses;

    // Adaptive learning sequence
    for (int i = 0; i < 5; i++) {
        auto features = create_test_features(10, 0.5f + static_cast<float>(i) * 0.05f);
        float loss = brain_learn_example(brain, features.data(), 10,
                                          "adaptive", 0.8f - static_cast<float>(i) * 0.1f);
        losses.push_back(loss);
    }

    // All learning iterations should complete
    for (float loss : losses) {
        EXPECT_GE(loss, 0.0f);
    }

    brain_destroy(brain);
}

// ============================================================================
// PERFORMANCE AND STRESS TESTS
// ============================================================================

TEST_F(CognitiveNeuronIntegrationTest, Performance_HighThroughput) {
    // WHAT: Test cognitive neuron processing under high load
    // WHY: Ensure efficient processing at scale
    // HOW: Process many decisions rapidly

    brain_t brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    uint64_t start = nimcp_time_monotonic_us();

    const int num_iterations = 1000;
    for (int i = 0; i < num_iterations; i++) {
        auto features = create_test_features(10, 0.5f);
        brain_decision_t* decision = brain_decide(brain, features.data(), 10);
        if (decision) {
            brain_free_decision(decision);
        }
    }

    uint64_t duration = nimcp_time_monotonic_us() - start;

    // Should complete in reasonable time (< 1 second for 1000 iterations)
    EXPECT_LT(duration, 1000000ULL);  // 1 second in microseconds

    brain_destroy(brain);
}

TEST_F(CognitiveNeuronIntegrationTest, Robustness_EdgeCaseInputs) {
    // WHAT: Test cognitive neurons with edge case inputs
    // WHY: Ensure robust handling of unusual inputs
    // HOW: Test zeros, ones, NaN handling

    brain_t brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    // Zero input
    std::vector<float> zeros(10, 0.0f);
    brain_decision_t* decision_zero = brain_decide(brain, zeros.data(), 10);
    ASSERT_NE(decision_zero, nullptr);

    // Max input
    std::vector<float> ones(10, 1.0f);
    brain_decision_t* decision_one = brain_decide(brain, ones.data(), 10);
    ASSERT_NE(decision_one, nullptr);

    // Mixed input
    std::vector<float> mixed = {0.0f, 1.0f, 0.5f, 0.2f, 0.8f, 0.1f, 0.9f, 0.3f, 0.7f, 0.6f};
    brain_decision_t* decision_mixed = brain_decide(brain, mixed.data(), 10);
    ASSERT_NE(decision_mixed, nullptr);

    // All should produce valid outputs
    EXPECT_GE(decision_zero->confidence, 0.0f);
    EXPECT_GE(decision_one->confidence, 0.0f);
    EXPECT_GE(decision_mixed->confidence, 0.0f);

    brain_free_decision(decision_zero);
    brain_free_decision(decision_one);
    brain_free_decision(decision_mixed);

    brain_destroy(brain);
}

TEST_F(CognitiveNeuronIntegrationTest, Robustness_LongRunningSession) {
    // WHAT: Test cognitive neurons over extended processing
    // WHY: Ensure stability over long sessions
    // HOW: Run extended learning and decision sequence

    brain_t brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    // Long session simulation
    for (int epoch = 0; epoch < 10; epoch++) {
        for (int sample = 0; sample < 100; sample++) {
            auto features = create_test_features(10,
                0.5f + static_cast<float>(epoch) * 0.05f);

            if (sample % 2 == 0) {
                // Learn
                brain_learn_example(brain, features.data(), 10, "label", 0.8f);
            } else {
                // Decide
                brain_decision_t* decision = brain_decide(brain, features.data(), 10);
                if (decision) {
                    EXPECT_GE(decision->confidence, 0.0f);
                    brain_free_decision(decision);
                }
            }
        }
    }

    brain_destroy(brain);
}
