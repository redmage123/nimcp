/**
 * @file test_emotional_tagging_real.cpp
 * @brief Real tests for emotional tagging system
 *
 * Tests only functions that ACTUALLY EXIST in nimcp_emotional_tagging.h
 */

#include <gtest/gtest.h>
#include <cmath>

#include "cognitive/nimcp_emotional_tagging.h"
#include "core/brain/nimcp_brain.h"

class EmotionalTaggingRealTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        brain = brain_create("test", BRAIN_SIZE_TINY,
                            BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) brain_destroy(brain);
    }
};

//=============================================================================
// Lifecycle Functions
//=============================================================================

TEST_F(EmotionalTaggingRealTest, CreateTag_ValidValuesAreStored) {
    emotional_tag_t tag = emotional_tag_create(0.5f, 0.7f, 1000);

    EXPECT_NEAR(tag.valence, 0.5f, 0.01f);
    EXPECT_NEAR(tag.arousal, 0.7f, 0.01f);
    EXPECT_EQ(tag.timestamp_ms, 1000);
}

TEST_F(EmotionalTaggingRealTest, CreateTag_ClampsValenceToValidRange) {
    // Test clamping to [-1, +1]
    emotional_tag_t tag1 = emotional_tag_create(2.0f, 0.5f, 0);
    EXPECT_LE(tag1.valence, 1.0f);

    emotional_tag_t tag2 = emotional_tag_create(-2.0f, 0.5f, 0);
    EXPECT_GE(tag2.valence, -1.0f);
}

TEST_F(EmotionalTaggingRealTest, CreateTag_ClampsArousalToValidRange) {
    // Test clamping to [0, 1]
    emotional_tag_t tag1 = emotional_tag_create(0.0f, 2.0f, 0);
    EXPECT_LE(tag1.arousal, 1.0f);

    emotional_tag_t tag2 = emotional_tag_create(0.0f, -1.0f, 0);
    EXPECT_GE(tag2.arousal, 0.0f);
}

TEST_F(EmotionalTaggingRealTest, CreateNeutral_ReturnsZeroValues) {
    emotional_tag_t tag = emotional_tag_neutral();

    EXPECT_EQ(tag.valence, 0.0f);
    EXPECT_EQ(tag.arousal, 0.0f);
    EXPECT_EQ(tag.category, EMOTION_CAT_NEUTRAL);
    EXPECT_EQ(tag.intensity, 0.0f);
}

//=============================================================================
// Classification & Analysis
//=============================================================================

TEST_F(EmotionalTaggingRealTest, Classify_HighArousalPositiveValence_ReturnsJoyOrExcitement) {
    emotional_tag_t tag = emotional_tag_create(0.8f, 0.8f, 0);
    emotion_category_t category = emotional_tag_classify(&tag);

    EXPECT_TRUE(category == EMOTION_CAT_JOY || category == EMOTION_CAT_EXCITEMENT);
}

TEST_F(EmotionalTaggingRealTest, Classify_LowArousalPositiveValence_ReturnsCalm) {
    emotional_tag_t tag = emotional_tag_create(0.5f, 0.1f, 0);
    emotion_category_t category = emotional_tag_classify(&tag);

    EXPECT_EQ(category, EMOTION_CAT_CALM);
}

TEST_F(EmotionalTaggingRealTest, Classify_HighArousalNegativeValence_ReturnsFearOrAnger) {
    emotional_tag_t tag = emotional_tag_create(-0.7f, 0.8f, 0);
    emotion_category_t category = emotional_tag_classify(&tag);

    EXPECT_TRUE(category == EMOTION_CAT_FEAR || category == EMOTION_CAT_ANGER ||
                category == EMOTION_CAT_ANXIETY);
}

TEST_F(EmotionalTaggingRealTest, Classify_LowArousalNegativeValence_ReturnsSadnessOrBoredom) {
    emotional_tag_t tag = emotional_tag_create(-0.5f, 0.1f, 0);
    emotion_category_t category = emotional_tag_classify(&tag);

    EXPECT_TRUE(category == EMOTION_CAT_SADNESS || category == EMOTION_CAT_BOREDOM);
}

TEST_F(EmotionalTaggingRealTest, Classify_NeutralValues_ReturnsNeutral) {
    emotional_tag_t tag = emotional_tag_neutral();
    emotion_category_t category = emotional_tag_classify(&tag);

    EXPECT_EQ(category, EMOTION_CAT_NEUTRAL);
}

TEST_F(EmotionalTaggingRealTest, Intensity_NeutralTag_ReturnsZero) {
    emotional_tag_t tag = emotional_tag_neutral();
    float intensity = emotional_tag_intensity(&tag);

    EXPECT_EQ(intensity, 0.0f);
}

TEST_F(EmotionalTaggingRealTest, Intensity_HighValenceHighArousal_ReturnsHighIntensity) {
    emotional_tag_t tag = emotional_tag_create(0.8f, 0.8f, 0);
    float intensity = emotional_tag_intensity(&tag);

    EXPECT_GT(intensity, 0.5f);
    EXPECT_LE(intensity, 1.0f);
}

TEST_F(EmotionalTaggingRealTest, CategoryName_ReturnsNonEmptyString) {
    const char* name = emotional_category_name(EMOTION_CAT_JOY);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0);
}

TEST_F(EmotionalTaggingRealTest, CategoryName_AllCategories_ReturnValidStrings) {
    emotion_category_t categories[] = {
        EMOTION_CAT_NEUTRAL, EMOTION_CAT_JOY, EMOTION_CAT_EXCITEMENT, EMOTION_CAT_CALM,
        EMOTION_CAT_FEAR, EMOTION_CAT_ANGER, EMOTION_CAT_SADNESS, EMOTION_CAT_ANXIETY,
        EMOTION_CAT_BOREDOM
    };

    for (auto cat : categories) {
        const char* name = emotional_category_name(cat);
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0);
    }
}

//=============================================================================
// Salience Modulation
//=============================================================================

TEST_F(EmotionalTaggingRealTest, SalienceBoost_NeutralTag_ReturnsOne) {
    emotional_tag_t tag = emotional_tag_neutral();
    float boost = emotional_compute_salience_boost(&tag);

    EXPECT_NEAR(boost, 1.0f, 0.01f);
}

TEST_F(EmotionalTaggingRealTest, SalienceBoost_HighArousal_BoostsAboveOne) {
    emotional_tag_t tag = emotional_tag_create(0.0f, 1.0f, 0);
    float boost = emotional_compute_salience_boost(&tag);

    EXPECT_GT(boost, 1.0f);
}

TEST_F(EmotionalTaggingRealTest, SalienceBoost_HighValence_BoostsAboveOne) {
    emotional_tag_t tag = emotional_tag_create(1.0f, 0.0f, 0);
    float boost = emotional_compute_salience_boost(&tag);

    EXPECT_GT(boost, 1.0f);
}

TEST_F(EmotionalTaggingRealTest, SalienceBoost_FearEmotion_HighBoost) {
    // Fear: high arousal + negative valence = high boost
    emotional_tag_t tag = emotional_tag_create(-0.8f, 0.9f, 0);
    float boost = emotional_compute_salience_boost(&tag);

    EXPECT_GT(boost, 1.5f); // Should be significant boost
}

TEST_F(EmotionalTaggingRealTest, ApplySalienceBoost_MultipliesCorrectly) {
    emotional_tag_t tag = emotional_tag_create(0.5f, 0.5f, 0);
    float base_salience = 0.5f;

    float enhanced = emotional_apply_salience_boost(base_salience, &tag);
    float expected_boost = emotional_compute_salience_boost(&tag);

    EXPECT_NEAR(enhanced, base_salience * expected_boost, 0.01f);
}

//=============================================================================
// Emotion Detection from Cognitive State
//=============================================================================

TEST_F(EmotionalTaggingRealTest, FromCognitiveState_HighConfidence_PositiveValence) {
    emotional_tag_t tag = emotional_tag_from_cognitive_state(
        0.9f,   // high confidence
        0.1f,   // low uncertainty
        0.0f,   // no novelty
        true,   // ethical
        1000
    );

    EXPECT_GT(tag.valence, 0.0f); // Positive valence
}

TEST_F(EmotionalTaggingRealTest, FromCognitiveState_HighUncertainty_HighArousal) {
    emotional_tag_t tag = emotional_tag_from_cognitive_state(
        0.5f,   // moderate confidence
        0.9f,   // high uncertainty
        0.0f,   // no novelty
        true,   // ethical
        1000
    );

    EXPECT_GT(tag.arousal, 0.0f); // High arousal from uncertainty
}

TEST_F(EmotionalTaggingRealTest, FromCognitiveState_EthicalViolation_NegativeValence) {
    emotional_tag_t tag = emotional_tag_from_cognitive_state(
        0.9f,   // high confidence
        0.1f,   // low uncertainty
        0.0f,   // no novelty
        false,  // NOT ethical
        1000
    );

    EXPECT_LT(tag.valence, 0.0f); // Negative valence
}

TEST_F(EmotionalTaggingRealTest, FromCognitiveState_HighNovelty_PositiveValenceAndArousal) {
    emotional_tag_t tag = emotional_tag_from_cognitive_state(
        0.5f,   // moderate confidence
        0.5f,   // moderate uncertainty
        0.9f,   // HIGH novelty (curiosity)
        true,   // ethical
        1000
    );

    EXPECT_GT(tag.valence, 0.0f);  // Curiosity is positive
    EXPECT_GT(tag.arousal, 0.0f);  // Novelty increases arousal
}

//=============================================================================
// Validation
//=============================================================================

TEST_F(EmotionalTaggingRealTest, IsValid_ValidTag_ReturnsTrue) {
    emotional_tag_t tag = emotional_tag_create(0.5f, 0.5f, 0);
    EXPECT_TRUE(emotional_tag_is_valid(&tag));
}

TEST_F(EmotionalTaggingRealTest, IsValid_InvalidValence_ReturnsFalse) {
    emotional_tag_t tag = emotional_tag_neutral();
    tag.valence = 2.0f; // Invalid

    EXPECT_FALSE(emotional_tag_is_valid(&tag));
}

TEST_F(EmotionalTaggingRealTest, IsValid_InvalidArousal_ReturnsFalse) {
    emotional_tag_t tag = emotional_tag_neutral();
    tag.arousal = -0.5f; // Invalid

    EXPECT_FALSE(emotional_tag_is_valid(&tag));
}

TEST_F(EmotionalTaggingRealTest, Clamp_FixesInvalidValence) {
    emotional_tag_t tag = emotional_tag_neutral();
    tag.valence = 2.0f;

    emotional_tag_clamp(&tag);

    EXPECT_LE(tag.valence, 1.0f);
    EXPECT_TRUE(emotional_tag_is_valid(&tag));
}

TEST_F(EmotionalTaggingRealTest, Clamp_FixesInvalidArousal) {
    emotional_tag_t tag = emotional_tag_neutral();
    tag.arousal = -0.5f;

    emotional_tag_clamp(&tag);

    EXPECT_GE(tag.arousal, 0.0f);
    EXPECT_TRUE(emotional_tag_is_valid(&tag));
}

//=============================================================================
// Bidirectional Feedback Functions (Phase 10.11.3)
//=============================================================================

TEST_F(EmotionalTaggingRealTest, GetValence_ReturnsCorrectValue) {
    emotional_tag_t tag = emotional_tag_create(0.7f, 0.5f, 0);
    float valence = emotional_get_valence(&tag);

    EXPECT_NEAR(valence, 0.7f, 0.01f);
}

TEST_F(EmotionalTaggingRealTest, GetArousal_ReturnsCorrectValue) {
    emotional_tag_t tag = emotional_tag_create(0.5f, 0.8f, 0);
    float arousal = emotional_get_arousal(&tag);

    EXPECT_NEAR(arousal, 0.8f, 0.01f);
}

TEST_F(EmotionalTaggingRealTest, ModulateArousal_IncreasesArousal) {
    emotional_tag_t tag = emotional_tag_create(0.5f, 0.5f, 0);

    emotional_modulate_arousal(&tag, 0.3f);

    EXPECT_NEAR(tag.arousal, 0.8f, 0.01f);
}

TEST_F(EmotionalTaggingRealTest, ModulateArousal_DecreasesArousal) {
    emotional_tag_t tag = emotional_tag_create(0.5f, 0.5f, 0);

    emotional_modulate_arousal(&tag, -0.3f);

    EXPECT_NEAR(tag.arousal, 0.2f, 0.01f);
}

TEST_F(EmotionalTaggingRealTest, ModulateArousal_ClampsToValidRange) {
    emotional_tag_t tag = emotional_tag_create(0.5f, 0.9f, 0);

    emotional_modulate_arousal(&tag, 0.5f); // Would be 1.4, should clamp to 1.0

    EXPECT_LE(tag.arousal, 1.0f);
    EXPECT_TRUE(emotional_tag_is_valid(&tag));
}
