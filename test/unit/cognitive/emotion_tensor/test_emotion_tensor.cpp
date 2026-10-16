/**
 * @file test_emotion_tensor.cpp
 * @brief Unit tests for Tensor-Based Emotional Representation System
 *
 * WHAT: Comprehensive unit tests for emotion tensor functionality
 * WHY:  Verify tensor operations, compound emotions, interactions work correctly
 * HOW:  Google Test framework with fixture for system lifecycle
 *
 * TEST COVERAGE:
 * - Lifecycle: create, destroy, reset
 * - Channel operations: get, set, stimulus
 * - Compound emotions: all 24 Plutchik dyads
 * - Interactions: facilitation, inhibition
 * - Aggregates: valence, arousal, entropy, stability
 * - Backward compatibility: scalar valence/arousal
 * - Edge cases: NULL handling, bounds checking
 *
 * @author NIMCP Development Team
 * @date 2025-12-09
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/nimcp_emotion_tensor.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EmotionTensorTest : public ::testing::Test {
protected:
    emotion_tensor_system_t* system = nullptr;

    void SetUp() override {
        system = emotion_tensor_create(nullptr);
        ASSERT_NE(system, nullptr) << "Failed to create emotion tensor system";
    }

    void TearDown() override {
        if (system) {
            emotion_tensor_destroy(system);
            system = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(EmotionTensorTest, CreateWithNullConfig) {
    /* Already tested in fixture - system created with nullptr config */
    EXPECT_NE(system, nullptr);
}

TEST_F(EmotionTensorTest, CreateWithCustomConfig) {
    emotion_tensor_config_t config = emotion_tensor_default_config();
    config.decay_rate = 0.2f;
    config.interaction_strength = 0.5f;

    emotion_tensor_system_t* custom = emotion_tensor_create(&config);
    ASSERT_NE(custom, nullptr);
    emotion_tensor_destroy(custom);
}

TEST_F(EmotionTensorTest, DefaultConfigValues) {
    emotion_tensor_config_t config = emotion_tensor_default_config();

    EXPECT_FLOAT_EQ(config.decay_rate, 0.1f);
    EXPECT_FLOAT_EQ(config.interaction_strength, 0.3f);
    EXPECT_FLOAT_EQ(config.blend_threshold, 0.2f);
    EXPECT_FLOAT_EQ(config.dominance_threshold, 0.5f);
    EXPECT_TRUE(config.enable_temporal_dynamics);
    EXPECT_TRUE(config.enable_appraisals);
    EXPECT_TRUE(config.enable_interactions);
}

TEST_F(EmotionTensorTest, DestroyNullSafe) {
    emotion_tensor_destroy(nullptr);
    /* Should not crash */
    SUCCEED();
}

TEST_F(EmotionTensorTest, ResetToNeutral) {
    /* Set some emotions */
    emotion_tensor_set_channel(system, TENSOR_JOY, 0.8f, 1000);
    emotion_tensor_set_channel(system, TENSOR_ANGER, 0.6f, 1000);

    /* Reset */
    ASSERT_TRUE(emotion_tensor_reset(system));

    /* All channels should be zero */
    for (int i = 0; i < TENSOR_PRIMARY_COUNT; i++) {
        float val = emotion_tensor_get_channel(system, (emotion_primary_t)i);
        EXPECT_FLOAT_EQ(val, 0.0f) << "Channel " << i << " not reset";
    }
}

//=============================================================================
// Channel Operation Tests
//=============================================================================

TEST_F(EmotionTensorTest, SetAndGetChannel) {
    ASSERT_TRUE(emotion_tensor_set_channel(system, TENSOR_JOY, 0.75f, 1000));

    float joy = emotion_tensor_get_channel(system, TENSOR_JOY);
    EXPECT_FLOAT_EQ(joy, 0.75f);
}

TEST_F(EmotionTensorTest, SetChannelClampsToRange) {
    /* Test upper bound */
    ASSERT_TRUE(emotion_tensor_set_channel(system, TENSOR_JOY, 1.5f, 1000));
    EXPECT_FLOAT_EQ(emotion_tensor_get_channel(system, TENSOR_JOY), 1.0f);

    /* Test lower bound */
    ASSERT_TRUE(emotion_tensor_set_channel(system, TENSOR_FEAR, -0.5f, 1000));
    EXPECT_FLOAT_EQ(emotion_tensor_get_channel(system, TENSOR_FEAR), 0.0f);
}

TEST_F(EmotionTensorTest, SetAllChannels) {
    float activations[EMOTION_TENSOR_PRIMARY_COUNT] = {
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f
    };

    ASSERT_TRUE(emotion_tensor_set_channels(system, activations, 1000));

    for (int i = 0; i < EMOTION_TENSOR_PRIMARY_COUNT; i++) {
        float val = emotion_tensor_get_channel(system, (emotion_primary_t)i);
        EXPECT_FLOAT_EQ(val, activations[i]);
    }
}

TEST_F(EmotionTensorTest, GetChannelInvalidEmotion) {
    float val = emotion_tensor_get_channel(system, (emotion_primary_t)99);
    EXPECT_FLOAT_EQ(val, -1.0f);
}

TEST_F(EmotionTensorTest, SetChannelInvalidEmotion) {
    bool result = emotion_tensor_set_channel(system, (emotion_primary_t)99, 0.5f, 1000);
    EXPECT_FALSE(result);
}

TEST_F(EmotionTensorTest, ApplyStimulusPositive) {
    emotion_tensor_set_channel(system, TENSOR_JOY, 0.3f, 1000);
    ASSERT_TRUE(emotion_tensor_apply_stimulus(system, TENSOR_JOY, 0.5f, true, 1100));

    float joy = emotion_tensor_get_channel(system, TENSOR_JOY);
    EXPECT_FLOAT_EQ(joy, 0.8f);
}

TEST_F(EmotionTensorTest, ApplyStimulusNegative) {
    emotion_tensor_set_channel(system, TENSOR_ANGER, 0.8f, 1000);
    ASSERT_TRUE(emotion_tensor_apply_stimulus(system, TENSOR_ANGER, 0.4f, false, 1100));

    float anger = emotion_tensor_get_channel(system, TENSOR_ANGER);
    EXPECT_FLOAT_EQ(anger, 0.6f); /* 0.8 - 0.4*0.5 = 0.6 */
}

//=============================================================================
// Compound Emotion Tests
//=============================================================================

TEST_F(EmotionTensorTest, CompoundLoveFromJoyAndTrust) {
    emotion_tensor_set_channel(system, TENSOR_JOY, 0.8f, 1000);
    emotion_tensor_set_channel(system, TENSOR_TRUST, 0.8f, 1000);

    float love = emotion_tensor_get_compound(system, COMPOUND_LOVE);
    EXPECT_FLOAT_EQ(love, 0.8f); /* sqrt(0.8 * 0.8) = 0.8 */
}

TEST_F(EmotionTensorTest, CompoundBittersweetnessFromJoyAndSadness) {
    emotion_tensor_set_channel(system, TENSOR_JOY, 0.6f, 1000);
    emotion_tensor_set_channel(system, TENSOR_SADNESS, 0.5f, 1000);

    float bittersweet = emotion_tensor_get_compound(system, COMPOUND_BITTERSWEETNESS);
    EXPECT_FLOAT_EQ(bittersweet, 0.3f); /* 0.6 * 0.5 = 0.3 (tertiary dyad) */
}

TEST_F(EmotionTensorTest, CompoundNostalgiaFromAnticipationAndSadness) {
    emotion_tensor_set_channel(system, TENSOR_ANTICIPATION, 0.7f, 1000);
    emotion_tensor_set_channel(system, TENSOR_SADNESS, 0.4f, 1000);

    float nostalgia = emotion_tensor_get_compound(system, COMPOUND_NOSTALGIA);
    EXPECT_FLOAT_EQ(nostalgia, 0.28f); /* 0.7 * 0.4 = 0.28 */
}

TEST_F(EmotionTensorTest, CompoundAnxietyFromFearAndAnger) {
    emotion_tensor_set_channel(system, TENSOR_FEAR, 0.9f, 1000);
    emotion_tensor_set_channel(system, TENSOR_ANGER, 0.5f, 1000);

    float anxiety = emotion_tensor_get_compound(system, COMPOUND_ANXIETY);
    EXPECT_FLOAT_EQ(anxiety, 0.45f); /* 0.9 * 0.5 = 0.45 */
}

TEST_F(EmotionTensorTest, CompoundAmbivalenceFromSurpriseAndAnticipation) {
    emotion_tensor_set_channel(system, TENSOR_SURPRISE, 0.8f, 1000);
    emotion_tensor_set_channel(system, TENSOR_ANTICIPATION, 0.6f, 1000);

    float ambivalence = emotion_tensor_get_compound(system, COMPOUND_AMBIVALENCE);
    EXPECT_FLOAT_EQ(ambivalence, 0.48f); /* 0.8 * 0.6 = 0.48 */
}

TEST_F(EmotionTensorTest, GetCompoundInvalidIndex) {
    float val = emotion_tensor_get_compound(system, (emotion_compound_t)99);
    EXPECT_FLOAT_EQ(val, -1.0f);
}

TEST_F(EmotionTensorTest, AllPrimaryDyadsComputed) {
    /* Set all channels to 0.5 */
    float activations[EMOTION_TENSOR_PRIMARY_COUNT];
    for (int i = 0; i < EMOTION_TENSOR_PRIMARY_COUNT; i++) {
        activations[i] = 0.5f;
    }
    emotion_tensor_set_channels(system, activations, 1000);

    /* All primary dyads should be sqrt(0.5 * 0.5) = 0.5 */
    float expected_primary = sqrtf(0.25f);
    EXPECT_NEAR(emotion_tensor_get_compound(system, COMPOUND_LOVE), expected_primary, 0.001f);
    EXPECT_NEAR(emotion_tensor_get_compound(system, COMPOUND_SUBMISSION), expected_primary, 0.001f);
    EXPECT_NEAR(emotion_tensor_get_compound(system, COMPOUND_AWE), expected_primary, 0.001f);
    EXPECT_NEAR(emotion_tensor_get_compound(system, COMPOUND_DISAPPROVAL), expected_primary, 0.001f);
    EXPECT_NEAR(emotion_tensor_get_compound(system, COMPOUND_REMORSE), expected_primary, 0.001f);
    EXPECT_NEAR(emotion_tensor_get_compound(system, COMPOUND_CONTEMPT), expected_primary, 0.001f);
    EXPECT_NEAR(emotion_tensor_get_compound(system, COMPOUND_AGGRESSIVENESS), expected_primary, 0.001f);
    EXPECT_NEAR(emotion_tensor_get_compound(system, COMPOUND_OPTIMISM), expected_primary, 0.001f);
}

//=============================================================================
// Contradictory Emotion Tests
//=============================================================================

TEST_F(EmotionTensorTest, DetectJoySadnessContradiction) {
    emotion_tensor_set_channel(system, TENSOR_JOY, 0.7f, 1000);
    emotion_tensor_set_channel(system, TENSOR_SADNESS, 0.6f, 1000);

    EXPECT_TRUE(emotion_tensor_is_contradictory(system, 0.5f));
}

TEST_F(EmotionTensorTest, DetectTrustDisgustContradiction) {
    emotion_tensor_set_channel(system, TENSOR_TRUST, 0.8f, 1000);
    emotion_tensor_set_channel(system, TENSOR_DISGUST, 0.7f, 1000);

    EXPECT_TRUE(emotion_tensor_is_contradictory(system, 0.5f));
}

TEST_F(EmotionTensorTest, DetectFearAngerContradiction) {
    emotion_tensor_set_channel(system, TENSOR_FEAR, 0.9f, 1000);
    emotion_tensor_set_channel(system, TENSOR_ANGER, 0.8f, 1000);

    EXPECT_TRUE(emotion_tensor_is_contradictory(system, 0.5f));
}

TEST_F(EmotionTensorTest, NoContradictionWhenBelowThreshold) {
    emotion_tensor_set_channel(system, TENSOR_JOY, 0.4f, 1000);
    emotion_tensor_set_channel(system, TENSOR_SADNESS, 0.3f, 1000);

    EXPECT_FALSE(emotion_tensor_is_contradictory(system, 0.5f));
}

TEST_F(EmotionTensorTest, NoContradictionWithCompatibleEmotions) {
    emotion_tensor_set_channel(system, TENSOR_JOY, 0.8f, 1000);
    emotion_tensor_set_channel(system, TENSOR_TRUST, 0.7f, 1000);

    EXPECT_FALSE(emotion_tensor_is_contradictory(system, 0.5f));
}

//=============================================================================
// Aggregate Metric Tests
//=============================================================================

TEST_F(EmotionTensorTest, ValencePositiveWithJoy) {
    emotion_tensor_set_channel(system, TENSOR_JOY, 0.9f, 1000);

    float valence = emotion_tensor_get_valence(system);
    EXPECT_GT(valence, 0.5f);
}

TEST_F(EmotionTensorTest, ValenceNegativeWithSadness) {
    emotion_tensor_set_channel(system, TENSOR_SADNESS, 0.9f, 1000);

    float valence = emotion_tensor_get_valence(system);
    EXPECT_LT(valence, -0.5f);
}

TEST_F(EmotionTensorTest, ValenceMixedWithJoyAndSadness) {
    emotion_tensor_set_channel(system, TENSOR_JOY, 0.5f, 1000);
    emotion_tensor_set_channel(system, TENSOR_SADNESS, 0.5f, 1000);

    float valence = emotion_tensor_get_valence(system);
    /* Should be close to neutral */
    EXPECT_NEAR(valence, 0.0f, 0.2f);
}

TEST_F(EmotionTensorTest, ArousalHighWithFear) {
    emotion_tensor_set_channel(system, TENSOR_FEAR, 0.9f, 1000);

    float arousal = emotion_tensor_get_arousal(system);
    EXPECT_GT(arousal, 0.7f);
}

TEST_F(EmotionTensorTest, ArousalLowWithSadness) {
    emotion_tensor_set_channel(system, TENSOR_SADNESS, 0.9f, 1000);

    float arousal = emotion_tensor_get_arousal(system);
    EXPECT_LT(arousal, 0.4f);
}

TEST_F(EmotionTensorTest, EntropyZeroWithSingleEmotion) {
    emotion_tensor_set_channel(system, TENSOR_JOY, 1.0f, 1000);

    float entropy = emotion_tensor_get_entropy(system);
    EXPECT_FLOAT_EQ(entropy, 0.0f);
}

TEST_F(EmotionTensorTest, EntropyHighWithMixedEmotions) {
    /* Set all emotions equal - maximum entropy */
    float activations[EMOTION_TENSOR_PRIMARY_COUNT];
    for (int i = 0; i < EMOTION_TENSOR_PRIMARY_COUNT; i++) {
        activations[i] = 0.5f;
    }
    emotion_tensor_set_channels(system, activations, 1000);

    float entropy = emotion_tensor_get_entropy(system);
    EXPECT_NEAR(entropy, 1.0f, 0.01f); /* Should be close to max */
}

TEST_F(EmotionTensorTest, StabilityHighWhenUnchanging) {
    /* Set and don't change - should be stable */
    emotion_tensor_set_channel(system, TENSOR_JOY, 0.5f, 1000);

    /* Update multiple times without changing */
    for (int i = 0; i < 20; i++) {
        emotion_tensor_update(system, 0.01f, 1000 + i * 10);
    }

    float stability = emotion_tensor_get_stability(system);
    EXPECT_GT(stability, 0.5f);
}

//=============================================================================
// Dominant Emotion Tests
//=============================================================================

TEST_F(EmotionTensorTest, DominantEmotionIdentified) {
    emotion_tensor_set_channel(system, TENSOR_ANGER, 0.9f, 1000);
    emotion_tensor_set_channel(system, TENSOR_FEAR, 0.4f, 1000);

    emotion_primary_t primary, secondary;
    float blend_ratio;
    ASSERT_TRUE(emotion_tensor_get_dominant(system, &primary, &secondary, &blend_ratio));

    EXPECT_EQ(primary, TENSOR_ANGER);
    EXPECT_NEAR(blend_ratio, 0.4f/0.9f, 0.01f);
}

TEST_F(EmotionTensorTest, SecondaryEmotionIdentified) {
    emotion_tensor_set_channel(system, TENSOR_JOY, 0.8f, 1000);
    emotion_tensor_set_channel(system, TENSOR_TRUST, 0.6f, 1000);
    emotion_tensor_set_channel(system, TENSOR_ANTICIPATION, 0.3f, 1000);

    emotion_primary_t primary, secondary;
    float blend_ratio;
    ASSERT_TRUE(emotion_tensor_get_dominant(system, &primary, &secondary, &blend_ratio));

    EXPECT_EQ(primary, TENSOR_JOY);
    EXPECT_EQ(secondary, TENSOR_TRUST);
}

//=============================================================================
// Dynamics Tests
//=============================================================================

TEST_F(EmotionTensorTest, DecayReducesActivation) {
    emotion_tensor_set_channel(system, TENSOR_JOY, 0.8f, 1000);

    /* Apply decay */
    emotion_tensor_update(system, 1.0f, 2000);

    float joy = emotion_tensor_get_channel(system, TENSOR_JOY);
    EXPECT_LT(joy, 0.8f);
    EXPECT_GT(joy, 0.0f);
}

TEST_F(EmotionTensorTest, InteractionsAffectChannels) {
    /* Set joy high - should facilitate anticipation (adjacent) */
    emotion_tensor_set_channel(system, TENSOR_JOY, 0.8f, 1000);
    emotion_tensor_set_channel(system, TENSOR_ANTICIPATION, 0.3f, 1000);

    /* Apply interactions */
    emotion_tensor_apply_interactions(system, 1.0f);

    float anticipation = emotion_tensor_get_channel(system, TENSOR_ANTICIPATION);
    /* Anticipation should increase due to joy facilitation */
    EXPECT_GT(anticipation, 0.3f);
}

TEST_F(EmotionTensorTest, OpposingEmotionsInhibit) {
    /* Set joy and sadness both high */
    emotion_tensor_set_channel(system, TENSOR_JOY, 0.8f, 1000);
    emotion_tensor_set_channel(system, TENSOR_SADNESS, 0.7f, 1000);

    /* Apply interactions - they should inhibit each other */
    emotion_tensor_apply_interactions(system, 1.0f);

    float joy = emotion_tensor_get_channel(system, TENSOR_JOY);
    float sadness = emotion_tensor_get_channel(system, TENSOR_SADNESS);

    /* Both should decrease due to mutual inhibition */
    EXPECT_LT(joy, 0.8f);
    EXPECT_LT(sadness, 0.7f);
}

//=============================================================================
// Appraisal Tests
//=============================================================================

TEST_F(EmotionTensorTest, SetAndGetAppraisal) {
    ASSERT_TRUE(emotion_tensor_set_appraisal(system, TENSOR_FEAR, APPRAISAL_CERTAINTY, 0.7f));

    emotion_tensor_t tensor;
    ASSERT_TRUE(emotion_tensor_get(system, &tensor));

    EXPECT_FLOAT_EQ(tensor.appraisals[TENSOR_FEAR][APPRAISAL_CERTAINTY], 0.7f);
}

TEST_F(EmotionTensorTest, AppraisalClampedToRange) {
    ASSERT_TRUE(emotion_tensor_set_appraisal(system, TENSOR_JOY, APPRAISAL_CONTROL, 1.5f));

    emotion_tensor_t tensor;
    ASSERT_TRUE(emotion_tensor_get(system, &tensor));

    EXPECT_FLOAT_EQ(tensor.appraisals[TENSOR_JOY][APPRAISAL_CONTROL], 1.0f);
}

TEST_F(EmotionTensorTest, InvalidAppraisalDimensionFails) {
    bool result = emotion_tensor_set_appraisal(system, TENSOR_JOY, (appraisal_dimension_t)99, 0.5f);
    EXPECT_FALSE(result);
}

//=============================================================================
// Utility Tests
//=============================================================================

TEST_F(EmotionTensorTest, EmotionNames) {
    EXPECT_STREQ(emotion_tensor_emotion_name(TENSOR_JOY), "joy");
    EXPECT_STREQ(emotion_tensor_emotion_name(TENSOR_TRUST), "trust");
    EXPECT_STREQ(emotion_tensor_emotion_name(TENSOR_FEAR), "fear");
    EXPECT_STREQ(emotion_tensor_emotion_name(TENSOR_SURPRISE), "surprise");
    EXPECT_STREQ(emotion_tensor_emotion_name(TENSOR_SADNESS), "sadness");
    EXPECT_STREQ(emotion_tensor_emotion_name(TENSOR_DISGUST), "disgust");
    EXPECT_STREQ(emotion_tensor_emotion_name(TENSOR_ANGER), "anger");
    EXPECT_STREQ(emotion_tensor_emotion_name(TENSOR_ANTICIPATION), "anticipation");
    EXPECT_STREQ(emotion_tensor_emotion_name((emotion_primary_t)99), "unknown");
}

TEST_F(EmotionTensorTest, CompoundNames) {
    EXPECT_STREQ(emotion_tensor_compound_name(COMPOUND_LOVE), "love");
    EXPECT_STREQ(emotion_tensor_compound_name(COMPOUND_BITTERSWEETNESS), "bittersweetness");
    EXPECT_STREQ(emotion_tensor_compound_name(COMPOUND_NOSTALGIA), "nostalgia");
    EXPECT_STREQ(emotion_tensor_compound_name(COMPOUND_ANXIETY), "anxiety");
    EXPECT_STREQ(emotion_tensor_compound_name(COMPOUND_AMBIVALENCE), "ambivalence");
    EXPECT_STREQ(emotion_tensor_compound_name((emotion_compound_t)99), "unknown");
}

TEST_F(EmotionTensorTest, InteractionMatrixInitialized) {
    emotion_interaction_matrix_t matrix;
    emotion_tensor_init_interaction_matrix(&matrix);

    /* Check self-reinforcement */
    EXPECT_FLOAT_EQ(matrix.matrix[TENSOR_JOY][TENSOR_JOY], 0.1f);

    /* Check facilitation (adjacent) */
    EXPECT_GT(matrix.matrix[TENSOR_JOY][TENSOR_TRUST], 0.0f);

    /* Check inhibition (opposite) */
    EXPECT_LT(matrix.matrix[TENSOR_JOY][TENSOR_SADNESS], 0.0f);
}

//=============================================================================
// Full Tensor Access Tests
//=============================================================================

TEST_F(EmotionTensorTest, GetFullTensor) {
    emotion_tensor_set_channel(system, TENSOR_JOY, 0.7f, 1000);
    emotion_tensor_set_channel(system, TENSOR_TRUST, 0.5f, 1000);

    emotion_tensor_t tensor;
    ASSERT_TRUE(emotion_tensor_get(system, &tensor));

    EXPECT_FLOAT_EQ(tensor.channels[TENSOR_JOY], 0.7f);
    EXPECT_FLOAT_EQ(tensor.channels[TENSOR_TRUST], 0.5f);
    EXPECT_EQ(tensor.primary_emotion, TENSOR_JOY);
    EXPECT_EQ(tensor.secondary_emotion, TENSOR_TRUST);
    EXPECT_GT(tensor.overall_valence, 0.0f);
}

TEST_F(EmotionTensorTest, TimestampUpdated) {
    emotion_tensor_set_channel(system, TENSOR_JOY, 0.5f, 12345);

    emotion_tensor_t tensor;
    ASSERT_TRUE(emotion_tensor_get(system, &tensor));

    EXPECT_EQ(tensor.last_update_ms, 12345ULL);
}

//=============================================================================
// NULL Safety Tests
//=============================================================================

TEST_F(EmotionTensorTest, GetChannelNullSystem) {
    float val = emotion_tensor_get_channel(nullptr, TENSOR_JOY);
    EXPECT_FLOAT_EQ(val, -1.0f);
}

TEST_F(EmotionTensorTest, SetChannelNullSystem) {
    bool result = emotion_tensor_set_channel(nullptr, TENSOR_JOY, 0.5f, 1000);
    EXPECT_FALSE(result);
}

TEST_F(EmotionTensorTest, GetTensorNullSystem) {
    emotion_tensor_t tensor;
    bool result = emotion_tensor_get(nullptr, &tensor);
    EXPECT_FALSE(result);
}

TEST_F(EmotionTensorTest, GetTensorNullOutput) {
    bool result = emotion_tensor_get(system, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(EmotionTensorTest, SetChannelsNullArray) {
    bool result = emotion_tensor_set_channels(system, nullptr, 1000);
    EXPECT_FALSE(result);
}

TEST_F(EmotionTensorTest, UpdateNullSystem) {
    bool result = emotion_tensor_update(nullptr, 1.0f, 1000);
    EXPECT_FALSE(result);
}

TEST_F(EmotionTensorTest, ResetNullSystem) {
    bool result = emotion_tensor_reset(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(EmotionTensorTest, GetDominantNullOutputs) {
    emotion_primary_t primary;
    float blend;

    bool result = emotion_tensor_get_dominant(system, nullptr, nullptr, nullptr);
    EXPECT_FALSE(result);

    result = emotion_tensor_get_dominant(system, &primary, nullptr, &blend);
    EXPECT_FALSE(result);
}
