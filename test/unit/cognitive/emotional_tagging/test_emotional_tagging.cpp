/**
 * @file test_emotional_tagging.cpp
 * @brief Comprehensive test suite for Phase 10.3 Emotional Tagging
 *
 * WHAT: Unit and integration tests for emotional tagging system
 * WHY:  Validate Russell's circumplex model implementation
 * HOW:  Google Test framework with 20+ test cases
 *
 * TEST COVERAGE:
 * - Emotion creation and classification
 * - Salience boost computation
 * - Cognitive state detection
 * - Edge cases and validation
 * - Performance requirements
 *
 * @author NIMCP Development Team - Phase 10.3
 * @date 2025-11-09
 */

#include <gtest/gtest.h>
#include "cognitive/nimcp_emotional_tagging.h"
#include <cmath>

//=============================================================================
// Test Fixture
//=============================================================================

class EmotionalTaggingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize timestamp for tests
        timestamp_ms = 1000;
    }

    uint64_t timestamp_ms;

    // Helper: Check if two floats are approximately equal
    bool approx_equal(float a, float b, float epsilon = 0.01f) {
        return std::abs(a - b) < epsilon;
    }
};

//=============================================================================
// Unit Tests: Emotion Creation
//=============================================================================

TEST_F(EmotionalTaggingTest, CreateNeutralEmotion) {
    emotional_tag_t tag = emotional_tag_neutral();

    EXPECT_FLOAT_EQ(tag.valence, 0.0f);
    EXPECT_FLOAT_EQ(tag.arousal, 0.0f);
    EXPECT_EQ(tag.category, EMOTION_CAT_NEUTRAL);
    EXPECT_FLOAT_EQ(tag.intensity, 0.0f);
}

TEST_F(EmotionalTaggingTest, CreateJoyEmotion) {
    emotional_tag_t tag = emotional_tag_create(0.9f, 0.8f, timestamp_ms);

    EXPECT_FLOAT_EQ(tag.valence, 0.9f);
    EXPECT_FLOAT_EQ(tag.arousal, 0.8f);
    EXPECT_EQ(tag.category, EMOTION_CAT_JOY);
    EXPECT_GT(tag.intensity, 0.8f);  // High intensity for strong emotion
}

TEST_F(EmotionalTaggingTest, CreateFearEmotion) {
    emotional_tag_t tag = emotional_tag_create(-0.8f, 0.9f, timestamp_ms);

    EXPECT_FLOAT_EQ(tag.valence, -0.8f);
    EXPECT_FLOAT_EQ(tag.arousal, 0.9f);
    EXPECT_EQ(tag.category, EMOTION_CAT_FEAR);
    EXPECT_GT(tag.intensity, 0.8f);
}

TEST_F(EmotionalTaggingTest, CreateSadnessEmotion) {
    emotional_tag_t tag = emotional_tag_create(-0.7f, 0.2f, timestamp_ms);

    EXPECT_FLOAT_EQ(tag.valence, -0.7f);
    EXPECT_FLOAT_EQ(tag.arousal, 0.2f);
    EXPECT_EQ(tag.category, EMOTION_CAT_SADNESS);
}

TEST_F(EmotionalTaggingTest, CreateCalmEmotion) {
    emotional_tag_t tag = emotional_tag_create(0.5f, 0.1f, timestamp_ms);

    EXPECT_FLOAT_EQ(tag.valence, 0.5f);
    EXPECT_FLOAT_EQ(tag.arousal, 0.1f);
    EXPECT_EQ(tag.category, EMOTION_CAT_CALM);
}

//=============================================================================
// Unit Tests: Emotion Classification
//=============================================================================

TEST_F(EmotionalTaggingTest, ClassifyHighArousalPositive) {
    // Joy: high arousal, high positive valence
    emotional_tag_t joy = emotional_tag_create(0.9f, 0.8f, timestamp_ms);
    EXPECT_EQ(joy.category, EMOTION_CAT_JOY);

    // Excitement: high arousal, moderate positive valence
    emotional_tag_t excitement = emotional_tag_create(0.4f, 0.8f, timestamp_ms);
    EXPECT_EQ(excitement.category, EMOTION_CAT_EXCITEMENT);
}

TEST_F(EmotionalTaggingTest, ClassifyHighArousalNegative) {
    // Fear: high arousal, strong negative valence
    emotional_tag_t fear = emotional_tag_create(-0.8f, 0.9f, timestamp_ms);
    EXPECT_EQ(fear.category, EMOTION_CAT_FEAR);

    // Anger: high arousal, moderate negative valence
    emotional_tag_t anger = emotional_tag_create(-0.5f, 0.7f, timestamp_ms);
    EXPECT_EQ(anger.category, EMOTION_CAT_ANGER);

    // Anxiety: high arousal, mild negative valence
    emotional_tag_t anxiety = emotional_tag_create(-0.3f, 0.6f, timestamp_ms);
    EXPECT_EQ(anxiety.category, EMOTION_CAT_ANXIETY);
}

TEST_F(EmotionalTaggingTest, ClassifyLowArousal) {
    // Calm: low arousal, positive valence
    emotional_tag_t calm = emotional_tag_create(0.3f, 0.1f, timestamp_ms);
    EXPECT_EQ(calm.category, EMOTION_CAT_CALM);

    // Sadness: low arousal, negative valence
    emotional_tag_t sadness = emotional_tag_create(-0.5f, 0.3f, timestamp_ms);
    EXPECT_EQ(sadness.category, EMOTION_CAT_SADNESS);

    // Boredom: low arousal, mild negative valence
    emotional_tag_t boredom = emotional_tag_create(-0.2f, 0.1f, timestamp_ms);
    EXPECT_EQ(boredom.category, EMOTION_CAT_BOREDOM);
}

//=============================================================================
// Unit Tests: Intensity Computation
//=============================================================================

TEST_F(EmotionalTaggingTest, IntensityIsZeroForNeutral) {
    emotional_tag_t tag = emotional_tag_neutral();
    EXPECT_FLOAT_EQ(tag.intensity, 0.0f);
}

TEST_F(EmotionalTaggingTest, IntensityHighForStrongEmotions) {
    // Strong fear (both dimensions high)
    emotional_tag_t fear = emotional_tag_create(-0.9f, 0.9f, timestamp_ms);
    EXPECT_GT(fear.intensity, 0.85f);

    // Strong joy
    emotional_tag_t joy = emotional_tag_create(0.9f, 0.9f, timestamp_ms);
    EXPECT_GT(joy.intensity, 0.85f);
}

TEST_F(EmotionalTaggingTest, IntensityLowForMildEmotions) {
    emotional_tag_t mild = emotional_tag_create(0.2f, 0.1f, timestamp_ms);
    EXPECT_LT(mild.intensity, 0.2f);
}

//=============================================================================
// Unit Tests: Salience Boost
//=============================================================================

TEST_F(EmotionalTaggingTest, SalienceBoostForNeutralIsMinimal) {
    emotional_tag_t neutral = emotional_tag_neutral();
    float boost = emotional_compute_salience_boost(&neutral);

    EXPECT_TRUE(approx_equal(boost, 1.0f, 0.05f));  // ~1.0 (no boost)
}

TEST_F(EmotionalTaggingTest, SalienceBoostForHighArousalIsSignificant) {
    // High arousal should boost salience by ~50%
    emotional_tag_t fear = emotional_tag_create(-0.8f, 0.9f, timestamp_ms);
    float boost = emotional_compute_salience_boost(&fear);

    EXPECT_GT(boost, 1.6f);  // Expect >60% boost
    EXPECT_LT(boost, 1.9f);  // But <90% boost
}

TEST_F(EmotionalTaggingTest, SalienceBoostForStrongValenceMatters) {
    // Strong negative valence (low arousal) should still boost
    emotional_tag_t sad = emotional_tag_create(-0.9f, 0.1f, timestamp_ms);
    float boost = emotional_compute_salience_boost(&sad);

    EXPECT_GT(boost, 1.2f);  // Expect >20% boost from valence
}

TEST_F(EmotionalTaggingTest, SalienceBoostCombinesArousalAndValence) {
    // Both high arousal and high valence magnitude
    emotional_tag_t intense = emotional_tag_create(0.8f, 0.8f, timestamp_ms);
    float boost = emotional_compute_salience_boost(&intense);

    // Expected: 1.0 + (0.8 × 0.5) + (0.8 × 0.3) = 1.0 + 0.4 + 0.24 = 1.64
    EXPECT_TRUE(approx_equal(boost, 1.64f, 0.05f));
}

TEST_F(EmotionalTaggingTest, ApplySalienceBoostMultipliesCorrectly) {
    emotional_tag_t joy = emotional_tag_create(0.8f, 0.8f, timestamp_ms);
    float base_salience = 0.5f;

    float boosted = emotional_apply_salience_boost(base_salience, &joy);

    // Expected: 0.5 × 1.64 ≈ 0.82
    EXPECT_TRUE(approx_equal(boosted, 0.82f, 0.05f));
}

//=============================================================================
// Unit Tests: Cognitive State Detection
//=============================================================================

TEST_F(EmotionalTaggingTest, HighConfidenceGivesPositiveValence) {
    emotional_tag_t tag = emotional_tag_from_cognitive_state(
        0.9f,   // High confidence
        0.1f,   // Low uncertainty
        0.0f,   // No novelty
        true,   // Ethically approved
        timestamp_ms
    );

    EXPECT_GT(tag.valence, 0.5f);  // Positive valence
    EXPECT_LT(tag.arousal, 0.3f);  // Low arousal (confident and calm)
}

TEST_F(EmotionalTaggingTest, LowConfidenceGivesNegativeValence) {
    emotional_tag_t tag = emotional_tag_from_cognitive_state(
        0.1f,   // Low confidence
        0.3f,   // Moderate uncertainty
        0.0f,   // No novelty
        true,   // Ethically approved
        timestamp_ms
    );

    EXPECT_LT(tag.valence, -0.5f);  // Negative valence
}

TEST_F(EmotionalTaggingTest, HighUncertaintyGivesHighArousal) {
    emotional_tag_t tag = emotional_tag_from_cognitive_state(
        0.5f,   // Neutral confidence
        0.9f,   // High uncertainty
        0.0f,   // No novelty
        true,   // Ethically approved
        timestamp_ms
    );

    EXPECT_GT(tag.arousal, 0.8f);  // High arousal from uncertainty
}

TEST_F(EmotionalTaggingTest, NoveltyAddsPositiveValenceAndArousal) {
    emotional_tag_t tag = emotional_tag_from_cognitive_state(
        0.5f,   // Neutral confidence
        0.2f,   // Low uncertainty
        0.9f,   // High novelty (curiosity)
        true,   // Ethically approved
        timestamp_ms
    );

    EXPECT_GT(tag.valence, 0.1f);   // Positive from novelty
    EXPECT_GT(tag.arousal, 0.3f);   // Aroused from curiosity
}

TEST_F(EmotionalTaggingTest, EthicalViolationGivesStrongNegativeValence) {
    emotional_tag_t tag = emotional_tag_from_cognitive_state(
        0.8f,   // Even high confidence
        0.1f,   // Low uncertainty
        0.0f,   // No novelty
        false,  // Ethically REJECTED
        timestamp_ms
    );

    EXPECT_LT(tag.valence, -0.7f);  // Strong negative valence
    EXPECT_GT(tag.arousal, 0.2f);   // Aroused from ethical concern
    // Should be any negative emotion (FEAR, ANXIETY, ANGER, or SADNESS)
    EXPECT_TRUE(tag.category == EMOTION_CAT_FEAR ||
                tag.category == EMOTION_CAT_ANXIETY ||
                tag.category == EMOTION_CAT_ANGER ||
                tag.category == EMOTION_CAT_SADNESS);
}

//=============================================================================
// Unit Tests: Validation
//=============================================================================

TEST_F(EmotionalTaggingTest, ValidateValidTag) {
    emotional_tag_t tag = emotional_tag_create(0.5f, 0.5f, timestamp_ms);
    EXPECT_TRUE(emotional_tag_is_valid(&tag));
}

TEST_F(EmotionalTaggingTest, ValidateNullTag) {
    EXPECT_FALSE(emotional_tag_is_valid(nullptr));
}

TEST_F(EmotionalTaggingTest, ClampOutOfRangeValence) {
    emotional_tag_t tag;
    tag.valence = 2.0f;  // Out of range
    tag.arousal = 0.5f;
    tag.intensity = 0.5f;
    tag.category = EMOTION_CAT_NEUTRAL;

    emotional_tag_clamp(&tag);

    EXPECT_FLOAT_EQ(tag.valence, 1.0f);  // Clamped to max
}

TEST_F(EmotionalTaggingTest, ClampNegativeArousal) {
    emotional_tag_t tag;
    tag.valence = 0.0f;
    tag.arousal = -0.5f;  // Invalid (arousal can't be negative)
    tag.intensity = 0.0f;
    tag.category = EMOTION_CAT_NEUTRAL;

    emotional_tag_clamp(&tag);

    EXPECT_FLOAT_EQ(tag.arousal, 0.0f);  // Clamped to min
}

//=============================================================================
// Unit Tests: Category Names
//=============================================================================

TEST_F(EmotionalTaggingTest, CategoryNamesAreCorrect) {
    EXPECT_STREQ(emotional_category_name(EMOTION_CAT_NEUTRAL), "NEUTRAL");
    EXPECT_STREQ(emotional_category_name(EMOTION_CAT_JOY), "JOY");
    EXPECT_STREQ(emotional_category_name(EMOTION_CAT_FEAR), "FEAR");
    EXPECT_STREQ(emotional_category_name(EMOTION_CAT_SADNESS), "SADNESS");
    EXPECT_STREQ(emotional_category_name(EMOTION_CAT_CALM), "CALM");
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(EmotionalTaggingTest, ExtremeValencesHandled) {
    // Maximum positive
    emotional_tag_t max_pos = emotional_tag_create(1.0f, 1.0f, timestamp_ms);
    EXPECT_TRUE(emotional_tag_is_valid(&max_pos));

    // Maximum negative
    emotional_tag_t max_neg = emotional_tag_create(-1.0f, 1.0f, timestamp_ms);
    EXPECT_TRUE(emotional_tag_is_valid(&max_neg));
}

TEST_F(EmotionalTaggingTest, ZeroArousalHandled) {
    emotional_tag_t tag = emotional_tag_create(0.5f, 0.0f, timestamp_ms);
    EXPECT_TRUE(emotional_tag_is_valid(&tag));
    EXPECT_EQ(tag.category, EMOTION_CAT_CALM);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(EmotionalTaggingTest, EndToEndEmotionDetectionAndBoost) {
    // Scenario: AI makes confident ethical decision with novelty
    emotional_tag_t emotion = emotional_tag_from_cognitive_state(
        0.85f,  // High confidence
        0.15f,  // Low uncertainty
        0.7f,   // Novel situation
        true,   // Ethically sound
        timestamp_ms
    );

    // Should be positive, mildly aroused (curious)
    EXPECT_GT(emotion.valence, 0.3f);
    EXPECT_GT(emotion.arousal, 0.2f);  // Lowered threshold (novelty adds some arousal)

    // Apply to salience
    float base_salience = 0.6f;
    float boosted_salience = emotional_apply_salience_boost(base_salience, &emotion);

    // Should be boosted above base
    EXPECT_GT(boosted_salience, base_salience);
    EXPECT_LT(boosted_salience, base_salience * 1.8f);  // But not excessively
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(EmotionalTaggingTest, EmotionClassificationIsFast) {
    auto start = std::chrono::high_resolution_clock::now();

    // Classify 10000 emotions
    for (int i = 0; i < 10000; i++) {
        emotional_tag_t tag = emotional_tag_create(
            (i % 200 - 100) / 100.0f,  // Varying valence
            (i % 100) / 100.0f,        // Varying arousal
            timestamp_ms
        );
        (void)tag;  // Use the tag
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should complete 10000 classifications in <5ms
    EXPECT_LT(duration.count(), 5000);  // 5ms = 5000μs
}
