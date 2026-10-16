/**
 * @file test_theory_of_mind.cpp
 * @brief Comprehensive tests for Theory of Mind module
 *
 * WHAT: Test BDI model, emotion/goal inference, empathy, false beliefs
 * WHY:  Verify social cognition capabilities
 * HOW:  GTest framework with fixtures for setup/teardown
 *
 * TEST COVERAGE:
 * 1. Creation/destruction
 * 2. Observation processing
 * 3. Emotion inference
 * 4. Goal inference
 * 5. Action prediction
 * 6. Empathy generation
 * 7. False belief understanding
 * 8. BDI state management
 * 9. Perspective-taking
 * 10. Statistics and diagnostics
 *
 * @author NIMCP Development Team - Phase 10.6
 * @date 2025-11-09
 */

#include <gtest/gtest.h>

    #include "cognitive/nimcp_theory_of_mind.h"
    #include "utils/time/nimcp_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TheoryOfMindTest : public ::testing::Test {
protected:
    theory_of_mind_t tom;
    brain_t mock_brain;

    void SetUp() override {
        // Mock brain (NULL is acceptable for ToM)
        mock_brain = nullptr;

        // Create ToM instance
        tom = tom_create(mock_brain);
        ASSERT_NE(tom, nullptr) << "Failed to create Theory of Mind instance";
    }

    void TearDown() override {
        if (tom) {
            tom_destroy(tom);
            tom = nullptr;
        }
    }

    // Helper: Create observation with verbal context
    tom_observation_t create_verbal_observation(const char* text, tom_emotion_t emotion = TOM_EMOTION_UNKNOWN) {
        tom_observation_t obs = {};
        obs.verbal_context = text;
        obs.observed_emotion = emotion;
        obs.action_vector = nullptr;
        obs.action_dim = 0;
        obs.situational_context = nullptr;
        obs.context_dim = 0;
        return obs;
    }
};

//=============================================================================
// 1. Creation/Destruction Tests
//=============================================================================

TEST_F(TheoryOfMindTest, CreateDestroy) {
    // Created in SetUp, destroyed in TearDown
    EXPECT_NE(tom, nullptr);

    // Test statistics are initialized
    tom_statistics_t stats;
    ASSERT_TRUE(tom_get_statistics(tom, &stats));
    EXPECT_EQ(stats.total_observations, 0);
    EXPECT_EQ(stats.emotion_inferences, 0);
    EXPECT_EQ(stats.goal_inferences, 0);
}

TEST_F(TheoryOfMindTest, CreateWithNullBrain) {
    theory_of_mind_t tom2 = tom_create(nullptr);
    EXPECT_NE(tom2, nullptr) << "ToM should work without brain reference";
    tom_destroy(tom2);
}

TEST_F(TheoryOfMindTest, DestroyNull) {
    tom_destroy(nullptr);  // Should not crash
}

//=============================================================================
// 2. Observation Processing Tests
//=============================================================================

TEST_F(TheoryOfMindTest, ObserveBasic) {
    tom_observation_t obs = create_verbal_observation("Hello world");

    EXPECT_TRUE(tom_observe(tom, &obs));

    // Check statistics updated
    tom_statistics_t stats;
    ASSERT_TRUE(tom_get_statistics(tom, &stats));
    EXPECT_EQ(stats.total_observations, 1);
}

TEST_F(TheoryOfMindTest, ObserveMultiple) {
    for (int i = 0; i < 10; i++) {
        tom_observation_t obs = create_verbal_observation("Test observation");
        EXPECT_TRUE(tom_observe(tom, &obs));
    }

    tom_statistics_t stats;
    ASSERT_TRUE(tom_get_statistics(tom, &stats));
    EXPECT_EQ(stats.total_observations, 10);
}

TEST_F(TheoryOfMindTest, ObserveNull) {
    EXPECT_FALSE(tom_observe(tom, nullptr));
}

TEST_F(TheoryOfMindTest, ObserveNullToM) {
    tom_observation_t obs = create_verbal_observation("Test");
    EXPECT_FALSE(tom_observe(nullptr, &obs));
}

//=============================================================================
// 3. Emotion Inference Tests
//=============================================================================

TEST_F(TheoryOfMindTest, InferEmotionFromKeywords) {
    // Test joy keywords
    tom_observation_t obs = create_verbal_observation("I am very happy and excited!");
    ASSERT_TRUE(tom_observe(tom, &obs));

    float confidence = 0.0f;
    tom_emotion_t emotion = tom_infer_emotion(tom, &confidence);
    EXPECT_EQ(emotion, TOM_EMOTION_JOY);
    EXPECT_GT(confidence, 0.5f);
}

TEST_F(TheoryOfMindTest, InferSadness) {
    tom_observation_t obs = create_verbal_observation("I feel so sad and unhappy");
    ASSERT_TRUE(tom_observe(tom, &obs));

    float confidence = 0.0f;
    tom_emotion_t emotion = tom_infer_emotion(tom, &confidence);
    EXPECT_EQ(emotion, TOM_EMOTION_SADNESS);
    EXPECT_GT(confidence, 0.5f);
}

TEST_F(TheoryOfMindTest, InferAnger) {
    tom_observation_t obs = create_verbal_observation("I am so angry and furious!");
    ASSERT_TRUE(tom_observe(tom, &obs));

    float confidence = 0.0f;
    tom_emotion_t emotion = tom_infer_emotion(tom, &confidence);
    EXPECT_EQ(emotion, TOM_EMOTION_ANGER);
    EXPECT_GT(confidence, 0.5f);
}

TEST_F(TheoryOfMindTest, InferFear) {
    tom_observation_t obs = create_verbal_observation("I am afraid and scared");
    ASSERT_TRUE(tom_observe(tom, &obs));

    float confidence = 0.0f;
    tom_emotion_t emotion = tom_infer_emotion(tom, &confidence);
    EXPECT_EQ(emotion, TOM_EMOTION_FEAR);
    EXPECT_GT(confidence, 0.5f);
}

TEST_F(TheoryOfMindTest, InferAnxiety) {
    tom_observation_t obs = create_verbal_observation("I'm so worried and anxious");
    ASSERT_TRUE(tom_observe(tom, &obs));

    float confidence = 0.0f;
    tom_emotion_t emotion = tom_infer_emotion(tom, &confidence);
    EXPECT_EQ(emotion, TOM_EMOTION_ANXIETY);
    EXPECT_GT(confidence, 0.5f);
}

TEST_F(TheoryOfMindTest, InferEmotionFromObserved) {
    // Direct emotion observation
    tom_observation_t obs = create_verbal_observation("Neutral text", TOM_EMOTION_JOY);
    ASSERT_TRUE(tom_observe(tom, &obs));

    float confidence = 0.0f;
    tom_emotion_t emotion = tom_infer_emotion(tom, &confidence);
    EXPECT_EQ(emotion, TOM_EMOTION_JOY);
    EXPECT_GT(confidence, 0.8f);  // High confidence for direct observation
}

TEST_F(TheoryOfMindTest, InferEmotionNeutral) {
    tom_observation_t obs = create_verbal_observation("The weather is fine");
    ASSERT_TRUE(tom_observe(tom, &obs));

    float confidence = 0.0f;
    tom_emotion_t emotion = tom_infer_emotion(tom, &confidence);
    EXPECT_EQ(emotion, TOM_EMOTION_NEUTRAL);
}

TEST_F(TheoryOfMindTest, InferEmotionNull) {
    float confidence = 0.0f;
    tom_emotion_t emotion = tom_infer_emotion(nullptr, &confidence);
    EXPECT_EQ(emotion, TOM_EMOTION_UNKNOWN);
    EXPECT_EQ(confidence, 0.0f);
}

//=============================================================================
// 4. Goal Inference Tests
//=============================================================================

TEST_F(TheoryOfMindTest, InferGoalFromWant) {
    tom_observation_t obs = create_verbal_observation("I want to get some water");
    ASSERT_TRUE(tom_observe(tom, &obs));

    char goal[256];
    float confidence = 0.0f;
    ASSERT_TRUE(tom_infer_goal(tom, goal, sizeof(goal), &confidence));
    EXPECT_STREQ(goal, "Satisfy expressed desire");
    EXPECT_GT(confidence, 0.5f);
}

TEST_F(TheoryOfMindTest, InferGoalFromNeed) {
    tom_observation_t obs = create_verbal_observation("I need help with this task");
    ASSERT_TRUE(tom_observe(tom, &obs));

    char goal[256];
    float confidence = 0.0f;
    ASSERT_TRUE(tom_infer_goal(tom, goal, sizeof(goal), &confidence));
    EXPECT_STREQ(goal, "Satisfy expressed desire");
    EXPECT_GT(confidence, 0.5f);
}

TEST_F(TheoryOfMindTest, InferGoalFromHelp) {
    tom_observation_t obs = create_verbal_observation("Can you help me?");
    ASSERT_TRUE(tom_observe(tom, &obs));

    char goal[256];
    float confidence = 0.0f;
    ASSERT_TRUE(tom_infer_goal(tom, goal, sizeof(goal), &confidence));
    EXPECT_STREQ(goal, "Seek assistance");
    EXPECT_GT(confidence, 0.5f);
}

TEST_F(TheoryOfMindTest, InferGoalFromQuestion) {
    tom_observation_t obs = create_verbal_observation("What is the time?");
    ASSERT_TRUE(tom_observe(tom, &obs));

    char goal[256];
    float confidence = 0.0f;
    ASSERT_TRUE(tom_infer_goal(tom, goal, sizeof(goal), &confidence));
    EXPECT_STREQ(goal, "Obtain information");
    EXPECT_GT(confidence, 0.5f);
}

TEST_F(TheoryOfMindTest, InferGoalUnknown) {
    tom_observation_t obs = create_verbal_observation("Random statement");
    ASSERT_TRUE(tom_observe(tom, &obs));

    char goal[256];
    float confidence = 0.0f;
    ASSERT_TRUE(tom_infer_goal(tom, goal, sizeof(goal), &confidence));
    EXPECT_STREQ(goal, "Unknown goal");
    EXPECT_LT(confidence, 0.5f);
}

TEST_F(TheoryOfMindTest, InferGoalNull) {
    char goal[256];
    float confidence = 0.0f;
    EXPECT_FALSE(tom_infer_goal(nullptr, goal, sizeof(goal), &confidence));
}

//=============================================================================
// 5. Action Prediction Tests
//=============================================================================

TEST_F(TheoryOfMindTest, PredictActionFromGoal) {
    // Set up a goal first
    tom_observation_t obs = create_verbal_observation("I want to eat");
    ASSERT_TRUE(tom_observe(tom, &obs));

    char action[256];
    float likelihood = 0.0f;
    ASSERT_TRUE(tom_predict_action(tom, action, sizeof(action), &likelihood));
    EXPECT_TRUE(strstr(action, "Satisfy expressed desire") != nullptr);
    EXPECT_GT(likelihood, 0.0f);
}

TEST_F(TheoryOfMindTest, PredictActionNoContext) {
    // No observations yet
    char action[256];
    float likelihood = 0.0f;
    ASSERT_TRUE(tom_predict_action(tom, action, sizeof(action), &likelihood));
    EXPECT_STREQ(action, "Unknown action");
    EXPECT_LT(likelihood, 0.5f);
}

TEST_F(TheoryOfMindTest, PredictActionNull) {
    char action[256];
    float likelihood = 0.0f;
    EXPECT_FALSE(tom_predict_action(nullptr, action, sizeof(action), &likelihood));
}

//=============================================================================
// 6. Empathy Tests
//=============================================================================

TEST_F(TheoryOfMindTest, EmpathizeWithJoy) {
    tom_emotion_t response;
    ASSERT_TRUE(tom_empathize(tom, TOM_EMOTION_JOY, &response));
    EXPECT_EQ(response, TOM_EMOTION_JOY);  // Mirror joy
}

TEST_F(TheoryOfMindTest, EmpathizeWithSadness) {
    tom_emotion_t response;
    ASSERT_TRUE(tom_empathize(tom, TOM_EMOTION_SADNESS, &response));
    EXPECT_EQ(response, TOM_EMOTION_SADNESS);  // Compassion
}

TEST_F(TheoryOfMindTest, EmpathizeWithAnger) {
    tom_emotion_t response;
    ASSERT_TRUE(tom_empathize(tom, TOM_EMOTION_ANGER, &response));
    EXPECT_EQ(response, TOM_EMOTION_CALM);  // Calming response
}

TEST_F(TheoryOfMindTest, EmpathizeWithFear) {
    tom_emotion_t response;
    ASSERT_TRUE(tom_empathize(tom, TOM_EMOTION_FEAR, &response));
    EXPECT_EQ(response, TOM_EMOTION_CALM);  // Reassuring response
}

TEST_F(TheoryOfMindTest, EmpathizeWithAnxiety) {
    tom_emotion_t response;
    ASSERT_TRUE(tom_empathize(tom, TOM_EMOTION_ANXIETY, &response));
    EXPECT_EQ(response, TOM_EMOTION_CALM);  // Calming response
}

TEST_F(TheoryOfMindTest, EmpathizeNull) {
    tom_emotion_t response;
    EXPECT_FALSE(tom_empathize(nullptr, TOM_EMOTION_JOY, &response));
}

//=============================================================================
// 7. False Belief Tests
//=============================================================================

TEST_F(TheoryOfMindTest, DetectFalseBelief) {
    const char* reality = "Box contains pencils";
    const char* belief = "Box contains candy";
    bool is_false = false;

    ASSERT_TRUE(tom_detect_false_belief(tom, reality, belief, &is_false));
    EXPECT_TRUE(is_false);

    // Check statistics
    tom_statistics_t stats;
    ASSERT_TRUE(tom_get_statistics(tom, &stats));
    EXPECT_EQ(stats.false_beliefs_detected, 1);
}

TEST_F(TheoryOfMindTest, DetectTrueBelief) {
    const char* reality = "Box contains candy";
    const char* belief = "Box contains candy";
    bool is_false = true;  // Initialize to true to verify it changes

    ASSERT_TRUE(tom_detect_false_belief(tom, reality, belief, &is_false));
    EXPECT_FALSE(is_false);
}

TEST_F(TheoryOfMindTest, FalseBeliefUpdatesPerspective) {
    // Initial perspective score
    float initial_score = tom_get_perspective_score(tom);

    // Detect false belief
    const char* reality = "Reality state";
    const char* belief = "Belief state";
    bool is_false = false;
    ASSERT_TRUE(tom_detect_false_belief(tom, reality, belief, &is_false));
    EXPECT_TRUE(is_false);

    // Perspective score should improve
    float new_score = tom_get_perspective_score(tom);
    EXPECT_GE(new_score, initial_score);
}

TEST_F(TheoryOfMindTest, DetectFalseBeliefNull) {
    bool is_false = false;
    EXPECT_FALSE(tom_detect_false_belief(nullptr, "reality", "belief", &is_false));
}

//=============================================================================
// 8. BDI State Management Tests
//=============================================================================

TEST_F(TheoryOfMindTest, GetBDIState) {
    // Set up some observations to populate BDI
    tom_observation_t obs = create_verbal_observation("I want to learn");
    ASSERT_TRUE(tom_observe(tom, &obs));

    tom_belief_t belief;
    tom_desire_t desire;
    tom_intention_t intention;

    ASSERT_TRUE(tom_get_bdi_state(tom, &belief, &desire, &intention));
    EXPECT_GT(strlen(desire.goal_description), 0);
}

TEST_F(TheoryOfMindTest, GetBDIStatePartial) {
    // Only retrieve desire
    tom_desire_t desire;
    ASSERT_TRUE(tom_get_bdi_state(tom, nullptr, &desire, nullptr));
}

TEST_F(TheoryOfMindTest, GetBDIStateNull) {
    tom_belief_t belief;
    EXPECT_FALSE(tom_get_bdi_state(nullptr, &belief, nullptr, nullptr));
}

//=============================================================================
// 9. Perspective-Taking Tests
//=============================================================================

TEST_F(TheoryOfMindTest, PerspectiveScoreInitial) {
    float score = tom_get_perspective_score(tom);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(TheoryOfMindTest, PerspectiveScoreImproves) {
    float initial = tom_get_perspective_score(tom);

    // Detect multiple false beliefs
    for (int i = 0; i < 5; i++) {
        char reality[64], belief[64];
        snprintf(reality, sizeof(reality), "Reality %d", i);
        snprintf(belief, sizeof(belief), "Belief %d", i + 100);

        bool is_false = false;
        ASSERT_TRUE(tom_detect_false_belief(tom, reality, belief, &is_false));
        EXPECT_TRUE(is_false);
    }

    float final = tom_get_perspective_score(tom);
    EXPECT_GE(final, initial);
}

TEST_F(TheoryOfMindTest, PerspectiveScoreNull) {
    float score = tom_get_perspective_score(nullptr);
    EXPECT_EQ(score, 0.0f);
}

//=============================================================================
// 10. Statistics & Diagnostics Tests
//=============================================================================

TEST_F(TheoryOfMindTest, GetStatistics) {
    tom_statistics_t stats;
    ASSERT_TRUE(tom_get_statistics(tom, &stats));

    EXPECT_EQ(stats.total_observations, 0);
    EXPECT_EQ(stats.emotion_inferences, 0);
    EXPECT_EQ(stats.goal_inferences, 0);
    EXPECT_EQ(stats.action_predictions, 0);
    EXPECT_EQ(stats.empathy_responses, 0);
    EXPECT_EQ(stats.false_beliefs_detected, 0);
}

TEST_F(TheoryOfMindTest, StatisticsUpdate) {
    // Perform various operations
    tom_observation_t obs = create_verbal_observation("I am happy and want help");
    ASSERT_TRUE(tom_observe(tom, &obs));

    char action[256];
    float likelihood;
    ASSERT_TRUE(tom_predict_action(tom, action, sizeof(action), &likelihood));

    tom_emotion_t response;
    ASSERT_TRUE(tom_empathize(tom, TOM_EMOTION_JOY, &response));

    bool is_false;
    ASSERT_TRUE(tom_detect_false_belief(tom, "A", "B", &is_false));

    // Check statistics
    tom_statistics_t stats;
    ASSERT_TRUE(tom_get_statistics(tom, &stats));

    EXPECT_EQ(stats.total_observations, 1);
    EXPECT_GE(stats.emotion_inferences, 1);
    EXPECT_GE(stats.goal_inferences, 1);
    EXPECT_EQ(stats.action_predictions, 1);
    EXPECT_EQ(stats.empathy_responses, 1);
    EXPECT_EQ(stats.false_beliefs_detected, 1);
}

TEST_F(TheoryOfMindTest, GetStatisticsNull) {
    tom_statistics_t stats;
    EXPECT_FALSE(tom_get_statistics(nullptr, &stats));
}

TEST_F(TheoryOfMindTest, Reset) {
    // Add some observations
    tom_observation_t obs = create_verbal_observation("Test");
    ASSERT_TRUE(tom_observe(tom, &obs));

    // Reset
    ASSERT_TRUE(tom_reset(tom));

    // BDI state should be cleared
    tom_belief_t belief;
    tom_desire_t desire;
    tom_intention_t intention;
    ASSERT_TRUE(tom_get_bdi_state(tom, &belief, &desire, &intention));

    // Emotion should be neutral
    float confidence;
    tom_emotion_t emotion = tom_infer_emotion(tom, &confidence);
    EXPECT_EQ(emotion, TOM_EMOTION_NEUTRAL);
}

TEST_F(TheoryOfMindTest, ResetNull) {
    EXPECT_FALSE(tom_reset(nullptr));
}

//=============================================================================
// 11. String Conversion Tests
//=============================================================================

TEST_F(TheoryOfMindTest, EmotionToString) {
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
    EXPECT_STREQ(tom_emotion_to_string(TOM_EMOTION_NEUTRAL), "Neutral");
    EXPECT_STREQ(tom_emotion_to_string(TOM_EMOTION_UNKNOWN), "Unknown");
}

//=============================================================================
// 12. Integration Tests
//=============================================================================

TEST_F(TheoryOfMindTest, CompleteInteraction) {
    // Simulate a complete social interaction

    // 1. Observe initial statement
    tom_observation_t obs1 = create_verbal_observation("I want to learn about AI");
    ASSERT_TRUE(tom_observe(tom, &obs1));

    // 2. Infer goal
    char goal[256];
    float goal_conf;
    ASSERT_TRUE(tom_infer_goal(tom, goal, sizeof(goal), &goal_conf));
    EXPECT_STREQ(goal, "Satisfy expressed desire");

    // 3. Observe emotion
    tom_observation_t obs2 = create_verbal_observation("This is exciting!", TOM_EMOTION_JOY);
    ASSERT_TRUE(tom_observe(tom, &obs2));

    // 4. Infer emotion
    float emotion_conf;
    tom_emotion_t emotion = tom_infer_emotion(tom, &emotion_conf);
    EXPECT_EQ(emotion, TOM_EMOTION_JOY);

    // 5. Generate empathy
    tom_emotion_t empathy_response;
    ASSERT_TRUE(tom_empathize(tom, emotion, &empathy_response));
    EXPECT_EQ(empathy_response, TOM_EMOTION_JOY);

    // 6. Predict action
    char action[256];
    float likelihood;
    ASSERT_TRUE(tom_predict_action(tom, action, sizeof(action), &likelihood));
    EXPECT_TRUE(strlen(action) > 0);

    // 7. Check statistics
    tom_statistics_t stats;
    ASSERT_TRUE(tom_get_statistics(tom, &stats));
    EXPECT_EQ(stats.total_observations, 2);
    EXPECT_GE(stats.emotion_inferences, 1);
    EXPECT_GE(stats.goal_inferences, 1);
}

//=============================================================================
// 13. Self-Model Tests (Phase 10.11.2)
//=============================================================================

TEST_F(TheoryOfMindTest, UpdateSelfModel_Basic) {
    float features[3] = {0.5f, 0.7f, 0.3f};
    bool updated = tom_update_self_model(tom, features, 3, "move_left", 0.8f);
    ASSERT_TRUE(updated);

    // Verify self-model was updated
    tom_statistics_t stats;
    ASSERT_TRUE(tom_get_statistics(tom, &stats));
    EXPECT_GT(stats.total_observations, 0);
}

TEST_F(TheoryOfMindTest, UpdateSelfModel_NullToM) {
    float features[3] = {0.5f, 0.7f, 0.3f};
    EXPECT_FALSE(tom_update_self_model(nullptr, features, 3, "action", 0.8f));
}

TEST_F(TheoryOfMindTest, UpdateSelfModel_NullFeatures) {
    EXPECT_FALSE(tom_update_self_model(tom, nullptr, 3, "action", 0.8f));
}

TEST_F(TheoryOfMindTest, UpdateSelfModel_NullLabel) {
    float features[3] = {0.5f, 0.7f, 0.3f};
    EXPECT_FALSE(tom_update_self_model(tom, features, 3, nullptr, 0.8f));
}

TEST_F(TheoryOfMindTest, UpdateSelfModel_ZeroFeatures) {
    float features[3] = {0.5f, 0.7f, 0.3f};
    EXPECT_FALSE(tom_update_self_model(tom, features, 0, "action", 0.8f));
}

TEST_F(TheoryOfMindTest, UpdateSelfModel_InvalidConfidence) {
    float features[3] = {0.5f, 0.7f, 0.3f};
    // Confidence out of range [0, 1]
    EXPECT_FALSE(tom_update_self_model(tom, features, 3, "action", -0.1f));
    EXPECT_FALSE(tom_update_self_model(tom, features, 3, "action", 1.5f));
}

TEST_F(TheoryOfMindTest, UpdateSelfModel_MultipleUpdates) {
    float features1[2] = {0.3f, 0.6f};
    float features2[2] = {0.8f, 0.2f};

    ASSERT_TRUE(tom_update_self_model(tom, features1, 2, "action1", 0.7f));
    ASSERT_TRUE(tom_update_self_model(tom, features2, 2, "action2", 0.9f));

    tom_statistics_t stats;
    ASSERT_TRUE(tom_get_statistics(tom, &stats));
    EXPECT_EQ(stats.total_observations, 2);
}

// Main is provided by GTest::Main library
