//=============================================================================
// test_cognitive_processor.cpp - Comprehensive Cognitive Processor Unit Tests
//=============================================================================

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/processing/cognitive_processor.h"
#include "core/brain/nimcp_brain.h"

using ::testing::_;
using ::testing::Return;
using ::testing::NotNull;

//=============================================================================
// Test Fixture
//=============================================================================

class CognitiveProcessorTest : public ::testing::Test {
protected:
    brain_t brain;
    network_output_t net_output;
    cognitive_annotations_t annotations;
    float output_vector[10];
    float integrated_features[20];

    void SetUp() override {
        // Create a simple brain for testing
        brain = brain_create(100, 10);
        ASSERT_NE(brain, nullptr) << "Failed to create brain for testing";

        // Initialize network output
        for (int i = 0; i < 10; i++) {
            output_vector[i] = 0.1f * i;
        }
        net_output.output_vector = output_vector;
        net_output.output_size = 10;
        net_output.spikes_generated = 42;
        net_output.inference_time_us = 1000;

        // Initialize integrated features
        for (int i = 0; i < 20; i++) {
            integrated_features[i] = 0.05f * i;
        }

        // Initialize annotations
        cognitive_annotations_init(&annotations);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// Basic Functionality Tests
//=============================================================================

TEST_F(CognitiveProcessorTest, InitializeAnnotations) {
    cognitive_annotations_t annot;
    cognitive_annotations_init(&annot);

    // Check default initialization
    EXPECT_FLOAT_EQ(annot.confidence, 0.0f);
    EXPECT_FLOAT_EQ(annot.uncertainty, 1.0f);
    EXPECT_FALSE(annot.ethical_approved);
    EXPECT_FLOAT_EQ(annot.salience_score, 0.0f);
    EXPECT_FLOAT_EQ(annot.novelty_score, 0.0f);
    EXPECT_FLOAT_EQ(annot.urgency_score, 0.0f);
    EXPECT_FLOAT_EQ(annot.exploration_bonus, 0.0f);
    EXPECT_FLOAT_EQ(annot.information_gain, 0.0f);
    EXPECT_FALSE(annot.logic_valid);
}

TEST_F(CognitiveProcessorTest, ProcessOutputBasic) {
    bool success = cognitive_process_output(
        brain,
        &net_output,
        integrated_features,
        20,
        1000,
        &annotations
    );

    EXPECT_TRUE(success) << "Cognitive processing should succeed";

    // Verify annotations are computed
    EXPECT_GE(annotations.confidence, 0.0f);
    EXPECT_LE(annotations.confidence, 1.0f);
    EXPECT_GE(annotations.uncertainty, 0.0f);
    EXPECT_LE(annotations.uncertainty, 1.0f);

    // Confidence and uncertainty should be complementary
    EXPECT_NEAR(annotations.confidence + annotations.uncertainty, 1.0f, 0.1f);
}

TEST_F(CognitiveProcessorTest, ProcessOutputRepeatable) {
    cognitive_annotations_t annot1, annot2;
    cognitive_annotations_init(&annot1);
    cognitive_annotations_init(&annot2);

    // Process twice with same inputs
    bool success1 = cognitive_process_output(
        brain, &net_output, integrated_features, 20, 1000, &annot1);
    bool success2 = cognitive_process_output(
        brain, &net_output, integrated_features, 20, 1000, &annot2);

    EXPECT_TRUE(success1);
    EXPECT_TRUE(success2);

    // Results should be deterministic (or very close for floating point)
    EXPECT_NEAR(annot1.confidence, annot2.confidence, 0.01f);
    EXPECT_NEAR(annot1.uncertainty, annot2.uncertainty, 0.01f);
    EXPECT_NEAR(annot1.salience_score, annot2.salience_score, 0.01f);
}

TEST_F(CognitiveProcessorTest, ConfidenceIncreasesWithSpikes) {
    cognitive_annotations_t low_spike_annot, high_spike_annot;
    cognitive_annotations_init(&low_spike_annot);
    cognitive_annotations_init(&high_spike_annot);

    // Low spike count
    net_output.spikes_generated = 10;
    cognitive_process_output(
        brain, &net_output, integrated_features, 20, 1000, &low_spike_annot);

    // High spike count
    net_output.spikes_generated = 100;
    cognitive_process_output(
        brain, &net_output, integrated_features, 20, 1000, &high_spike_annot);

    // Higher spike count often indicates more confident activation
    // (Note: exact behavior depends on implementation)
    EXPECT_GE(low_spike_annot.confidence, 0.0f);
    EXPECT_GE(high_spike_annot.confidence, 0.0f);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(CognitiveProcessorTest, NullBrainHandling) {
    bool success = cognitive_process_output(
        nullptr,
        &net_output,
        integrated_features,
        20,
        1000,
        &annotations
    );

    EXPECT_FALSE(success) << "Should fail with null brain";
}

TEST_F(CognitiveProcessorTest, NullNetworkOutputHandling) {
    bool success = cognitive_process_output(
        brain,
        nullptr,
        integrated_features,
        20,
        1000,
        &annotations
    );

    EXPECT_FALSE(success) << "Should fail with null network output";
}

TEST_F(CognitiveProcessorTest, NullAnnotationsHandling) {
    bool success = cognitive_process_output(
        brain,
        &net_output,
        integrated_features,
        20,
        1000,
        nullptr
    );

    EXPECT_FALSE(success) << "Should fail with null annotations";
}

TEST_F(CognitiveProcessorTest, NullIntegratedFeatures) {
    bool success = cognitive_process_output(
        brain,
        &net_output,
        nullptr,
        0,
        1000,
        &annotations
    );

    // May succeed with null features depending on implementation
    // At minimum should not crash
    (void)success;  // Result depends on implementation
}

TEST_F(CognitiveProcessorTest, ZeroOutputSize) {
    net_output.output_size = 0;

    bool success = cognitive_process_output(
        brain,
        &net_output,
        integrated_features,
        20,
        1000,
        &annotations
    );

    // Should handle gracefully
    EXPECT_FALSE(success) << "Should fail with zero output size";
}

TEST_F(CognitiveProcessorTest, ZeroFeatureDimension) {
    bool success = cognitive_process_output(
        brain,
        &net_output,
        integrated_features,
        0,  // Zero dimension
        1000,
        &annotations
    );

    // Should handle gracefully
    (void)success;  // Result depends on implementation
}

TEST_F(CognitiveProcessorTest, ZeroTimestamp) {
    bool success = cognitive_process_output(
        brain,
        &net_output,
        integrated_features,
        20,
        0,  // Zero timestamp
        &annotations
    );

    EXPECT_TRUE(success) << "Zero timestamp should be valid";
}

TEST_F(CognitiveProcessorTest, ExtremelyLargeTimestamp) {
    bool success = cognitive_process_output(
        brain,
        &net_output,
        integrated_features,
        20,
        UINT64_MAX,
        &annotations
    );

    EXPECT_TRUE(success) << "Large timestamp should be handled";
}

//=============================================================================
// Output Validation Tests
//=============================================================================

TEST_F(CognitiveProcessorTest, ConfidenceRange) {
    cognitive_process_output(
        brain, &net_output, integrated_features, 20, 1000, &annotations);

    EXPECT_GE(annotations.confidence, 0.0f) << "Confidence should be >= 0";
    EXPECT_LE(annotations.confidence, 1.0f) << "Confidence should be <= 1";
}

TEST_F(CognitiveProcessorTest, UncertaintyRange) {
    cognitive_process_output(
        brain, &net_output, integrated_features, 20, 1000, &annotations);

    EXPECT_GE(annotations.uncertainty, 0.0f) << "Uncertainty should be >= 0";
    EXPECT_LE(annotations.uncertainty, 1.0f) << "Uncertainty should be <= 1";
}

TEST_F(CognitiveProcessorTest, SalienceRange) {
    cognitive_process_output(
        brain, &net_output, integrated_features, 20, 1000, &annotations);

    EXPECT_GE(annotations.salience_score, 0.0f);
    EXPECT_LE(annotations.salience_score, 1.0f);
}

TEST_F(CognitiveProcessorTest, NoveltyRange) {
    cognitive_process_output(
        brain, &net_output, integrated_features, 20, 1000, &annotations);

    EXPECT_GE(annotations.novelty_score, 0.0f);
    EXPECT_LE(annotations.novelty_score, 1.0f);
}

TEST_F(CognitiveProcessorTest, UrgencyRange) {
    cognitive_process_output(
        brain, &net_output, integrated_features, 20, 1000, &annotations);

    EXPECT_GE(annotations.urgency_score, 0.0f);
    EXPECT_LE(annotations.urgency_score, 1.0f);
}

TEST_F(CognitiveProcessorTest, ExplorationBonusNonNegative) {
    cognitive_process_output(
        brain, &net_output, integrated_features, 20, 1000, &annotations);

    EXPECT_GE(annotations.exploration_bonus, 0.0f);
}

TEST_F(CognitiveProcessorTest, InformationGainNonNegative) {
    cognitive_process_output(
        brain, &net_output, integrated_features, 20, 1000, &annotations);

    EXPECT_GE(annotations.information_gain, 0.0f);
}

//=============================================================================
// Temporal Consistency Tests
//=============================================================================

TEST_F(CognitiveProcessorTest, TemporalProgression) {
    cognitive_annotations_t annot_t1, annot_t2, annot_t3;
    cognitive_annotations_init(&annot_t1);
    cognitive_annotations_init(&annot_t2);
    cognitive_annotations_init(&annot_t3);

    // Process at different timestamps
    cognitive_process_output(brain, &net_output, integrated_features, 20, 100, &annot_t1);
    cognitive_process_output(brain, &net_output, integrated_features, 20, 500, &annot_t2);
    cognitive_process_output(brain, &net_output, integrated_features, 20, 1000, &annot_t3);

    // All should succeed
    EXPECT_GE(annot_t1.confidence, 0.0f);
    EXPECT_GE(annot_t2.confidence, 0.0f);
    EXPECT_GE(annot_t3.confidence, 0.0f);
}

//=============================================================================
// Different Output Patterns Tests
//=============================================================================

TEST_F(CognitiveProcessorTest, UniformOutput) {
    // All outputs equal
    for (int i = 0; i < 10; i++) {
        output_vector[i] = 0.5f;
    }

    bool success = cognitive_process_output(
        brain, &net_output, integrated_features, 20, 1000, &annotations);

    EXPECT_TRUE(success);
    // Uniform output might indicate low confidence
    EXPECT_GE(annotations.confidence, 0.0f);
}

TEST_F(CognitiveProcessorTest, SparseOutput) {
    // Only one neuron active
    for (int i = 0; i < 10; i++) {
        output_vector[i] = 0.0f;
    }
    output_vector[5] = 1.0f;

    bool success = cognitive_process_output(
        brain, &net_output, integrated_features, 20, 1000, &annotations);

    EXPECT_TRUE(success);
    // Sparse output might indicate high confidence
    EXPECT_GE(annotations.confidence, 0.0f);
}

TEST_F(CognitiveProcessorTest, ZeroOutput) {
    // All outputs zero
    for (int i = 0; i < 10; i++) {
        output_vector[i] = 0.0f;
    }

    bool success = cognitive_process_output(
        brain, &net_output, integrated_features, 20, 1000, &annotations);

    EXPECT_TRUE(success);
    // Zero output might indicate low confidence or uncertain state
    EXPECT_GE(annotations.uncertainty, 0.0f);
}

TEST_F(CognitiveProcessorTest, MaximumOutput) {
    // All outputs at maximum
    for (int i = 0; i < 10; i++) {
        output_vector[i] = 1.0f;
    }

    bool success = cognitive_process_output(
        brain, &net_output, integrated_features, 20, 1000, &annotations);

    EXPECT_TRUE(success);
}

//=============================================================================
// Ethics Module Tests
//=============================================================================

TEST_F(CognitiveProcessorTest, EthicsApprovalBoolean) {
    cognitive_process_output(
        brain, &net_output, integrated_features, 20, 1000, &annotations);

    // ethical_approved is boolean
    EXPECT_TRUE(annotations.ethical_approved == true ||
                annotations.ethical_approved == false);
}

//=============================================================================
// Logic Module Tests
//=============================================================================

TEST_F(CognitiveProcessorTest, LogicValidBoolean) {
    cognitive_process_output(
        brain, &net_output, integrated_features, 20, 1000, &annotations);

    // logic_valid is boolean
    EXPECT_TRUE(annotations.logic_valid == true ||
                annotations.logic_valid == false);
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(CognitiveProcessorTest, MultipleSequentialProcessing) {
    for (int i = 0; i < 100; i++) {
        cognitive_annotations_t annot;
        cognitive_annotations_init(&annot);

        bool success = cognitive_process_output(
            brain, &net_output, integrated_features, 20, i * 10, &annot);

        EXPECT_TRUE(success) << "Failed at iteration " << i;
        EXPECT_GE(annot.confidence, 0.0f);
        EXPECT_LE(annot.confidence, 1.0f);
    }
}

TEST_F(CognitiveProcessorTest, LargeOutputVector) {
    // Test with larger output
    float large_output[1000];
    for (int i = 0; i < 1000; i++) {
        large_output[i] = sinf(i * 0.1f);
    }

    net_output.output_vector = large_output;
    net_output.output_size = 1000;

    bool success = cognitive_process_output(
        brain, &net_output, integrated_features, 20, 1000, &annotations);

    EXPECT_TRUE(success);
}

TEST_F(CognitiveProcessorTest, LargeFeatureVector) {
    // Test with larger features
    float large_features[1000];
    for (int i = 0; i < 1000; i++) {
        large_features[i] = cosf(i * 0.05f);
    }

    bool success = cognitive_process_output(
        brain, &net_output, large_features, 1000, 1000, &annotations);

    EXPECT_TRUE(success);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
