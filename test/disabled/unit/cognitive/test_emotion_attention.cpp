/**
 * @file test_emotion_attention.cpp
 * @brief Unit tests for emotion-modulated attention system
 *
 * WHAT: Comprehensive unit tests for emotion-attention integration
 * WHY:  Ensure emotion tensor correctly modulates attentional processes
 * HOW:  Test all APIs, edge cases, and bio-async integration
 *
 * @author NIMCP Development Team
 * @date 2025-12-09
 */

#include <gtest/gtest.h>
#include "cognitive/attention/nimcp_emotion_attention.h"
#include "cognitive/nimcp_emotion_tensor.h"
#include "plasticity/attention/nimcp_attention.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EmotionAttentionTest : public ::testing::Test {
protected:
    emotion_tensor_system_t* tensor_system;
    multihead_attention_t attention;
    emotion_attention_system_t* ea_system;

    void SetUp() override {
        /* Create emotion tensor system */
        tensor_system = emotion_tensor_create(nullptr);
        ASSERT_NE(tensor_system, nullptr);

        /* Create multihead attention */
        multihead_attention_config_t attention_config = {};
        attention_config.num_heads = 4;
        attention_config.input_dim = 64;
        attention_config.output_dim = 64;
        attention_config.sequence_length = 16;
        attention_config.use_thalamic_gate = true;
        attention_config.use_salience_weighting = true;
        attention_config.gate_bias = 0.5f;

        attention = multihead_attention_create(&attention_config);
        ASSERT_NE(attention, nullptr);

        /* Create emotion-attention system */
        ea_system = emotion_attention_create(tensor_system, attention, nullptr);
        ASSERT_NE(ea_system, nullptr);
    }

    void TearDown() override {
        emotion_attention_destroy(ea_system);
        multihead_attention_destroy(attention);
        emotion_tensor_destroy(tensor_system);
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(EmotionAttentionTest, CreateWithDefaults) {
    /* WHAT: Test creation with default config */
    EXPECT_NE(ea_system, nullptr);

    /* WHAT: Verify initial state */
    float width = emotion_attention_get_width(ea_system);
    EXPECT_NEAR(width, 0.5f, 0.01f);  /* Should start neutral */
}

TEST_F(EmotionAttentionTest, CreateWithCustomConfig) {
    /* WHAT: Create with custom config */
    emotion_attention_config_t config = emotion_attention_default_config();
    config.arousal_narrowing_factor = 0.9f;
    config.valence_broadening_factor = 0.3f;
    config.emotion_salience_boost = 3.0f;

    emotion_attention_system_t* custom = emotion_attention_create(
        tensor_system, attention, &config
    );
    ASSERT_NE(custom, nullptr);

    emotion_attention_destroy(custom);
}

TEST_F(EmotionAttentionTest, CreateWithNullInputs) {
    /* WHAT: Test null pointer handling */
    EXPECT_EQ(emotion_attention_create(nullptr, attention, nullptr), nullptr);
    EXPECT_EQ(emotion_attention_create(tensor_system, nullptr, nullptr), nullptr);
}

TEST_F(EmotionAttentionTest, DestroyNullSafe) {
    /* WHAT: Destroy should handle NULL safely */
    emotion_attention_destroy(nullptr);  /* Should not crash */
}

//=============================================================================
// Attention Width Modulation Tests
//=============================================================================

TEST_F(EmotionAttentionTest, HighArousalNarrowsAttention) {
    /* WHAT: Test that high arousal (fear/anger) narrows attention */

    /* Set high fear (high arousal, negative valence) */
    emotion_tensor_set_channel(tensor_system, TENSOR_FEAR, 0.9f, 0);
    emotion_tensor_set_channel(tensor_system, TENSOR_SADNESS, 0.0f, 0);

    /* Trigger manual update */
    emotion_attention_modulate(ea_system);

    /* Check width narrowed */
    float width = emotion_attention_get_width(ea_system);
    EXPECT_LT(width, 0.5f);  /* Should be narrower than neutral */
    EXPECT_GT(width, 0.1f);  /* But not too narrow */
}

TEST_F(EmotionAttentionTest, PositiveValenceBroadensAttention) {
    /* WHAT: Test that positive emotions (joy) broaden attention */

    /* Set high joy (moderate arousal, positive valence) */
    emotion_tensor_set_channel(tensor_system, TENSOR_JOY, 0.8f, 0);
    emotion_tensor_set_channel(tensor_system, TENSOR_ANTICIPATION, 0.0f, 0);

    /* Trigger manual update */
    emotion_attention_modulate(ea_system);

    /* Check width broadened */
    float width = emotion_attention_get_width(ea_system);
    EXPECT_GT(width, 0.5f);  /* Should be broader than neutral */
}

TEST_F(EmotionAttentionTest, NeutralEmotionMaintainsWidth) {
    /* WHAT: Test neutral emotional state keeps normal width */

    /* Set all emotions low */
    for (int i = 0; i < EMOTION_TENSOR_PRIMARY_COUNT; i++) {
        emotion_tensor_set_channel(tensor_system, (emotion_primary_t)i, 0.1f, 0);
    }

    emotion_attention_modulate(ea_system);

    float width = emotion_attention_get_width(ea_system);
    EXPECT_NEAR(width, 0.5f, 0.15f);  /* Should stay near neutral */
}

TEST_F(EmotionAttentionTest, WidthClampsToLimits) {
    /* WHAT: Test width respects min/max bounds */

    /* Create with tight limits */
    emotion_attention_config_t config = emotion_attention_default_config();
    config.min_attention_width = 0.3f;
    config.max_attention_width = 0.7f;

    emotion_attention_system_t* bounded = emotion_attention_create(
        tensor_system, attention, &config
    );

    /* Set extreme fear (should clamp to min) */
    emotion_tensor_set_channel(tensor_system, TENSOR_FEAR, 1.0f, 0);
    emotion_tensor_set_channel(tensor_system, TENSOR_ANGER, 1.0f, 0);

    float width = emotion_attention_get_width(bounded);
    EXPECT_GE(width, 0.3f);  /* Should respect minimum */
    EXPECT_LE(width, 0.7f);  /* Should respect maximum */

    emotion_attention_destroy(bounded);
}

//=============================================================================
// Salience Modulation Tests
//=============================================================================

TEST_F(EmotionAttentionTest, EmotionCongruenceBooststSalience) {
    /* WHAT: Test that emotion-congruent stimuli get salience boost */

    /* Set fear as dominant emotion */
    emotion_tensor_set_channel(tensor_system, TENSOR_FEAR, 0.8f, 0);

    float base_salience = 0.5f;

    /* Fear-related stimulus should get boost */
    float boosted = emotion_attention_compute_salience(
        ea_system, base_salience, TENSOR_FEAR
    );
    EXPECT_GT(boosted, base_salience);

    /* Joy stimulus should not get boost */
    float unboosted = emotion_attention_compute_salience(
        ea_system, base_salience, TENSOR_JOY
    );
    EXPECT_NEAR(unboosted, base_salience, 0.01f);
}

TEST_F(EmotionAttentionTest, AdjacentEmotionsAreCongruent) {
    /* WHAT: Test that adjacent emotions on Plutchik wheel are congruent */

    /* Set anger dominant */
    emotion_tensor_set_channel(tensor_system, TENSOR_ANGER, 0.7f, 0);

    float base_salience = 0.4f;

    /* Adjacent emotion (anticipation) should get boost */
    float boosted = emotion_attention_compute_salience(
        ea_system, base_salience, TENSOR_ANTICIPATION
    );
    EXPECT_GT(boosted, base_salience);
}

TEST_F(EmotionAttentionTest, SalienceClampsToOne) {
    /* WHAT: Test salience doesn't exceed 1.0 */

    emotion_attention_config_t config = emotion_attention_default_config();
    config.emotion_salience_boost = 5.0f;  /* Very high boost */

    emotion_attention_system_t* high_boost = emotion_attention_create(
        tensor_system, attention, &config
    );

    emotion_tensor_set_channel(tensor_system, TENSOR_FEAR, 0.9f, 0);

    float salience = emotion_attention_compute_salience(
        high_boost, 0.9f, TENSOR_FEAR
    );
    EXPECT_LE(salience, 1.0f);  /* Should clamp at 1.0 */

    emotion_attention_destroy(high_boost);
}

TEST_F(EmotionAttentionTest, SalienceWithNullSystem) {
    /* WHAT: Test salience computation with NULL system */
    float salience = emotion_attention_compute_salience(
        nullptr, 0.5f, TENSOR_JOY
    );
    EXPECT_NEAR(salience, 0.5f, 0.01f);  /* Should return base */
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(EmotionAttentionTest, StatisticsInitiallyZero) {
    /* WHAT: Test statistics start at zero */
    emotion_attention_stats_t stats;
    ASSERT_TRUE(emotion_attention_get_stats(ea_system, &stats));

    EXPECT_EQ(stats.emotion_updates_received, 0);
    EXPECT_EQ(stats.emotional_gating_events, 0);
    EXPECT_EQ(stats.congruency_biases, 0);
    EXPECT_NEAR(stats.current_attention_width, 0.5f, 0.01f);
}

TEST_F(EmotionAttentionTest, StatisticsTrackUpdates) {
    /* WHAT: Test statistics are updated correctly */

    /* Trigger multiple updates */
    for (int i = 0; i < 5; i++) {
        emotion_tensor_set_channel(tensor_system, TENSOR_JOY, 0.5f + i * 0.1f, 0);
        emotion_attention_modulate(ea_system);
    }

    emotion_attention_stats_t stats;
    ASSERT_TRUE(emotion_attention_get_stats(ea_system, &stats));

    EXPECT_GT(stats.emotional_gating_events, 0);
}

TEST_F(EmotionAttentionTest, StatisticsReset) {
    /* WHAT: Test statistics can be reset */

    /* Accumulate some stats */
    emotion_tensor_set_channel(tensor_system, TENSOR_ANGER, 0.8f, 0);
    emotion_attention_modulate(ea_system);
    emotion_attention_compute_salience(ea_system, 0.5f, TENSOR_ANGER);

    /* Reset */
    emotion_attention_reset_stats(ea_system);

    /* Check stats cleared */
    emotion_attention_stats_t stats;
    ASSERT_TRUE(emotion_attention_get_stats(ea_system, &stats));

    EXPECT_EQ(stats.emotion_updates_received, 0);
    EXPECT_EQ(stats.emotional_gating_events, 0);
    EXPECT_EQ(stats.congruency_biases, 0);
}

TEST_F(EmotionAttentionTest, GetStatsWithNullInputs) {
    /* WHAT: Test null pointer handling */
    emotion_attention_stats_t stats;

    EXPECT_FALSE(emotion_attention_get_stats(nullptr, &stats));
    EXPECT_FALSE(emotion_attention_get_stats(ea_system, nullptr));
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(EmotionAttentionTest, HandlesNegativeSalience) {
    /* WHAT: Test negative salience is clamped */
    float salience = emotion_attention_compute_salience(
        ea_system, -0.5f, TENSOR_JOY
    );
    EXPECT_GE(salience, 0.0f);
}

TEST_F(EmotionAttentionTest, HandlesSalienceAboveOne) {
    /* WHAT: Test salience >1.0 is clamped */
    float salience = emotion_attention_compute_salience(
        ea_system, 1.5f, TENSOR_JOY
    );
    EXPECT_LE(salience, 1.0f);
}

TEST_F(EmotionAttentionTest, GetWidthWithNullSystem) {
    /* WHAT: Test width query with NULL */
    float width = emotion_attention_get_width(nullptr);
    EXPECT_LT(width, 0.0f);  /* Should return error value */
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(EmotionAttentionTest, FullEmotionalCycle) {
    /* WHAT: Test complete emotional cycle: fear → joy → neutral */

    /* Phase 1: Fear (high arousal, negative) */
    emotion_tensor_set_channel(tensor_system, TENSOR_FEAR, 0.9f, 0);
    emotion_attention_modulate(ea_system);
    float fear_width = emotion_attention_get_width(ea_system);
    EXPECT_LT(fear_width, 0.5f);  /* Narrowed */

    /* Phase 2: Joy (moderate arousal, positive) */
    emotion_tensor_reset(tensor_system);
    emotion_tensor_set_channel(tensor_system, TENSOR_JOY, 0.8f, 0);
    emotion_attention_modulate(ea_system);
    float joy_width = emotion_attention_get_width(ea_system);
    EXPECT_GT(joy_width, fear_width);  /* Broadened */

    /* Phase 3: Neutral */
    emotion_tensor_reset(tensor_system);
    emotion_attention_modulate(ea_system);
    float neutral_width = emotion_attention_get_width(ea_system);
    EXPECT_NEAR(neutral_width, 0.5f, 0.2f);  /* Back to neutral */
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
