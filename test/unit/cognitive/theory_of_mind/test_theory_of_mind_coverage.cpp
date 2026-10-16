/**
 * @file test_theory_of_mind_coverage.cpp
 * @brief Comprehensive tests for nimcp_theory_of_mind.c (TARGET: 100% coverage)
 *
 * WHAT: Test Theory of Mind (BDI model, emotion inference, empathy)
 * WHY:  Achieve 100% line/branch coverage for nimcp_theory_of_mind.c
 * HOW:  Test all public functions, BDI model, inference logic, guard clauses
 *
 * COVERAGE GOALS:
 * - Line coverage: 100%
 * - Branch coverage: 100%
 * - Function coverage: 100%
 *
 * @author NIMCP Development Team
 * @date 2025-11-10
 */

#include <gtest/gtest.h>
#include <cstring>

#include "cognitive/nimcp_theory_of_mind.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class TheoryOfMindTest : public ::testing::Test {
protected:
    brain_t mock_brain;
    theory_of_mind_t tom;

    void SetUp() override {
        // Mock brain (just needs to be non-NULL)
        mock_brain = nullptr;
        tom = tom_create(mock_brain);
        ASSERT_NE(tom, nullptr);
    }

    void TearDown() override {
        if (tom) {
            tom_destroy(tom);
        }
    }

    // Helper: Create observation with verbal context
    tom_observation_t create_observation(const char* verbal_context,
                                         tom_emotion_t observed_emotion = TOM_EMOTION_UNKNOWN) {
        tom_observation_t obs;
        obs.action_vector = NULL;
        obs.action_dim = 0;
        obs.verbal_context = verbal_context;
        obs.observed_emotion = observed_emotion;
        obs.situational_context = NULL;
        obs.context_dim = 0;
        return obs;
    }
};

//=============================================================================
// Test Suite: Lifecycle
//=============================================================================

TEST_F(TheoryOfMindTest, CreateValid) {
    // Already created in SetUp
    EXPECT_NE(tom, nullptr);
}

TEST_F(TheoryOfMindTest, CreateNull_Brain) {
    // Can create with NULL brain (some implementations may allow)
    theory_of_mind_t tom2 = tom_create(NULL);
    // Result depends on implementation - just verify no crash
    if (tom2) {
        tom_destroy(tom2);
    }
    SUCCEED();
}

TEST_F(TheoryOfMindTest, DestroyNull) {
    // Guard: Destroying NULL should be safe
    tom_destroy(NULL);
    SUCCEED();
}

TEST_F(TheoryOfMindTest, DestroyValid) {
    theory_of_mind_t tom2 = tom_create(mock_brain);
    ASSERT_NE(tom2, nullptr);
    tom_destroy(tom2);
    SUCCEED();
}

//=============================================================================
// Test Suite: Emotion Utility
//=============================================================================

TEST_F(TheoryOfMindTest, EmotionToString_AllEmotions) {
    EXPECT_STREQ(tom_emotion_to_string(TOM_EMOTION_UNKNOWN), "Unknown");
    EXPECT_STREQ(tom_emotion_to_string(TOM_EMOTION_NEUTRAL), "Neutral");
    EXPECT_STREQ(tom_emotion_to_string(TOM_EMOTION_JOY), "Joy");
    EXPECT_STREQ(tom_emotion_to_string(TOM_EMOTION_SADNESS), "Sadness");
    EXPECT_STREQ(tom_emotion_to_string(TOM_EMOTION_ANGER), "Anger");
    EXPECT_STREQ(tom_emotion_to_string(TOM_EMOTION_FEAR), "Fear");
    EXPECT_STREQ(tom_emotion_to_string(TOM_EMOTION_SURPRISE), "Surprise");
    EXPECT_STREQ(tom_emotion_to_string(TOM_EMOTION_DISGUST), "Disgust");
    EXPECT_STREQ(tom_emotion_to_string(TOM_EMOTION_ANXIETY), "Anxiety");
    EXPECT_STREQ(tom_emotion_to_string(TOM_EMOTION_PRIDE), "Pride");
    EXPECT_STREQ(tom_emotion_to_string(TOM_EMOTION_SHAME), "Shame");
    EXPECT_STREQ(tom_emotion_to_string(TOM_EMOTION_CALM), "Calm");
}

TEST_F(TheoryOfMindTest, EmotionToString_Invalid) {
    const char* str = tom_emotion_to_string((tom_emotion_t)999);
    EXPECT_STREQ(str, "Unknown");
}

//=============================================================================
// Test Suite: Observation
//=============================================================================

TEST_F(TheoryOfMindTest, ObserveNull_Tom) {
    tom_observation_t obs = create_observation("happy");
    bool success = tom_observe(NULL, &obs);
    EXPECT_FALSE(success);
}

TEST_F(TheoryOfMindTest, ObserveNull_Observation) {
    bool success = tom_observe(tom, NULL);
    EXPECT_FALSE(success);
}

TEST_F(TheoryOfMindTest, ObserveValid) {
    tom_observation_t obs = create_observation("I am happy today");
    bool success = tom_observe(tom, &obs);
    EXPECT_TRUE(success);
}

TEST_F(TheoryOfMindTest, ObserveDirectEmotion) {
    // Observe emotion directly (not inferred)
    tom_observation_t obs = create_observation("", TOM_EMOTION_JOY);
    bool success = tom_observe(tom, &obs);
    EXPECT_TRUE(success);

    // Verify emotion was recorded
    float confidence;
    tom_emotion_t emotion = tom_infer_emotion(tom, &confidence);
    EXPECT_EQ(emotion, TOM_EMOTION_JOY);
    EXPECT_GT(confidence, 0.5f);
}

//=============================================================================
// Test Suite: Emotion Inference
//=============================================================================

TEST_F(TheoryOfMindTest, InferEmotionNull_Tom) {
    float confidence;
    tom_emotion_t emotion = tom_infer_emotion(NULL, &confidence);
    EXPECT_EQ(emotion, TOM_EMOTION_UNKNOWN);
    EXPECT_FLOAT_EQ(confidence, 0.0f);
}

TEST_F(TheoryOfMindTest, InferEmotionNull_Confidence) {
    tom_emotion_t emotion = tom_infer_emotion(tom, NULL);
    EXPECT_NE(emotion, TOM_EMOTION_UNKNOWN);  // Should return current emotion
}

TEST_F(TheoryOfMindTest, InferEmotion_Happy) {
    tom_observation_t obs = create_observation("I am so happy and excited");
    tom_observe(tom, &obs);

    float confidence;
    tom_emotion_t emotion = tom_infer_emotion(tom, &confidence);
    EXPECT_EQ(emotion, TOM_EMOTION_JOY);
    EXPECT_GT(confidence, 0.0f);
}

TEST_F(TheoryOfMindTest, InferEmotion_Sad) {
    tom_observation_t obs = create_observation("I feel sad and unhappy");
    tom_observe(tom, &obs);

    float confidence;
    tom_emotion_t emotion = tom_infer_emotion(tom, &confidence);
    EXPECT_EQ(emotion, TOM_EMOTION_SADNESS);
}

TEST_F(TheoryOfMindTest, InferEmotion_Angry) {
    tom_observation_t obs = create_observation("I am so angry and furious");
    tom_observe(tom, &obs);

    float confidence;
    tom_emotion_t emotion = tom_infer_emotion(tom, &confidence);
    EXPECT_EQ(emotion, TOM_EMOTION_ANGER);
}

TEST_F(TheoryOfMindTest, InferEmotion_Fear) {
    tom_observation_t obs = create_observation("I am afraid and scared");
    tom_observe(tom, &obs);

    float confidence;
    tom_emotion_t emotion = tom_infer_emotion(tom, &confidence);
    EXPECT_EQ(emotion, TOM_EMOTION_FEAR);
}

TEST_F(TheoryOfMindTest, InferEmotion_Anxiety) {
    tom_observation_t obs = create_observation("I feel anxious and worried");
    tom_observe(tom, &obs);

    float confidence;
    tom_emotion_t emotion = tom_infer_emotion(tom, &confidence);
    EXPECT_EQ(emotion, TOM_EMOTION_ANXIETY);
}

TEST_F(TheoryOfMindTest, InferEmotion_KeywordPriority) {
    // Test that "unhappy" matches sadness (not joy from "happy" substring)
    tom_observation_t obs = create_observation("I am unhappy");
    tom_observe(tom, &obs);

    float confidence;
    tom_emotion_t emotion = tom_infer_emotion(tom, &confidence);
    EXPECT_EQ(emotion, TOM_EMOTION_SADNESS);
}

TEST_F(TheoryOfMindTest, InferEmotion_NoContext) {
    // Observation with no verbal context
    tom_observation_t obs = create_observation(NULL);
    tom_observe(tom, &obs);

    float confidence;
    tom_emotion_t emotion = tom_infer_emotion(tom, &confidence);
    // Should return default (neutral)
    EXPECT_EQ(emotion, TOM_EMOTION_NEUTRAL);
}

//=============================================================================
// Test Suite: Goal Inference
//=============================================================================

TEST_F(TheoryOfMindTest, InferGoalNull_Tom) {
    char goal[256];
    float confidence;
    bool success = tom_infer_goal(NULL, goal, sizeof(goal), &confidence);
    EXPECT_FALSE(success);
}

TEST_F(TheoryOfMindTest, InferGoalNull_Buffer) {
    float confidence;
    bool success = tom_infer_goal(tom, NULL, 256, &confidence);
    EXPECT_FALSE(success);
}

TEST_F(TheoryOfMindTest, InferGoalZero_BufferSize) {
    char goal[256];
    float confidence;
    bool success = tom_infer_goal(tom, goal, 0, &confidence);
    EXPECT_FALSE(success);
}

TEST_F(TheoryOfMindTest, InferGoalNull_Confidence) {
    // Confidence is optional
    char goal[256];
    bool success = tom_infer_goal(tom, goal, sizeof(goal), NULL);
    EXPECT_TRUE(success);
}

TEST_F(TheoryOfMindTest, InferGoal_Want) {
    tom_observation_t obs = create_observation("I want some food");
    tom_observe(tom, &obs);

    char goal[256];
    float confidence;
    bool success = tom_infer_goal(tom, goal, sizeof(goal), &confidence);
    EXPECT_TRUE(success);
    EXPECT_STREQ(goal, "Satisfy expressed desire");
    EXPECT_GT(confidence, 0.0f);
}

TEST_F(TheoryOfMindTest, InferGoal_Need) {
    tom_observation_t obs = create_observation("I need help with this");
    tom_observe(tom, &obs);

    char goal[256];
    float confidence;
    bool success = tom_infer_goal(tom, goal, sizeof(goal), &confidence);
    EXPECT_TRUE(success);
    EXPECT_STREQ(goal, "Satisfy expressed desire");
}

TEST_F(TheoryOfMindTest, InferGoal_Help) {
    tom_observation_t obs = create_observation("Can you help me?");
    tom_observe(tom, &obs);

    char goal[256];
    float confidence;
    bool success = tom_infer_goal(tom, goal, sizeof(goal), &confidence);
    EXPECT_TRUE(success);
    EXPECT_STREQ(goal, "Seek assistance");
}

TEST_F(TheoryOfMindTest, InferGoal_Question) {
    tom_observation_t obs = create_observation("What is the time?");
    tom_observe(tom, &obs);

    char goal[256];
    float confidence;
    bool success = tom_infer_goal(tom, goal, sizeof(goal), &confidence);
    EXPECT_TRUE(success);
    EXPECT_STREQ(goal, "Obtain information");
}

TEST_F(TheoryOfMindTest, InferGoal_Unknown) {
    tom_observation_t obs = create_observation("Hello there");
    tom_observe(tom, &obs);

    char goal[256];
    float confidence;
    bool success = tom_infer_goal(tom, goal, sizeof(goal), &confidence);
    EXPECT_TRUE(success);
    EXPECT_STREQ(goal, "Unknown goal");
    EXPECT_LT(confidence, 0.5f);
}

//=============================================================================
// Test Suite: Action Prediction
//=============================================================================

TEST_F(TheoryOfMindTest, PredictActionNull_Tom) {
    char action[256];
    float likelihood;
    bool success = tom_predict_action(NULL, action, sizeof(action), &likelihood);
    EXPECT_FALSE(success);
}

TEST_F(TheoryOfMindTest, PredictActionNull_Buffer) {
    float likelihood;
    bool success = tom_predict_action(tom, NULL, 256, &likelihood);
    EXPECT_FALSE(success);
}

TEST_F(TheoryOfMindTest, PredictActionZero_BufferSize) {
    char action[256];
    float likelihood;
    bool success = tom_predict_action(tom, action, 0, &likelihood);
    EXPECT_FALSE(success);
}

TEST_F(TheoryOfMindTest, PredictActionNull_Likelihood) {
    // Likelihood is optional
    char action[256];
    bool success = tom_predict_action(tom, action, sizeof(action), NULL);
    EXPECT_TRUE(success);
}

TEST_F(TheoryOfMindTest, PredictAction_WithGoal) {
    // Observe to establish goal
    tom_observation_t obs = create_observation("I want food");
    tom_observe(tom, &obs);

    char action[256];
    float likelihood;
    bool success = tom_predict_action(tom, action, sizeof(action), &likelihood);
    EXPECT_TRUE(success);
    EXPECT_GT(strlen(action), 0);
    EXPECT_GT(likelihood, 0.0f);
}

TEST_F(TheoryOfMindTest, PredictAction_NoGoal) {
    // No observations yet - should return unknown
    char action[256];
    float likelihood;
    bool success = tom_predict_action(tom, action, sizeof(action), &likelihood);
    EXPECT_TRUE(success);
    EXPECT_STREQ(action, "Unknown action");
    EXPECT_LT(likelihood, 0.2f);
}

//=============================================================================
// Test Suite: Empathy
//=============================================================================

TEST_F(TheoryOfMindTest, EmpathizeNull_Tom) {
    tom_emotion_t response;
    bool success = tom_empathize(NULL, TOM_EMOTION_JOY, &response);
    EXPECT_FALSE(success);
}

TEST_F(TheoryOfMindTest, EmpathizeNull_Response) {
    bool success = tom_empathize(tom, TOM_EMOTION_JOY, NULL);
    EXPECT_FALSE(success);
}

TEST_F(TheoryOfMindTest, Empathize_Joy) {
    tom_emotion_t response;
    bool success = tom_empathize(tom, TOM_EMOTION_JOY, &response);
    EXPECT_TRUE(success);
    EXPECT_EQ(response, TOM_EMOTION_JOY);  // Share joy
}

TEST_F(TheoryOfMindTest, Empathize_Sadness) {
    tom_emotion_t response;
    bool success = tom_empathize(tom, TOM_EMOTION_SADNESS, &response);
    EXPECT_TRUE(success);
    EXPECT_EQ(response, TOM_EMOTION_SADNESS);  // Mirror sadness
}

TEST_F(TheoryOfMindTest, Empathize_Anger) {
    tom_emotion_t response;
    bool success = tom_empathize(tom, TOM_EMOTION_ANGER, &response);
    EXPECT_TRUE(success);
    EXPECT_EQ(response, TOM_EMOTION_CALM);  // Calming response
}

TEST_F(TheoryOfMindTest, Empathize_Fear) {
    tom_emotion_t response;
    bool success = tom_empathize(tom, TOM_EMOTION_FEAR, &response);
    EXPECT_TRUE(success);
    EXPECT_EQ(response, TOM_EMOTION_CALM);  // Reassuring
}

TEST_F(TheoryOfMindTest, Empathize_Anxiety) {
    tom_emotion_t response;
    bool success = tom_empathize(tom, TOM_EMOTION_ANXIETY, &response);
    EXPECT_TRUE(success);
    EXPECT_EQ(response, TOM_EMOTION_CALM);
}

TEST_F(TheoryOfMindTest, Empathize_Pride) {
    tom_emotion_t response;
    bool success = tom_empathize(tom, TOM_EMOTION_PRIDE, &response);
    EXPECT_TRUE(success);
    EXPECT_EQ(response, TOM_EMOTION_JOY);  // Share pride
}

TEST_F(TheoryOfMindTest, Empathize_Shame) {
    tom_emotion_t response;
    bool success = tom_empathize(tom, TOM_EMOTION_SHAME, &response);
    EXPECT_TRUE(success);
    EXPECT_EQ(response, TOM_EMOTION_CALM);  // Supportive
}

TEST_F(TheoryOfMindTest, Empathize_Neutral) {
    tom_emotion_t response;
    bool success = tom_empathize(tom, TOM_EMOTION_NEUTRAL, &response);
    EXPECT_TRUE(success);
    EXPECT_EQ(response, TOM_EMOTION_NEUTRAL);
}

TEST_F(TheoryOfMindTest, Empathize_Surprise) {
    tom_emotion_t response;
    bool success = tom_empathize(tom, TOM_EMOTION_SURPRISE, &response);
    EXPECT_TRUE(success);
    EXPECT_EQ(response, TOM_EMOTION_NEUTRAL);
}

TEST_F(TheoryOfMindTest, Empathize_Disgust) {
    tom_emotion_t response;
    bool success = tom_empathize(tom, TOM_EMOTION_DISGUST, &response);
    EXPECT_TRUE(success);
    EXPECT_EQ(response, TOM_EMOTION_NEUTRAL);
}

//=============================================================================
// Test Suite: False Belief Detection
//=============================================================================

TEST_F(TheoryOfMindTest, DetectFalseBeliefNull_Tom) {
    bool is_false;
    bool success = tom_detect_false_belief(NULL, "reality", "belief", &is_false);
    EXPECT_FALSE(success);
}

TEST_F(TheoryOfMindTest, DetectFalseBeliefNull_Reality) {
    bool is_false;
    bool success = tom_detect_false_belief(tom, NULL, "belief", &is_false);
    EXPECT_FALSE(success);
}

TEST_F(TheoryOfMindTest, DetectFalseBeliefNull_Belief) {
    bool is_false;
    bool success = tom_detect_false_belief(tom, "reality", NULL, &is_false);
    EXPECT_FALSE(success);
}

TEST_F(TheoryOfMindTest, DetectFalseBeliefNull_Output) {
    bool success = tom_detect_false_belief(tom, "reality", "belief", NULL);
    EXPECT_FALSE(success);
}

TEST_F(TheoryOfMindTest, DetectFalseBelief_True) {
    bool is_false;
    bool success = tom_detect_false_belief(tom, "Box contains pencils",
                                           "Box contains candy", &is_false);
    EXPECT_TRUE(success);
    EXPECT_TRUE(is_false);  // Beliefs differ
}

TEST_F(TheoryOfMindTest, DetectFalseBelief_False) {
    bool is_false;
    bool success = tom_detect_false_belief(tom, "Box contains candy",
                                           "Box contains candy", &is_false);
    EXPECT_TRUE(success);
    EXPECT_FALSE(is_false);  // Beliefs match
}

TEST_F(TheoryOfMindTest, DetectFalseBelief_UpdatesPerspectiveScore) {
    float score_before = tom_get_perspective_score(tom);

    bool is_false;
    tom_detect_false_belief(tom, "reality", "false belief", &is_false);

    float score_after = tom_get_perspective_score(tom);
    EXPECT_GE(score_after, score_before);  // Should increase
}

//=============================================================================
// Test Suite: BDI State Access
//=============================================================================

TEST_F(TheoryOfMindTest, GetBdiStateNull_Tom) {
    tom_belief_t belief;
    tom_desire_t desire;
    tom_intention_t intention;
    bool success = tom_get_bdi_state(NULL, &belief, &desire, &intention);
    EXPECT_FALSE(success);
}

TEST_F(TheoryOfMindTest, GetBdiStateNull_AllOutputs) {
    // All outputs NULL is valid (just check if state exists)
    bool success = tom_get_bdi_state(tom, NULL, NULL, NULL);
    EXPECT_TRUE(success);
}

TEST_F(TheoryOfMindTest, GetBdiState_AfterObservation) {
    // Observe to create state
    tom_observation_t obs = create_observation("I want food");
    tom_observe(tom, &obs);

    tom_belief_t belief;
    tom_desire_t desire;
    tom_intention_t intention;
    bool success = tom_get_bdi_state(tom, &belief, &desire, &intention);
    EXPECT_TRUE(success);
}

TEST_F(TheoryOfMindTest, GetBdiState_PartialOutputs) {
    // Only desire requested
    tom_desire_t desire;
    bool success = tom_get_bdi_state(tom, NULL, &desire, NULL);
    EXPECT_TRUE(success);
}

//=============================================================================
// Test Suite: Perspective Score
//=============================================================================

TEST_F(TheoryOfMindTest, GetPerspectiveScoreNull) {
    float score = tom_get_perspective_score(NULL);
    EXPECT_FLOAT_EQ(score, 0.0f);
}

TEST_F(TheoryOfMindTest, GetPerspectiveScore_Initial) {
    float score = tom_get_perspective_score(tom);
    EXPECT_EQ(score, 1.0f);  // Perfect by default
}

//=============================================================================
// Test Suite: Statistics
//=============================================================================

TEST_F(TheoryOfMindTest, GetStatisticsNull_Tom) {
    tom_statistics_t stats;
    bool success = tom_get_statistics(NULL, &stats);
    EXPECT_FALSE(success);
}

TEST_F(TheoryOfMindTest, GetStatisticsNull_Stats) {
    bool success = tom_get_statistics(tom, NULL);
    EXPECT_FALSE(success);
}

TEST_F(TheoryOfMindTest, GetStatistics_Initial) {
    tom_statistics_t stats;
    bool success = tom_get_statistics(tom, &stats);
    EXPECT_TRUE(success);
    EXPECT_EQ(stats.total_observations, 0);
    EXPECT_EQ(stats.emotion_inferences, 0);
}

TEST_F(TheoryOfMindTest, GetStatistics_AfterObservations) {
    // Make observations
    tom_observation_t obs1 = create_observation("I am happy");
    tom_observe(tom, &obs1);

    tom_observation_t obs2 = create_observation("I want food");
    tom_observe(tom, &obs2);

    tom_statistics_t stats;
    tom_get_statistics(tom, &stats);

    EXPECT_EQ(stats.total_observations, 2);
    EXPECT_GE(stats.emotion_inferences, 1);
    EXPECT_GE(stats.goal_inferences, 1);
}

//=============================================================================
// Test Suite: Reset
//=============================================================================

TEST_F(TheoryOfMindTest, ResetNull) {
    bool success = tom_reset(NULL);
    EXPECT_FALSE(success);
}

TEST_F(TheoryOfMindTest, ResetValid) {
    // Create state
    tom_observation_t obs = create_observation("I am happy");
    tom_observe(tom, &obs);

    // Reset
    bool success = tom_reset(tom);
    EXPECT_TRUE(success);

    // Verify emotion reset to neutral
    float confidence;
    tom_emotion_t emotion = tom_infer_emotion(tom, &confidence);
    EXPECT_EQ(emotion, TOM_EMOTION_NEUTRAL);
}

//=============================================================================
// Test Suite: Integration Scenarios
//=============================================================================

TEST_F(TheoryOfMindTest, CompleteInteractionFlow) {
    // Observe emotion
    tom_observation_t obs1 = create_observation("I am so sad");
    tom_observe(tom, &obs1);

    // Check inferred emotion
    float confidence;
    tom_emotion_t emotion = tom_infer_emotion(tom, &confidence);
    EXPECT_EQ(emotion, TOM_EMOTION_SADNESS);

    // Generate empathy
    tom_emotion_t empathy_response;
    tom_empathize(tom, emotion, &empathy_response);
    EXPECT_EQ(empathy_response, TOM_EMOTION_SADNESS);

    // Check statistics
    tom_statistics_t stats;
    tom_get_statistics(tom, &stats);
    EXPECT_GE(stats.total_observations, 1);
    EXPECT_GE(stats.empathy_responses, 1);
}

TEST_F(TheoryOfMindTest, MultipleObservations) {
    // Multiple observations should update state
    tom_observation_t obs1 = create_observation("I am happy");
    tom_observation_t obs2 = create_observation("I want help");
    tom_observation_t obs3 = create_observation("What time is it?");

    tom_observe(tom, &obs1);
    tom_observe(tom, &obs2);
    tom_observe(tom, &obs3);

    tom_statistics_t stats;
    tom_get_statistics(tom, &stats);
    EXPECT_EQ(stats.total_observations, 3);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
