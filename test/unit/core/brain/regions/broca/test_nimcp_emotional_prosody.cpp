/**
 * @file test_nimcp_emotional_prosody.cpp
 * @brief Unit tests for nimcp_emotional_prosody.c
 *
 * @version Phase B4: Speech Enhancement
 * @date 2026-01-15
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "core/brain/regions/broca/nimcp_emotional_prosody.h"

class EmotionalProsodyTest : public ::testing::Test {
protected:
    emotional_prosody_t* processor;
    emotional_prosody_config_t config;

    void SetUp() override {
        config = emotional_prosody_default_config();
        processor = emotional_prosody_create(&config);
        ASSERT_NE(nullptr, processor);
    }

    void TearDown() override {
        emotional_prosody_destroy(processor);
    }
};

// Lifecycle Tests
TEST_F(EmotionalProsodyTest, DefaultConfigReasonable) {
    auto cfg = emotional_prosody_default_config();
    EXPECT_GT(cfg.max_contour_points, 0u);
    EXPECT_GT(cfg.base_pitch_hz, 0.0f);
    EXPECT_GT(cfg.base_rate_wpm, 0.0f);
}

TEST_F(EmotionalProsodyTest, CreateWithNullConfig) {
    auto* p = emotional_prosody_create(NULL);
    ASSERT_NE(nullptr, p);
    emotional_prosody_destroy(p);
}

TEST_F(EmotionalProsodyTest, DestroyNull) {
    emotional_prosody_destroy(NULL);
}

TEST_F(EmotionalProsodyTest, Reset) {
    EXPECT_TRUE(emotional_prosody_reset(processor));
    EXPECT_EQ(emotional_prosody_get_status(processor), PROSODY_STATUS_IDLE);
}

// Emotion Mapping Tests
TEST_F(EmotionalProsodyTest, GetParamsNeutral) {
    emotional_state_t state = {EMOTION_NEUTRAL, 1.0f, EMOTION_NEUTRAL, 0.0f, 0.5f, 0.0f};
    prosodic_params_t params;

    EXPECT_TRUE(emotional_prosody_get_params(processor, &state, &params));
    EXPECT_GT(params.pitch_mean_hz, 0.0f);
    EXPECT_EQ(params.voice_quality, VOICE_QUALITY_NORMAL);
}

TEST_F(EmotionalProsodyTest, GetParamsHappy) {
    emotional_state_t state = {EMOTION_HAPPY, 1.0f, EMOTION_NEUTRAL, 0.0f, 0.8f, 0.7f};
    prosodic_params_t params;

    EXPECT_TRUE(emotional_prosody_get_params(processor, &state, &params));
    EXPECT_GT(params.pitch_mean_hz, config.base_pitch_hz);
    EXPECT_GT(params.rate_factor, 1.0f);
}

TEST_F(EmotionalProsodyTest, GetParamsSad) {
    emotional_state_t state = {EMOTION_SAD, 1.0f, EMOTION_NEUTRAL, 0.0f, 0.2f, -0.7f};
    prosodic_params_t params;

    EXPECT_TRUE(emotional_prosody_get_params(processor, &state, &params));
    EXPECT_LT(params.pitch_mean_hz, config.base_pitch_hz);
    EXPECT_LT(params.rate_factor, 1.0f);
    EXPECT_EQ(params.voice_quality, VOICE_QUALITY_BREATHY);
}

TEST_F(EmotionalProsodyTest, GetParamsAngry) {
    emotional_state_t state = {EMOTION_ANGRY, 1.0f, EMOTION_NEUTRAL, 0.0f, 0.9f, -0.5f};
    prosodic_params_t params;

    EXPECT_TRUE(emotional_prosody_get_params(processor, &state, &params));
    EXPECT_GT(params.pitch_mean_hz, config.base_pitch_hz);
    EXPECT_GT(params.intensity_factor, 1.0f);
    EXPECT_EQ(params.voice_quality, VOICE_QUALITY_TENSE);
}

TEST_F(EmotionalProsodyTest, SetGetEmotion) {
    emotional_state_t state = {EMOTION_HAPPY, 0.8f, EMOTION_NEUTRAL, 0.0f, 0.7f, 0.6f};
    EXPECT_TRUE(emotional_prosody_set_emotion(processor, &state));

    emotional_state_t retrieved;
    EXPECT_TRUE(emotional_prosody_get_emotion(processor, &retrieved));
    EXPECT_EQ(retrieved.primary_emotion, EMOTION_HAPPY);
}

TEST_F(EmotionalProsodyTest, BlendEmotions) {
    emotional_state_t happy = {EMOTION_HAPPY, 1.0f, EMOTION_NEUTRAL, 0.0f, 0.8f, 0.7f};
    emotional_state_t sad = {EMOTION_SAD, 1.0f, EMOTION_NEUTRAL, 0.0f, 0.2f, -0.7f};
    emotional_state_t result;

    EXPECT_TRUE(emotional_prosody_blend_emotions(processor, &happy, &sad, 0.3f, &result));
    EXPECT_EQ(result.primary_emotion, EMOTION_HAPPY);
    EXPECT_EQ(result.secondary_emotion, EMOTION_SAD);
}

// Contour Generation Tests
TEST_F(EmotionalProsodyTest, GenerateContour) {
    emotional_state_t state = {EMOTION_NEUTRAL, 1.0f, EMOTION_NEUTRAL, 0.0f, 0.5f, 0.0f};
    emotional_prosody_set_emotion(processor, &state);

    prosodic_contour_t contour;
    EXPECT_TRUE(emotional_prosody_generate_contour(processor, "Hello world", 500.0f, &contour));

    EXPECT_GT(contour.point_count, 0u);
    EXPECT_FLOAT_EQ(contour.duration_ms, 500.0f);
    EXPECT_NE(contour.points, nullptr);

    for (uint32_t i = 0; i < contour.point_count; i++) {
        EXPECT_GT(contour.points[i].pitch_hz, 0.0f);
    }

    emotional_prosody_free_contour(&contour);
}

TEST_F(EmotionalProsodyTest, ApplyContour) {
    emotional_state_t state = {EMOTION_NEUTRAL, 1.0f, EMOTION_NEUTRAL, 0.0f, 0.5f, 0.0f};
    emotional_prosody_set_emotion(processor, &state);

    prosodic_contour_t contour;
    emotional_prosody_generate_contour(processor, "Test", 300.0f, &contour);

    uint8_t phonemes[5] = {1, 2, 3, 4, 5};
    float pitches[5], durations[5];

    EXPECT_TRUE(emotional_prosody_apply_contour(processor, &contour, phonemes, 5, pitches, durations));

    for (int i = 0; i < 5; i++) {
        EXPECT_GT(pitches[i], 0.0f);
        EXPECT_GT(durations[i], 0.0f);
    }

    emotional_prosody_free_contour(&contour);
}

TEST_F(EmotionalProsodyTest, FreeContourNull) {
    emotional_prosody_free_contour(NULL);
}

// Name Functions
TEST_F(EmotionalProsodyTest, EmotionNames) {
    EXPECT_STREQ(emotional_prosody_emotion_name(EMOTION_NEUTRAL), "NEUTRAL");
    EXPECT_STREQ(emotional_prosody_emotion_name(EMOTION_HAPPY), "HAPPY");
    EXPECT_STREQ(emotional_prosody_emotion_name(EMOTION_SAD), "SAD");
    EXPECT_STREQ(emotional_prosody_emotion_name(static_cast<emotion_type_t>(999)), "INVALID");
}

TEST_F(EmotionalProsodyTest, QualityNames) {
    EXPECT_STREQ(emotional_prosody_quality_name(VOICE_QUALITY_NORMAL), "NORMAL");
    EXPECT_STREQ(emotional_prosody_quality_name(VOICE_QUALITY_BREATHY), "BREATHY");
    EXPECT_STREQ(emotional_prosody_quality_name(static_cast<voice_quality_t>(999)), "INVALID");
}

// Statistics
TEST_F(EmotionalProsodyTest, StatsTracking) {
    emotional_state_t state = {EMOTION_NEUTRAL, 1.0f, EMOTION_NEUTRAL, 0.0f, 0.5f, 0.0f};
    prosodic_params_t params;
    emotional_prosody_get_params(processor, &state, &params);

    prosody_stats_t stats;
    EXPECT_TRUE(emotional_prosody_get_stats(processor, &stats));
    EXPECT_GT(stats.parameters_computed, 0u);
}

TEST_F(EmotionalProsodyTest, StatsReset) {
    emotional_state_t state = {EMOTION_NEUTRAL, 1.0f, EMOTION_NEUTRAL, 0.0f, 0.5f, 0.0f};
    prosodic_params_t params;
    emotional_prosody_get_params(processor, &state, &params);

    emotional_prosody_reset_stats(processor);

    prosody_stats_t stats;
    emotional_prosody_get_stats(processor, &stats);
    EXPECT_EQ(stats.parameters_computed, 0u);
}

// Null Checks
TEST_F(EmotionalProsodyTest, NullChecks) {
    prosodic_params_t params;
    emotional_state_t state = {EMOTION_NEUTRAL, 1.0f, EMOTION_NEUTRAL, 0.0f, 0.5f, 0.0f};

    EXPECT_FALSE(emotional_prosody_get_params(NULL, &state, &params));
    EXPECT_FALSE(emotional_prosody_get_params(processor, NULL, &params));
    EXPECT_FALSE(emotional_prosody_get_params(processor, &state, NULL));

    EXPECT_EQ(emotional_prosody_get_status(NULL), PROSODY_STATUS_ERROR);
    EXPECT_EQ(emotional_prosody_get_last_error(NULL), PROSODY_ERROR_INTERNAL);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
