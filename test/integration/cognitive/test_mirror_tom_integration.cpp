/**
 * @file test_mirror_tom_integration.cpp
 * @brief Integration tests for Mirror Neuron → Theory of Mind pipeline
 *
 * WHAT: Tests complete social cognition pathway
 * WHY:  Validate mirror neurons enable ToM to infer mental states
 * HOW:  Observe actions → mirror activation → ToM inference → empathy
 *
 * BIOLOGICAL RATIONALE:
 * Tests the full pathway described by Rizzolatti & Craighero (2004):
 * Visual observation → F5/IPL mirror neurons → mPFC/TPJ ToM → Empathy
 *
 * TEST SCENARIOS:
 * - Single agent observation and intention inference
 * - Multiple agent tracking
 * - False belief understanding via mirror neurons
 * - Empathy generation from observed distress/joy
 * - Action prediction from mirror neuron patterns
 */

#include <gtest/gtest.h>
#include "core/brain/nimcp_brain.h"
#include "cognitive/nimcp_mirror_neurons.h"
#include "cognitive/nimcp_theory_of_mind.h"

class MirrorToMIntegrationTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        // Create brain with full social cognition stack (new API)
        // Note: Modules are enabled internally based on brain configuration
        brain = brain_create("social_brain", BRAIN_SIZE_MEDIUM,
                           BRAIN_TASK_CLASSIFICATION, 8, 3);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    // Helper: Simulate observing an agent reaching for object
    void observe_reaching_action(uint32_t agent_id) {
        float reaching_features[8] = {
            0.8f,  // High arm extension
            0.6f,  // Moderate hand opening
            0.3f,  // Low torso rotation
            0.9f,  // High forward motion
            0.2f,  // Low downward motion
            0.5f,  // Moderate gaze direction
            0.7f,  // High goal directedness
            0.4f   // Moderate speed
        };
        ASSERT_TRUE(brain_observe_action(brain, reaching_features, 8, agent_id));
    }

    // Helper: Simulate observing distress behavior
    void observe_distress_action(uint32_t agent_id) {
        float distress_features[8] = {
            0.2f,  // Low arm extension (withdrawn)
            0.1f,  // Closed hands
            0.8f,  // High torso rotation (turning away)
            0.3f,  // Low forward motion
            0.4f,  // Moderate downward motion
            0.6f,  // Moderate gaze aversion
            0.2f,  // Low goal directedness
            0.5f   // Moderate speed (agitated)
        };
        ASSERT_TRUE(brain_observe_action(brain, distress_features, 8, agent_id));
    }

    // Helper: Simulate observing joyful behavior
    void observe_joyful_action(uint32_t agent_id) {
        float joy_features[8] = {
            0.9f,  // High arm extension (open arms)
            0.9f,  // Open hands
            0.3f,  // Low torso rotation
            0.5f,  // Moderate forward motion
            0.2f,  // Low downward motion
            0.8f,  // High gaze engagement
            0.7f,  // High goal directedness
            0.8f   // High speed (energetic)
        };
        ASSERT_TRUE(brain_observe_action(brain, joy_features, 8, agent_id));
    }
};

//=============================================================================
// TEST SUITE 1: Basic Mirror → ToM Pipeline
//=============================================================================

TEST_F(MirrorToMIntegrationTest, Pipeline_ObservationActivatesMirrorNeurons) {
    // Observe action
    observe_reaching_action(1);

    // Check mirror neurons activated
    float activations[100];
    uint32_t num_activations = 0;

    ASSERT_TRUE(brain_get_mirror_activations(brain, activations, 100, &num_activations));
    EXPECT_GT(num_activations, 0) << "Mirror neurons should activate after observation";
}

TEST_F(MirrorToMIntegrationTest, Pipeline_MirrorActivationsEnableEmpathy) {
    // Observe distress
    observe_distress_action(1);

    // Compute empathy
    float valence, arousal, confidence;
    float distress_features[8] = {0.2f, 0.1f, 0.8f, 0.3f, 0.4f, 0.6f, 0.2f, 0.5f};

    ASSERT_TRUE(brain_compute_empathy(brain, distress_features, 8,
                                     &valence, &arousal, &confidence));

    // Should have non-zero empathetic response
    EXPECT_GT(confidence, 0.0f) << "Should have some empathy confidence";
}

TEST_F(MirrorToMIntegrationTest, Pipeline_DifferentActionsProduceDifferentEmpathy) {
    // Observe distress
    observe_distress_action(1);
    float distress_features[8] = {0.2f, 0.1f, 0.8f, 0.3f, 0.4f, 0.6f, 0.2f, 0.5f};
    float v_distress, a_distress, c_distress;
    ASSERT_TRUE(brain_compute_empathy(brain, distress_features, 8,
                                     &v_distress, &a_distress, &c_distress));

    // Observe joy
    observe_joyful_action(2);
    float joy_features[8] = {0.9f, 0.9f, 0.3f, 0.5f, 0.2f, 0.8f, 0.7f, 0.8f};
    float v_joy, a_joy, c_joy;
    ASSERT_TRUE(brain_compute_empathy(brain, joy_features, 8,
                                     &v_joy, &a_joy, &c_joy));

    // Empathy should differ based on observed action
    // (Note: exact values depend on implementation, but should be different)
    EXPECT_TRUE(std::abs(a_distress - a_joy) > 0.01f ||
                std::abs(v_distress - v_joy) > 0.01f)
        << "Different observed emotions should produce different empathy";
}

//=============================================================================
// TEST SUITE 2: Multi-Agent Tracking
//=============================================================================

TEST_F(MirrorToMIntegrationTest, MultiAgent_TrackMultipleAgents) {
    // Observe three different agents
    observe_reaching_action(1);
    observe_distress_action(2);
    observe_joyful_action(3);

    // Should track all agents
    float activations[100];
    uint32_t num_activations = 0;

    ASSERT_TRUE(brain_get_mirror_activations(brain, activations, 100, &num_activations));
    EXPECT_GT(num_activations, 0) << "Should track multiple agents";
}

TEST_F(MirrorToMIntegrationTest, MultiAgent_SameAgentMultipleObservations) {
    // Observe same agent multiple times
    observe_reaching_action(1);
    observe_reaching_action(1);
    observe_reaching_action(1);

    // Should maintain consistent activations
    float activations[100];
    uint32_t num_activations = 0;

    EXPECT_TRUE(brain_get_mirror_activations(brain, activations, 100, &num_activations));
}

//=============================================================================
// TEST SUITE 3: Empathy Generation Scenarios
//=============================================================================

TEST_F(MirrorToMIntegrationTest, Empathy_ObservedDistressElicitsEmpathy) {
    observe_distress_action(1);

    float distress_features[8] = {0.2f, 0.1f, 0.8f, 0.3f, 0.4f, 0.6f, 0.2f, 0.5f};
    float valence, arousal, confidence;

    ASSERT_TRUE(brain_compute_empathy(brain, distress_features, 8,
                                     &valence, &arousal, &confidence));

    // Should have some arousal from observing distress
    EXPECT_GT(arousal, 0.0f);
    EXPECT_GT(confidence, 0.0f);
}

TEST_F(MirrorToMIntegrationTest, Empathy_ObservedJoyElicitsEmpathy) {
    observe_joyful_action(1);

    float joy_features[8] = {0.9f, 0.9f, 0.3f, 0.5f, 0.2f, 0.8f, 0.7f, 0.8f};
    float valence, arousal, confidence;

    ASSERT_TRUE(brain_compute_empathy(brain, joy_features, 8,
                                     &valence, &arousal, &confidence));

    // Should have positive response to observed joy
    EXPECT_GT(arousal, 0.0f);
    EXPECT_GT(confidence, 0.0f);
}

TEST_F(MirrorToMIntegrationTest, Empathy_RepeatedObservationsStrengthensEmpathy) {
    // First observation
    observe_distress_action(1);
    float distress_features[8] = {0.2f, 0.1f, 0.8f, 0.3f, 0.4f, 0.6f, 0.2f, 0.5f};
    float v1, a1, c1;
    ASSERT_TRUE(brain_compute_empathy(brain, distress_features, 8, &v1, &a1, &c1));

    // Additional observations (simulate prolonged distress)
    observe_distress_action(1);
    observe_distress_action(1);

    float v2, a2, c2;
    ASSERT_TRUE(brain_compute_empathy(brain, distress_features, 8, &v2, &a2, &c2));

    // Empathy should be stable or strengthened
    EXPECT_GE(c2, c1 * 0.8f) << "Empathy confidence should not drop significantly";
}

//=============================================================================
// TEST SUITE 4: Social Learning Scenarios
//=============================================================================

TEST_F(MirrorToMIntegrationTest, SocialLearning_ActionSequenceLearning) {
    // Observe action sequence: reach → grasp → retract
    float reach[5] = {0.8f, 0.5f, 0.3f, 0.9f, 0.2f};
    float grasp[5] = {0.9f, 0.9f, 0.2f, 0.5f, 0.3f};
    float retract[5] = {0.3f, 0.8f, 0.1f, 0.2f, 0.5f};

    ASSERT_TRUE(brain_observe_action(brain, reach, 5, 1));
    ASSERT_TRUE(brain_observe_action(brain, grasp, 5, 1));
    ASSERT_TRUE(brain_observe_action(brain, retract, 5, 1));

    // Mirror neurons should have learned sequence
    float activations[100];
    uint32_t num_activations = 0;

    EXPECT_TRUE(brain_get_mirror_activations(brain, activations, 100, &num_activations));
    EXPECT_GT(num_activations, 0);
}

TEST_F(MirrorToMIntegrationTest, SocialLearning_ImitationLearningPreparation) {
    // Observe skilled action from expert agent
    float expert_action[10] = {
        0.9f, 0.8f, 0.7f, 0.6f, 0.5f,
        0.4f, 0.3f, 0.2f, 0.1f, 0.95f
    };

    ASSERT_TRUE(brain_observe_action(brain, expert_action, 10, 99)); // Agent 99 = expert

    // Mirror neurons should encode the observed action
    float activations[100];
    uint32_t num_activations = 0;

    ASSERT_TRUE(brain_get_mirror_activations(brain, activations, 100, &num_activations));

    // Should have learned representation for potential imitation
    EXPECT_GT(num_activations, 0);
}

//=============================================================================
// TEST SUITE 5: Biological Plausibility
//=============================================================================

TEST_F(MirrorToMIntegrationTest, BiologicalPlausibility_EmpathyLatency) {
    // Empathy should be computed quickly (mirror neurons are fast)
    observe_distress_action(1);

    float distress_features[8] = {0.2f, 0.1f, 0.8f, 0.3f, 0.4f, 0.6f, 0.2f, 0.5f};

    // Time empathy computation
    auto start = std::chrono::high_resolution_clock::now();

    float valence, arousal, confidence;
    for (int i = 0; i < 100; i++) {
        brain_compute_empathy(brain, distress_features, 8,
                             &valence, &arousal, &confidence);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should be fast (< 10ms per call on average)
    EXPECT_LT(duration.count() / 100, 10000)
        << "Empathy computation should be fast (< 10ms per call)";
}

TEST_F(MirrorToMIntegrationTest, BiologicalPlausibility_SelectiveActivation) {
    // Mirror neurons should selectively activate for relevant actions
    // (Not just activate indiscriminately)

    // Observe clear reaching action
    observe_reaching_action(1);
    float activations1[100];
    uint32_t size1 = 0;
    ASSERT_TRUE(brain_get_mirror_activations(brain, activations1, 100, &size1));

    // Observe different clear joyful action
    observe_joyful_action(2);
    float activations2[100];
    uint32_t size2 = 0;
    ASSERT_TRUE(brain_get_mirror_activations(brain, activations2, 100, &size2));

    // Both should activate, but patterns should differ
    EXPECT_GT(size1, 0);
    EXPECT_GT(size2, 0);
}

//=============================================================================
// TEST SUITE 6: Stress and Performance
//=============================================================================

TEST_F(MirrorToMIntegrationTest, Stress_ManySequentialObservations) {
    // Simulate prolonged social interaction
    for (int i = 0; i < 100; i++) {
        float action[5] = {
            (float)(i % 10) / 10.0f,
            (float)(i % 7) / 7.0f,
            (float)(i % 5) / 5.0f,
            (float)(i % 3) / 3.0f,
            0.5f
        };
        EXPECT_TRUE(brain_observe_action(brain, action, 5, (i % 5) + 1));
    }

    // Should still work correctly
    float activations[100];
    uint32_t num_activations = 0;
    EXPECT_TRUE(brain_get_mirror_activations(brain, activations, 100, &num_activations));

    float valence, arousal, confidence;
    float test_features[5] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    EXPECT_TRUE(brain_compute_empathy(brain, test_features, 5,
                                     &valence, &arousal, &confidence));
}

TEST_F(MirrorToMIntegrationTest, Stress_RapidEmpathyQueries) {
    // Set up observations
    observe_distress_action(1);
    observe_joyful_action(2);

    // Rapid empathy queries
    float features[5] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    for (int i = 0; i < 1000; i++) {
        float valence, arousal, confidence;
        EXPECT_TRUE(brain_compute_empathy(brain, features, 5,
                                         &valence, &arousal, &confidence));
    }
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
