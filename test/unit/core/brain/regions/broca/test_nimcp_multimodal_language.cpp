/**
 * @file test_nimcp_multimodal_language.cpp
 * @brief Unit tests for nimcp_multimodal_language.c
 *
 * @version Phase B4: Speech Enhancement
 * @date 2026-01-15
 */

#include <gtest/gtest.h>
#include <cstring>

#include "core/brain/regions/broca/nimcp_multimodal_language.h"

class MultimodalLanguageTest : public ::testing::Test {
protected:
    multimodal_language_t* processor;
    multimodal_config_t config;

    void SetUp() override {
        config = multimodal_lang_default_config();
        processor = multimodal_lang_create(&config);
        ASSERT_NE(nullptr, processor);
    }

    void TearDown() override {
        multimodal_lang_destroy(processor);
    }
};

// Lifecycle Tests
TEST_F(MultimodalLanguageTest, DefaultConfigReasonable) {
    auto cfg = multimodal_lang_default_config();
    EXPECT_GT(cfg.max_gestures, 0u);
    EXPECT_GT(cfg.max_expressions, 0u);
    EXPECT_TRUE(cfg.enable_auto_gestures);
    EXPECT_TRUE(cfg.enable_auto_expressions);
}

TEST_F(MultimodalLanguageTest, CreateWithNullConfig) {
    auto* p = multimodal_lang_create(NULL);
    ASSERT_NE(nullptr, p);
    multimodal_lang_destroy(p);
}

TEST_F(MultimodalLanguageTest, DestroyNull) {
    multimodal_lang_destroy(NULL);
}

TEST_F(MultimodalLanguageTest, Reset) {
    EXPECT_TRUE(multimodal_lang_reset(processor));
    EXPECT_EQ(multimodal_lang_get_status(processor), MULTIMODAL_STATUS_IDLE);
}

// Plan Generation Tests
TEST_F(MultimodalLanguageTest, GeneratePlan) {
    multimodal_plan_t plan;
    EXPECT_TRUE(multimodal_lang_generate_plan(processor, "Hello world", 1000.0f, &plan));

    EXPECT_FLOAT_EQ(plan.speech_duration_ms, 1000.0f);
    EXPECT_STREQ(plan.utterance, "Hello world");

    multimodal_lang_free_plan(&plan);
}

TEST_F(MultimodalLanguageTest, GeneratePlanAutoGestures) {
    multimodal_plan_t plan;
    // "this" should trigger deictic gesture
    EXPECT_TRUE(multimodal_lang_generate_plan(processor, "Look at this big thing", 1500.0f, &plan));

    EXPECT_GT(plan.gesture_count, 0u);

    multimodal_lang_free_plan(&plan);
}

TEST_F(MultimodalLanguageTest, GeneratePlanAutoExpressions) {
    multimodal_plan_t plan;
    EXPECT_TRUE(multimodal_lang_generate_plan(processor, "I am so happy today", 1000.0f, &plan));

    EXPECT_GT(plan.expression_count, 0u);

    multimodal_lang_free_plan(&plan);
}

TEST_F(MultimodalLanguageTest, GeneratePlanGaze) {
    multimodal_plan_t plan;
    EXPECT_TRUE(multimodal_lang_generate_plan(processor, "Hello", 500.0f, &plan));

    EXPECT_GT(plan.gaze_count, 0u);

    multimodal_lang_free_plan(&plan);
}

TEST_F(MultimodalLanguageTest, FreePlanNull) {
    multimodal_lang_free_plan(NULL);
}

// Manual Addition Tests
TEST_F(MultimodalLanguageTest, AddGesture) {
    multimodal_plan_t plan;
    multimodal_lang_generate_plan(processor, "Test", 1000.0f, &plan);

    gesture_spec_t gesture = {};
    gesture.type = GESTURE_TYPE_BEAT;
    gesture.start_time_ms = 100.0f;
    gesture.duration_ms = 200.0f;
    gesture.intensity = 0.5f;

    uint32_t initial_count = plan.gesture_count;
    EXPECT_TRUE(multimodal_lang_add_gesture(processor, &plan, &gesture));
    EXPECT_EQ(plan.gesture_count, initial_count + 1);

    multimodal_lang_free_plan(&plan);
}

TEST_F(MultimodalLanguageTest, AddExpression) {
    multimodal_plan_t plan;
    multimodal_lang_generate_plan(processor, "Test", 1000.0f, &plan);

    expression_spec_t expression = {};
    expression.type = EXPRESSION_SMILE;
    expression.start_time_ms = 0.0f;
    expression.duration_ms = 500.0f;
    expression.intensity = 0.7f;

    uint32_t initial_count = plan.expression_count;
    EXPECT_TRUE(multimodal_lang_add_expression(processor, &plan, &expression));
    EXPECT_EQ(plan.expression_count, initial_count + 1);

    multimodal_lang_free_plan(&plan);
}

TEST_F(MultimodalLanguageTest, AddGaze) {
    multimodal_plan_t plan;
    multimodal_lang_generate_plan(processor, "Test", 1000.0f, &plan);

    gaze_spec_t gaze = {};
    gaze.target = GAZE_TARGET_OBJECT;
    gaze.start_time_ms = 200.0f;
    gaze.duration_ms = 300.0f;

    uint32_t initial_count = plan.gaze_count;
    EXPECT_TRUE(multimodal_lang_add_gaze(processor, &plan, &gaze));
    EXPECT_EQ(plan.gaze_count, initial_count + 1);

    multimodal_lang_free_plan(&plan);
}

// Synchronization Tests
TEST_F(MultimodalLanguageTest, SynchronizePlan) {
    multimodal_plan_t plan;
    multimodal_lang_generate_plan(processor, "Big thing here", 800.0f, &plan);

    EXPECT_TRUE(multimodal_lang_synchronize_plan(processor, &plan));
    EXPECT_GT(plan.sync_score, 0.5f);

    multimodal_lang_free_plan(&plan);
}

TEST_F(MultimodalLanguageTest, GetEventsAtTime) {
    multimodal_plan_t plan;
    multimodal_lang_generate_plan(processor, "Look at this", 1000.0f, &plan);

    gesture_spec_t gesture;
    expression_spec_t expression;
    gaze_spec_t gaze;

    EXPECT_TRUE(multimodal_lang_get_events_at_time(&plan, 100.0f, &gesture, &expression, &gaze));

    multimodal_lang_free_plan(&plan);
}

// Auto-Generation Tests
TEST_F(MultimodalLanguageTest, AutoGesturesIconic) {
    gesture_spec_t gestures[10];
    uint32_t count = multimodal_lang_auto_gestures(processor,
        "It was a very big round ball", gestures, 10);

    // Should generate gestures for "big" and "round"
    EXPECT_GT(count, 0u);

    bool found_iconic = false;
    for (uint32_t i = 0; i < count; i++) {
        if (gestures[i].type == GESTURE_TYPE_ICONIC) {
            found_iconic = true;
            break;
        }
    }
    EXPECT_TRUE(found_iconic);
}

TEST_F(MultimodalLanguageTest, AutoGesturesDeictic) {
    gesture_spec_t gestures[10];
    uint32_t count = multimodal_lang_auto_gestures(processor,
        "Look at that over there", gestures, 10);

    bool found_deictic = false;
    for (uint32_t i = 0; i < count; i++) {
        if (gestures[i].type == GESTURE_TYPE_DEICTIC) {
            found_deictic = true;
            break;
        }
    }
    EXPECT_TRUE(found_deictic);
}

TEST_F(MultimodalLanguageTest, AutoGesturesBeat) {
    gesture_spec_t gestures[10];
    uint32_t count = multimodal_lang_auto_gestures(processor,
        "It was absolutely definitely amazing", gestures, 10);

    bool found_beat = false;
    for (uint32_t i = 0; i < count; i++) {
        if (gestures[i].type == GESTURE_TYPE_BEAT) {
            found_beat = true;
            break;
        }
    }
    EXPECT_TRUE(found_beat);
}

TEST_F(MultimodalLanguageTest, AutoExpressionsHappy) {
    expression_spec_t expressions[10];
    uint32_t count = multimodal_lang_auto_expressions(processor,
        "I am so happy and wonderful", 0.5f, expressions, 10);

    EXPECT_GT(count, 0u);
    EXPECT_EQ(expressions[0].type, EXPRESSION_SMILE);
}

TEST_F(MultimodalLanguageTest, AutoExpressionsSad) {
    expression_spec_t expressions[10];
    uint32_t count = multimodal_lang_auto_expressions(processor,
        "I am so sad and sorry", -0.5f, expressions, 10);

    EXPECT_GT(count, 0u);
    EXPECT_EQ(expressions[0].type, EXPRESSION_FROWN);
}

TEST_F(MultimodalLanguageTest, AutoExpressionsSurprised) {
    expression_spec_t expressions[10];
    uint32_t count = multimodal_lang_auto_expressions(processor,
        "What? Really? That's surprising!", 0.0f, expressions, 10);

    EXPECT_GT(count, 0u);
    EXPECT_EQ(expressions[0].type, EXPRESSION_RAISED_EYEBROWS);
}

// Name Functions
TEST_F(MultimodalLanguageTest, GestureNames) {
    EXPECT_STREQ(multimodal_lang_gesture_name(GESTURE_TYPE_NONE), "NONE");
    EXPECT_STREQ(multimodal_lang_gesture_name(GESTURE_TYPE_ICONIC), "ICONIC");
    EXPECT_STREQ(multimodal_lang_gesture_name(GESTURE_TYPE_DEICTIC), "DEICTIC");
    EXPECT_STREQ(multimodal_lang_gesture_name(GESTURE_TYPE_BEAT), "BEAT");
    EXPECT_STREQ(multimodal_lang_gesture_name(static_cast<gesture_type_t>(999)), "INVALID");
}

TEST_F(MultimodalLanguageTest, ExpressionNames) {
    EXPECT_STREQ(multimodal_lang_expression_name(EXPRESSION_NEUTRAL), "NEUTRAL");
    EXPECT_STREQ(multimodal_lang_expression_name(EXPRESSION_SMILE), "SMILE");
    EXPECT_STREQ(multimodal_lang_expression_name(EXPRESSION_FROWN), "FROWN");
    EXPECT_STREQ(multimodal_lang_expression_name(static_cast<expression_type_t>(999)), "INVALID");
}

TEST_F(MultimodalLanguageTest, GazeNames) {
    EXPECT_STREQ(multimodal_lang_gaze_name(GAZE_TARGET_FORWARD), "FORWARD");
    EXPECT_STREQ(multimodal_lang_gaze_name(GAZE_TARGET_ADDRESSEE), "ADDRESSEE");
    EXPECT_STREQ(multimodal_lang_gaze_name(GAZE_TARGET_AWAY), "AWAY");
    EXPECT_STREQ(multimodal_lang_gaze_name(static_cast<gaze_target_t>(999)), "INVALID");
}

// Statistics Tests
TEST_F(MultimodalLanguageTest, StatsTracking) {
    multimodal_plan_t plan;
    multimodal_lang_generate_plan(processor, "Look at this big thing", 1000.0f, &plan);

    multimodal_stats_t stats;
    EXPECT_TRUE(multimodal_lang_get_stats(processor, &stats));
    EXPECT_GT(stats.plans_generated, 0u);

    multimodal_lang_free_plan(&plan);
}

TEST_F(MultimodalLanguageTest, StatsReset) {
    multimodal_plan_t plan;
    multimodal_lang_generate_plan(processor, "Test", 500.0f, &plan);
    multimodal_lang_free_plan(&plan);

    multimodal_lang_reset_stats(processor);

    multimodal_stats_t stats;
    multimodal_lang_get_stats(processor, &stats);
    EXPECT_EQ(stats.plans_generated, 0u);
}

// Configuration
TEST_F(MultimodalLanguageTest, GetConfig) {
    multimodal_config_t retrieved;
    EXPECT_TRUE(multimodal_lang_get_config(processor, &retrieved));
    EXPECT_EQ(retrieved.max_gestures, config.max_gestures);
}

// Null Checks
TEST_F(MultimodalLanguageTest, NullChecks) {
    multimodal_plan_t plan;
    gesture_spec_t gesture = {};
    expression_spec_t expression = {};
    gaze_spec_t gaze = {};

    EXPECT_FALSE(multimodal_lang_generate_plan(NULL, "test", 100.0f, &plan));
    EXPECT_FALSE(multimodal_lang_generate_plan(processor, NULL, 100.0f, &plan));
    EXPECT_FALSE(multimodal_lang_generate_plan(processor, "test", 100.0f, NULL));
    EXPECT_FALSE(multimodal_lang_generate_plan(processor, "test", -1.0f, &plan));

    EXPECT_FALSE(multimodal_lang_add_gesture(NULL, &plan, &gesture));
    EXPECT_FALSE(multimodal_lang_add_gesture(processor, NULL, &gesture));
    EXPECT_FALSE(multimodal_lang_add_expression(processor, &plan, NULL));

    EXPECT_EQ(multimodal_lang_auto_gestures(NULL, "test", nullptr, 10), 0u);

    EXPECT_EQ(multimodal_lang_get_status(NULL), MULTIMODAL_STATUS_ERROR);
    EXPECT_EQ(multimodal_lang_get_last_error(NULL), MULTIMODAL_ERROR_INTERNAL);
}

// Disabled Auto-Generation
TEST_F(MultimodalLanguageTest, DisabledAutoGeneration) {
    multimodal_config_t no_auto = multimodal_lang_default_config();
    no_auto.enable_auto_gestures = false;
    no_auto.enable_auto_expressions = false;
    no_auto.enable_gaze_tracking = false;

    multimodal_language_t* no_auto_proc = multimodal_lang_create(&no_auto);
    ASSERT_NE(nullptr, no_auto_proc);

    multimodal_plan_t plan;
    EXPECT_TRUE(multimodal_lang_generate_plan(no_auto_proc, "Big thing here", 1000.0f, &plan));

    // Without auto-generation, counts should be 0
    EXPECT_EQ(plan.gesture_count, 0u);
    EXPECT_EQ(plan.expression_count, 0u);
    EXPECT_EQ(plan.gaze_count, 0u);

    multimodal_lang_free_plan(&plan);
    multimodal_lang_destroy(no_auto_proc);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
