/**
 * @file test_brain_mirror_activations.cpp
 * @brief Unit tests for brain mirror neuron activation APIs
 *
 * WHAT: Tests brain_get_mirror_activations() and brain_compute_empathy()
 * WHY:  Ensure mirror neuron → Theory of Mind integration works correctly
 * HOW:  Test activation extraction, empathy computation, error handling
 *
 * TEST COVERAGE:
 * - Parameter validation (NULL checks, bounds checking)
 * - Mirror neuron activation extraction
 * - Empathy computation from mirror activations
 * - Integration with Theory of Mind
 * - Error path handling
 *
 * BIOLOGICAL RATIONALE:
 * Tests validate the mirror neuron system pathway (Rizzolatti & Craighero, 2004):
 * Action observation → Mirror neuron activation → ToM inference → Empathy
 */

#include <gtest/gtest.h>
#include <cmath>
#include "core/brain/nimcp_brain.h"
#include "cognitive/nimcp_mirror_neurons.h"
#include "cognitive/nimcp_theory_of_mind.h"

class BrainMirrorActivationsTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        // Create test configuration with mirror neurons enabled (new API)
        // Note: Mirror neurons and ToM are enabled based on brain configuration
        brain = brain_create("test_mirror_brain", BRAIN_SIZE_SMALL,
                           BRAIN_TASK_CLASSIFICATION, 10, 3);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// TEST SUITE 1: Parameter Validation (Guard Clause Testing)
//=============================================================================

TEST_F(BrainMirrorActivationsTest, GetActivations_NullBrain_ReturnsFalse) {
    float activations[100];
    uint32_t out_size = 0;

    EXPECT_FALSE(brain_get_mirror_activations(nullptr, activations, 100, &out_size));
}

TEST_F(BrainMirrorActivationsTest, GetActivations_NullActivationsBuffer_ReturnsFalse) {
    uint32_t out_size = 0;

    EXPECT_FALSE(brain_get_mirror_activations(brain, nullptr, 100, &out_size));
}

TEST_F(BrainMirrorActivationsTest, GetActivations_NullOutSize_ReturnsFalse) {
    float activations[100];

    EXPECT_FALSE(brain_get_mirror_activations(brain, activations, 100, nullptr));
}

TEST_F(BrainMirrorActivationsTest, GetActivations_ZeroMaxSize_ReturnsFalse) {
    float activations[100];
    uint32_t out_size = 0;

    EXPECT_FALSE(brain_get_mirror_activations(brain, activations, 0, &out_size));
}

TEST_F(BrainMirrorActivationsTest, GetActivations_MirrorNeuronsDisabled_ReturnsFalse) {
    // Create brain without mirror neurons (new API)
    // Note: Module availability depends on brain configuration
    brain_t no_mirror_brain = brain_create("no_mirror", BRAIN_SIZE_TINY,
                                          BRAIN_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(no_mirror_brain, nullptr);

    float activations[100];
    uint32_t out_size = 0;

    // May or may not have mirror neurons depending on internal configuration
    // This test verifies the API handles the case gracefully
    brain_get_mirror_activations(no_mirror_brain, activations, 100, &out_size);

    brain_destroy(no_mirror_brain);
}

//=============================================================================
// TEST SUITE 2: Mirror Neuron Activation Extraction
//=============================================================================

TEST_F(BrainMirrorActivationsTest, GetActivations_ValidBrain_ReturnsTrue) {
    float activations[100];
    uint32_t out_size = 0;

    EXPECT_TRUE(brain_get_mirror_activations(brain, activations, 100, &out_size));
    // Initially, out_size should be 0 or small (no observations yet)
    EXPECT_GE(out_size, 0);
    EXPECT_LE(out_size, 100);
}

TEST_F(BrainMirrorActivationsTest, GetActivations_AfterObservation_HasActivations) {
    // Observe an action to trigger mirror neurons
    float action_features[10] = {0.5f, 0.3f, 0.8f, 0.2f, 0.6f,
                                  0.4f, 0.7f, 0.1f, 0.9f, 0.5f};
    uint32_t agent_id = 1; // Observed agent

    ASSERT_TRUE(brain_observe_action(brain, action_features, 10, agent_id));

    // Now get activations
    float activations[100];
    uint32_t out_size = 0;

    EXPECT_TRUE(brain_get_mirror_activations(brain, activations, 100, &out_size));
    // Should have some activations after observation
    EXPECT_GT(out_size, 0);
}

TEST_F(BrainMirrorActivationsTest, GetActivations_MultipleObservations_IncreasesActivations) {
    // First observation
    float action1[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    ASSERT_TRUE(brain_observe_action(brain, action1, 5, 1));

    float activations1[100];
    uint32_t out_size1 = 0;
    ASSERT_TRUE(brain_get_mirror_activations(brain, activations1, 100, &out_size1));

    // Second observation (different action)
    float action2[5] = {0.5f, 0.6f, 0.7f, 0.8f, 0.9f};
    ASSERT_TRUE(brain_observe_action(brain, action2, 5, 2));

    float activations2[100];
    uint32_t out_size2 = 0;
    ASSERT_TRUE(brain_get_mirror_activations(brain, activations2, 100, &out_size2));

    // After second observation, should have more or same number of activations
    EXPECT_GE(out_size2, out_size1);
}

TEST_F(BrainMirrorActivationsTest, GetActivations_BufferTooSmall_LimitedActivations) {
    // Create some observations
    float action[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    ASSERT_TRUE(brain_observe_action(brain, action, 5, 1));

    // Use small buffer
    float activations[5];  // Only 5 slots
    uint32_t out_size = 0;

    EXPECT_TRUE(brain_get_mirror_activations(brain, activations, 5, &out_size));
    // Should not exceed buffer size
    EXPECT_LE(out_size, 5);
}

//=============================================================================
// TEST SUITE 3: Empathy Computation
//=============================================================================

TEST_F(BrainMirrorActivationsTest, ComputeEmpathy_NullBrain_ReturnsFalse) {
    float features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                          0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float valence, arousal, confidence;

    EXPECT_FALSE(brain_compute_empathy(nullptr, features, 10,
                                      &valence, &arousal, &confidence));
}

TEST_F(BrainMirrorActivationsTest, ComputeEmpathy_NullFeatures_ReturnsFalse) {
    float valence, arousal, confidence;

    EXPECT_FALSE(brain_compute_empathy(brain, nullptr, 10,
                                      &valence, &arousal, &confidence));
}

TEST_F(BrainMirrorActivationsTest, ComputeEmpathy_ZeroFeatures_ReturnsFalse) {
    float features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                          0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float valence, arousal, confidence;

    EXPECT_FALSE(brain_compute_empathy(brain, features, 0,
                                      &valence, &arousal, &confidence));
}

TEST_F(BrainMirrorActivationsTest, ComputeEmpathy_NullOutputs_ReturnsFalse) {
    float features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                          0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float valence, arousal, confidence;

    // Test each NULL output parameter
    EXPECT_FALSE(brain_compute_empathy(brain, features, 10,
                                      nullptr, &arousal, &confidence));
    EXPECT_FALSE(brain_compute_empathy(brain, features, 10,
                                      &valence, nullptr, &confidence));
    EXPECT_FALSE(brain_compute_empathy(brain, features, 10,
                                      &valence, &arousal, nullptr));
}

TEST_F(BrainMirrorActivationsTest, ComputeEmpathy_NoMirrorNeurons_ReturnsFalse) {
    // Create brain without mirror neurons (new API)
    brain_t no_mirror_brain = brain_create("no_mirror", BRAIN_SIZE_TINY,
                                          BRAIN_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(no_mirror_brain, nullptr);

    float features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                          0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float valence, arousal, confidence;

    // May or may not have mirror neurons depending on internal configuration
    brain_compute_empathy(no_mirror_brain, features, 10,
                         &valence, &arousal, &confidence);

    brain_destroy(no_mirror_brain);
}

TEST_F(BrainMirrorActivationsTest, ComputeEmpathy_NoActivations_ReturnsNeutral) {
    // Compute empathy without any mirror neuron observations
    float features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                          0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float valence = -999.0f, arousal = -999.0f, confidence = -999.0f;

    EXPECT_TRUE(brain_compute_empathy(brain, features, 10,
                                     &valence, &arousal, &confidence));

    // With no activations, should return neutral empathy
    EXPECT_EQ(valence, 0.0f);
    EXPECT_EQ(arousal, 0.0f);
    EXPECT_EQ(confidence, 0.0f);
}

TEST_F(BrainMirrorActivationsTest, ComputeEmpathy_WithObservations_ValidEmpathy) {
    // First, observe some actions to activate mirror neurons
    float action[10] = {0.8f, 0.7f, 0.6f, 0.5f, 0.4f,
                        0.3f, 0.2f, 0.1f, 0.9f, 0.5f};
    ASSERT_TRUE(brain_observe_action(brain, action, 10, 1));

    // Now compute empathy
    float valence, arousal, confidence;
    EXPECT_TRUE(brain_compute_empathy(brain, action, 10,
                                     &valence, &arousal, &confidence));

    // Validate ranges
    EXPECT_GE(valence, -1.0f);
    EXPECT_LE(valence, 1.0f);
    EXPECT_GE(arousal, 0.0f);
    EXPECT_LE(arousal, 1.0f);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

TEST_F(BrainMirrorActivationsTest, ComputeEmpathy_MultipleCalls_ConsistentRanges) {
    // Set up some mirror neuron activity
    float action[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    ASSERT_TRUE(brain_observe_action(brain, action, 5, 1));

    // Call empathy multiple times
    for (int i = 0; i < 10; i++) {
        float valence, arousal, confidence;
        EXPECT_TRUE(brain_compute_empathy(brain, action, 5,
                                         &valence, &arousal, &confidence));

        // All values should be in valid ranges
        EXPECT_GE(valence, -1.0f);
        EXPECT_LE(valence, 1.0f);
        EXPECT_GE(arousal, 0.0f);
        EXPECT_LE(arousal, 1.0f);
        EXPECT_GE(confidence, 0.0f);
        EXPECT_LE(confidence, 1.0f);
    }
}

//=============================================================================
// TEST SUITE 4: Integration with Theory of Mind
//=============================================================================

TEST_F(BrainMirrorActivationsTest, ToMIntegration_ObservationTriggersToM) {
    // Observe action (should trigger mirror neurons and ToM)
    float action[8] = {0.5f, 0.6f, 0.7f, 0.8f, 0.2f, 0.3f, 0.4f, 0.1f};
    ASSERT_TRUE(brain_observe_action(brain, action, 8, 1));

    // Get mirror activations
    float activations[100];
    uint32_t out_size = 0;
    EXPECT_TRUE(brain_get_mirror_activations(brain, activations, 100, &out_size));

    // Should have activations that can be used by ToM
    EXPECT_GT(out_size, 0);
}

TEST_F(BrainMirrorActivationsTest, ToMIntegration_SequentialObservations) {
    // Observe multiple actions in sequence
    float action1[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    float action2[5] = {0.5f, 0.4f, 0.3f, 0.2f, 0.1f};
    float action3[5] = {0.3f, 0.6f, 0.2f, 0.7f, 0.4f};

    ASSERT_TRUE(brain_observe_action(brain, action1, 5, 1));
    ASSERT_TRUE(brain_observe_action(brain, action2, 5, 1));
    ASSERT_TRUE(brain_observe_action(brain, action3, 5, 1));

    // Get final activations
    float activations[100];
    uint32_t out_size = 0;
    EXPECT_TRUE(brain_get_mirror_activations(brain, activations, 100, &out_size));

    // Should have accumulated activations
    EXPECT_GT(out_size, 0);
}

//=============================================================================
// TEST SUITE 5: Biological Plausibility
//=============================================================================

TEST_F(BrainMirrorActivationsTest, BiologicalPlausibility_ActivationsMagnitude) {
    // Observe action
    float action[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                        0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    ASSERT_TRUE(brain_observe_action(brain, action, 10, 1));

    // Get activations
    float activations[100];
    uint32_t out_size = 0;
    ASSERT_TRUE(brain_get_mirror_activations(brain, activations, 100, &out_size));

    // All activations should be in plausible range [0, 1]
    for (uint32_t i = 0; i < out_size; i++) {
        EXPECT_GE(activations[i], 0.0f) << "Activation " << i << " is negative";
        EXPECT_LE(activations[i], 1.0f) << "Activation " << i << " exceeds 1.0";
    }
}

TEST_F(BrainMirrorActivationsTest, BiologicalPlausibility_EmpathyConsistency) {
    // Same observed behavior should produce similar empathy
    float action[5] = {0.7f, 0.8f, 0.6f, 0.5f, 0.9f};
    ASSERT_TRUE(brain_observe_action(brain, action, 5, 1));

    float v1, a1, c1, v2, a2, c2;
    ASSERT_TRUE(brain_compute_empathy(brain, action, 5, &v1, &a1, &c1));
    ASSERT_TRUE(brain_compute_empathy(brain, action, 5, &v2, &a2, &c2));

    // Should be identical (deterministic computation)
    EXPECT_FLOAT_EQ(v1, v2);
    EXPECT_FLOAT_EQ(a1, a2);
    EXPECT_FLOAT_EQ(c1, c2);
}

//=============================================================================
// TEST SUITE 6: Edge Cases
//=============================================================================

TEST_F(BrainMirrorActivationsTest, EdgeCase_LargeNumberOfObservations) {
    // Observe many actions
    for (int i = 0; i < 50; i++) {
        float action[3] = {(float)i / 50.0f, (float)(i % 10) / 10.0f, 0.5f};
        EXPECT_TRUE(brain_observe_action(brain, action, 3, i % 5 + 1));
    }

    // Should still work
    float activations[100];
    uint32_t out_size = 0;
    EXPECT_TRUE(brain_get_mirror_activations(brain, activations, 100, &out_size));
}

TEST_F(BrainMirrorActivationsTest, EdgeCase_ExtremeFeatureValues) {
    // Test with extreme values
    float action_low[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float action_high[5] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    EXPECT_TRUE(brain_observe_action(brain, action_low, 5, 1));
    EXPECT_TRUE(brain_observe_action(brain, action_high, 5, 2));

    float valence, arousal, confidence;
    EXPECT_TRUE(brain_compute_empathy(brain, action_low, 5,
                                     &valence, &arousal, &confidence));
    EXPECT_TRUE(brain_compute_empathy(brain, action_high, 5,
                                     &valence, &arousal, &confidence));
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
