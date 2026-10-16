/**
 * @file test_theory_of_mind_real.cpp
 * @brief REAL tests for nimcp_theory_of_mind.c that exercise actual implementation
 *
 * DIFFERENCE FROM test_theory_of_mind_coverage.cpp:
 * - Creates REAL brain instances
 * - Creates REAL theory of mind contexts
 * - Exercises actual implementation code paths
 * - NOT just NULL guards and mock brain pointers
 *
 * @date 2025-11-10
 */

#include <gtest/gtest.h>
#include <cstring>

#include "cognitive/nimcp_theory_of_mind.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class TheoryOfMindRealTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    theory_of_mind_t tom = nullptr;

    void SetUp() override {
        // Create a REAL brain instance (tiny size for testing)
        brain = brain_create("tom_test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr) << "Failed to create brain";

        // Create REAL theory of mind context
        tom = tom_create(brain);
        ASSERT_NE(tom, nullptr) << "Failed to create theory of mind context";
    }

    void TearDown() override {
        // Clean up theory of mind context
        if (tom) {
            tom_destroy(tom);
            tom = nullptr;
        }

        // Clean up brain
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    // Helper: Create observation with verbal context
    tom_observation_t create_observation(const char* verbal_context,
                                         tom_emotion_t observed_emotion = TOM_EMOTION_UNKNOWN) {
        tom_observation_t obs;
        obs.action_vector = nullptr;
        obs.action_dim = 0;
        obs.verbal_context = verbal_context;
        obs.observed_emotion = observed_emotion;
        obs.situational_context = nullptr;
        obs.context_dim = 0;
        return obs;
    }
};

//=============================================================================
// Test Suite: REAL ToM Creation with Real Brain
//=============================================================================

TEST_F(TheoryOfMindRealTest, CreateWithRealBrain) {
    // Already created in SetUp
    EXPECT_NE(tom, nullptr);
}

TEST_F(TheoryOfMindRealTest, CreateMultipleContexts) {
    // Create another ToM context with same brain
    theory_of_mind_t tom2 = tom_create(brain);
    ASSERT_NE(tom2, nullptr);

    // Both should be independent
    tom_observation_t obs1 = create_observation("I am happy");
    tom_observation_t obs2 = create_observation("I am sad");

    tom_observe(tom, &obs1);
    tom_observe(tom2, &obs2);

    // Check they have different states
    float conf1, conf2;
    tom_emotion_t emotion1 = tom_infer_emotion(tom, &conf1);
    tom_emotion_t emotion2 = tom_infer_emotion(tom2, &conf2);

    EXPECT_EQ(emotion1, TOM_EMOTION_JOY);
    EXPECT_EQ(emotion2, TOM_EMOTION_SADNESS);

    tom_destroy(tom2);
}

//=============================================================================
// Test Suite: REAL Observation with Real ToM
//=============================================================================

TEST_F(TheoryOfMindRealTest, Observe_HappyExpression) {
    tom_observation_t obs = create_observation("I am so happy and excited!");
    bool success = tom_observe(tom, &obs);

    EXPECT_TRUE(success);

    // Verify emotion was inferred
    float confidence;
    tom_emotion_t emotion = tom_infer_emotion(tom, &confidence);
    EXPECT_EQ(emotion, TOM_EMOTION_JOY);
    EXPECT_GT(confidence, 0.0f);
}

TEST_F(TheoryOfMindRealTest, Observe_SadExpression) {
    tom_observation_t obs = create_observation("I feel so sad and depressed");
    tom_observe(tom, &obs);

    float confidence;
    tom_emotion_t emotion = tom_infer_emotion(tom, &confidence);
    EXPECT_EQ(emotion, TOM_EMOTION_SADNESS);
    EXPECT_GT(confidence, 0.0f);
}

TEST_F(TheoryOfMindRealTest, Observe_AngryExpression) {
    tom_observation_t obs = create_observation("I am so angry and furious!");
    tom_observe(tom, &obs);

    float confidence;
    tom_emotion_t emotion = tom_infer_emotion(tom, &confidence);
    EXPECT_EQ(emotion, TOM_EMOTION_ANGER);
}

TEST_F(TheoryOfMindRealTest, Observe_FearfulExpression) {
    tom_observation_t obs = create_observation("I am scared and afraid");
    tom_observe(tom, &obs);

    float confidence;
    tom_emotion_t emotion = tom_infer_emotion(tom, &confidence);
    EXPECT_EQ(emotion, TOM_EMOTION_FEAR);
}

TEST_F(TheoryOfMindRealTest, Observe_DirectEmotion) {
    // Observe emotion directly (not inferred from text)
    tom_observation_t obs = create_observation("", TOM_EMOTION_JOY);
    bool success = tom_observe(tom, &obs);

    EXPECT_TRUE(success);

    float confidence;
    tom_emotion_t emotion = tom_infer_emotion(tom, &confidence);
    EXPECT_EQ(emotion, TOM_EMOTION_JOY);
    EXPECT_GT(confidence, 0.5f);
}

//=============================================================================
// Test Suite: REAL Emotion Inference
//=============================================================================

TEST_F(TheoryOfMindRealTest, InferEmotion_AfterMultipleObservations) {
    // Multiple observations should update state
    tom_observation_t obs1 = create_observation("I am happy");
    tom_observation_t obs2 = create_observation("I am joyful");
    tom_observation_t obs3 = create_observation("I am excited");

    tom_observe(tom, &obs1);
    tom_observe(tom, &obs2);
    tom_observe(tom, &obs3);

    float confidence;
    tom_emotion_t emotion = tom_infer_emotion(tom, &confidence);

    // Should consistently infer joy
    EXPECT_EQ(emotion, TOM_EMOTION_JOY);
    EXPECT_GT(confidence, 0.0f);
}

TEST_F(TheoryOfMindRealTest, InferEmotion_AllEmotionTypes) {
    const char* emotion_texts[] = {
        "I am happy",           // JOY
        "I am sad",            // SADNESS
        "I am angry",          // ANGER
        "I am afraid",         // FEAR
        "I am anxious",        // ANXIETY
    };

    tom_emotion_t expected_emotions[] = {
        TOM_EMOTION_JOY,
        TOM_EMOTION_SADNESS,
        TOM_EMOTION_ANGER,
        TOM_EMOTION_FEAR,
        TOM_EMOTION_ANXIETY,
    };

    // Test each emotion type
    for (int i = 0; i < 5; i++) {
        // Reset for clean slate
        tom_reset(tom);

        tom_observation_t obs = create_observation(emotion_texts[i]);
        tom_observe(tom, &obs);

        float confidence;
        tom_emotion_t emotion = tom_infer_emotion(tom, &confidence);
        EXPECT_EQ(emotion, expected_emotions[i]);
    }
}

//=============================================================================
// Test Suite: REAL Goal Inference
//=============================================================================

TEST_F(TheoryOfMindRealTest, InferGoal_Want) {
    tom_observation_t obs = create_observation("I want some food");
    tom_observe(tom, &obs);

    char goal[256];
    float confidence;
    bool success = tom_infer_goal(tom, goal, sizeof(goal), &confidence);

    EXPECT_TRUE(success);
    EXPECT_STREQ(goal, "Satisfy expressed desire");
    EXPECT_GT(confidence, 0.0f);
}

TEST_F(TheoryOfMindRealTest, InferGoal_Need) {
    tom_observation_t obs = create_observation("I need help with this task");
    tom_observe(tom, &obs);

    char goal[256];
    float confidence;
    bool success = tom_infer_goal(tom, goal, sizeof(goal), &confidence);

    EXPECT_TRUE(success);
    EXPECT_STREQ(goal, "Satisfy expressed desire");
}

TEST_F(TheoryOfMindRealTest, InferGoal_Help) {
    tom_observation_t obs = create_observation("Can you help me?");
    tom_observe(tom, &obs);

    char goal[256];
    float confidence;
    bool success = tom_infer_goal(tom, goal, sizeof(goal), &confidence);

    EXPECT_TRUE(success);
    EXPECT_STREQ(goal, "Seek assistance");
}

TEST_F(TheoryOfMindRealTest, InferGoal_Question) {
    tom_observation_t obs = create_observation("What is the time?");
    tom_observe(tom, &obs);

    char goal[256];
    float confidence;
    bool success = tom_infer_goal(tom, goal, sizeof(goal), &confidence);

    EXPECT_TRUE(success);
    EXPECT_STREQ(goal, "Obtain information");
}

//=============================================================================
// Test Suite: REAL Action Prediction
//=============================================================================

TEST_F(TheoryOfMindRealTest, PredictAction_WithGoal) {
    tom_observation_t obs = create_observation("I want food");
    tom_observe(tom, &obs);

    char action[256];
    float likelihood;
    bool success = tom_predict_action(tom, action, sizeof(action), &likelihood);

    EXPECT_TRUE(success);
    EXPECT_GT(strlen(action), 0);
    EXPECT_GT(likelihood, 0.0f);
}

TEST_F(TheoryOfMindRealTest, PredictAction_NoGoal) {
    // No observations - should return unknown
    char action[256];
    float likelihood;
    bool success = tom_predict_action(tom, action, sizeof(action), &likelihood);

    EXPECT_TRUE(success);
    EXPECT_STREQ(action, "Unknown action");
    EXPECT_LT(likelihood, 0.2f);
}

//=============================================================================
// Test Suite: REAL Empathy
//=============================================================================

TEST_F(TheoryOfMindRealTest, Empathize_AllEmotions) {
    tom_emotion_t test_emotions[] = {
        TOM_EMOTION_JOY,
        TOM_EMOTION_SADNESS,
        TOM_EMOTION_ANGER,
        TOM_EMOTION_FEAR,
        TOM_EMOTION_ANXIETY,
        TOM_EMOTION_PRIDE,
        TOM_EMOTION_SHAME,
        TOM_EMOTION_NEUTRAL,
    };

    tom_emotion_t expected_responses[] = {
        TOM_EMOTION_JOY,      // Share joy
        TOM_EMOTION_SADNESS,  // Mirror sadness
        TOM_EMOTION_CALM,     // Calm response to anger
        TOM_EMOTION_CALM,     // Reassure fear
        TOM_EMOTION_CALM,     // Soothe anxiety
        TOM_EMOTION_JOY,      // Share pride
        TOM_EMOTION_CALM,     // Support shame
        TOM_EMOTION_NEUTRAL,  // Match neutral
    };

    for (size_t i = 0; i < sizeof(test_emotions) / sizeof(test_emotions[0]); i++) {
        tom_emotion_t response;
        bool success = tom_empathize(tom, test_emotions[i], &response);

        EXPECT_TRUE(success);
        EXPECT_EQ(response, expected_responses[i]);
    }
}

TEST_F(TheoryOfMindRealTest, Empathize_UpdatesStatistics) {
    tom_emotion_t response;
    tom_empathize(tom, TOM_EMOTION_JOY, &response);

    tom_statistics_t stats;
    tom_get_statistics(tom, &stats);

    EXPECT_GE(stats.empathy_responses, 1U);
}

//=============================================================================
// Test Suite: REAL False Belief Detection
//=============================================================================

TEST_F(TheoryOfMindRealTest, DetectFalseBelief_True) {
    bool is_false;
    bool success = tom_detect_false_belief(
        tom,
        "The box contains pencils",
        "The box contains candy",
        &is_false
    );

    EXPECT_TRUE(success);
    EXPECT_TRUE(is_false);  // Beliefs differ
}

TEST_F(TheoryOfMindRealTest, DetectFalseBelief_False) {
    bool is_false;
    bool success = tom_detect_false_belief(
        tom,
        "The box contains candy",
        "The box contains candy",
        &is_false
    );

    EXPECT_TRUE(success);
    EXPECT_FALSE(is_false);  // Beliefs match
}

TEST_F(TheoryOfMindRealTest, DetectFalseBelief_UpdatesPerspectiveScore) {
    float score_before = tom_get_perspective_score(tom);

    bool is_false;
    tom_detect_false_belief(tom, "reality", "false belief", &is_false);

    float score_after = tom_get_perspective_score(tom);

    // Score should increase (better perspective-taking)
    EXPECT_GE(score_after, score_before);
}

//=============================================================================
// Test Suite: REAL BDI State Access
//=============================================================================

TEST_F(TheoryOfMindRealTest, GetBdiState_AfterObservations) {
    tom_observation_t obs = create_observation("I want food");
    tom_observe(tom, &obs);

    tom_belief_t belief;
    tom_desire_t desire;
    tom_intention_t intention;

    bool success = tom_get_bdi_state(tom, &belief, &desire, &intention);
    EXPECT_TRUE(success);
}

TEST_F(TheoryOfMindRealTest, GetBdiState_PartialAccess) {
    tom_observation_t obs = create_observation("I am happy");
    tom_observe(tom, &obs);

    // Request only desire
    tom_desire_t desire;
    bool success = tom_get_bdi_state(tom, nullptr, &desire, nullptr);
    EXPECT_TRUE(success);
}

//=============================================================================
// Test Suite: REAL Statistics
//=============================================================================

TEST_F(TheoryOfMindRealTest, GetStatistics_AfterOperations) {
    // Perform various operations
    tom_observation_t obs1 = create_observation("I am happy");
    tom_observation_t obs2 = create_observation("I want help");

    tom_observe(tom, &obs1);
    tom_observe(tom, &obs2);

    float conf;
    tom_infer_emotion(tom, &conf);

    char goal[256];
    tom_infer_goal(tom, goal, sizeof(goal), nullptr);

    tom_emotion_t response;
    tom_empathize(tom, TOM_EMOTION_JOY, &response);

    // Get statistics
    tom_statistics_t stats;
    bool success = tom_get_statistics(tom, &stats);

    EXPECT_TRUE(success);
    EXPECT_EQ(stats.total_observations, 2U);
    EXPECT_GE(stats.emotion_inferences, 1U);
    EXPECT_GE(stats.goal_inferences, 1U);
    EXPECT_GE(stats.empathy_responses, 1U);
}

//=============================================================================
// Test Suite: REAL Reset
//=============================================================================

TEST_F(TheoryOfMindRealTest, Reset_ClearsState) {
    // Create state
    tom_observation_t obs = create_observation("I am very happy");
    tom_observe(tom, &obs);

    float conf;
    tom_emotion_t emotion = tom_infer_emotion(tom, &conf);
    EXPECT_EQ(emotion, TOM_EMOTION_JOY);

    // Reset
    bool success = tom_reset(tom);
    EXPECT_TRUE(success);

    // Check state is reset
    emotion = tom_infer_emotion(tom, &conf);
    EXPECT_EQ(emotion, TOM_EMOTION_NEUTRAL);
}

//=============================================================================
// Test Suite: REAL Integration Workflow
//=============================================================================

TEST_F(TheoryOfMindRealTest, CompleteInteractionWorkflow) {
    // 1. Observe emotion
    tom_observation_t obs = create_observation("I am so sad and lonely");
    tom_observe(tom, &obs);

    // 2. Infer emotion
    float confidence;
    tom_emotion_t emotion = tom_infer_emotion(tom, &confidence);
    EXPECT_EQ(emotion, TOM_EMOTION_SADNESS);

    // 3. Infer goal
    char goal[256];
    tom_infer_goal(tom, goal, sizeof(goal), nullptr);

    // 4. Predict action
    char action[256];
    tom_predict_action(tom, action, sizeof(action), nullptr);

    // 5. Generate empathy
    tom_emotion_t empathy_response;
    tom_empathize(tom, emotion, &empathy_response);
    EXPECT_EQ(empathy_response, TOM_EMOTION_SADNESS);

    // 6. Check statistics
    tom_statistics_t stats;
    tom_get_statistics(tom, &stats);
    EXPECT_GE(stats.total_observations, 1U);
    EXPECT_GE(stats.empathy_responses, 1U);
}

TEST_F(TheoryOfMindRealTest, MultipleAgentModeling) {
    // Model different agents with different mental states
    const char* agents[] = {
        "I am happy",
        "I am sad",
        "I am angry"
    };

    tom_emotion_t expected[] = {
        TOM_EMOTION_JOY,
        TOM_EMOTION_SADNESS,
        TOM_EMOTION_ANGER
    };

    for (int i = 0; i < 3; i++) {
        // Reset for new agent
        tom_reset(tom);

        // Observe
        tom_observation_t obs = create_observation(agents[i]);
        tom_observe(tom, &obs);

        // Infer
        float conf;
        tom_emotion_t emotion = tom_infer_emotion(tom, &conf);
        EXPECT_EQ(emotion, expected[i]);
    }
}

//=============================================================================
// Test Suite: NULL Guards (still important for safety)
//=============================================================================

TEST_F(TheoryOfMindRealTest, NullGuard_CreateNull) {
    theory_of_mind_t null_tom = tom_create(nullptr);
    // May or may not allow NULL brain
    if (null_tom) {
        tom_destroy(null_tom);
    }
    SUCCEED();
}

TEST_F(TheoryOfMindRealTest, NullGuard_DestroyNull) {
    tom_destroy(nullptr);
    SUCCEED();
}

TEST_F(TheoryOfMindRealTest, NullGuard_ObserveNull) {
    tom_observation_t obs = create_observation("test");
    bool success = tom_observe(nullptr, &obs);
    EXPECT_FALSE(success);
}

TEST_F(TheoryOfMindRealTest, NullGuard_InferEmotionNull) {
    float confidence;
    tom_emotion_t emotion = tom_infer_emotion(nullptr, &confidence);
    EXPECT_EQ(emotion, TOM_EMOTION_UNKNOWN);
    EXPECT_FLOAT_EQ(confidence, 0.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
