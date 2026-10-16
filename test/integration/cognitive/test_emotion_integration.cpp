/**
 * @file test_emotion_integration.cpp
 * @brief Integration tests for complete emotion-cognition system
 *
 * WHAT: End-to-end tests of emotion tensor integration with attention and memory
 * WHY:  Verify entire system works together correctly
 * HOW:  Test realistic scenarios with all components
 *
 * @author NIMCP Development Team
 * @date 2025-12-09
 */

#include <gtest/gtest.h>
#include "cognitive/attention/nimcp_emotion_attention.h"
#include "cognitive/consolidation/nimcp_emotion_consolidation.h"
#include "cognitive/nimcp_emotion_tensor.h"
#include "plasticity/attention/nimcp_attention.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EmotionIntegrationTest : public ::testing::Test {
protected:
    emotion_tensor_system_t* tensor;
    multihead_attention_t attention;
    emotion_attention_system_t* ea_system;
    emotion_consolidation_system_t* ec_system;

    void SetUp() override {
        /* Create emotion tensor */
        tensor = emotion_tensor_create(nullptr);
        ASSERT_NE(tensor, nullptr);

        /* Create attention system */
        multihead_attention_config_t att_config = {};
        att_config.num_heads = 4;
        att_config.input_dim = 64;
        att_config.output_dim = 64;
        att_config.sequence_length = 16;
        att_config.use_thalamic_gate = true;
        att_config.use_salience_weighting = true;

        attention = multihead_attention_create(&att_config);
        ASSERT_NE(attention, nullptr);

        /* Create emotion-attention integration */
        ea_system = emotion_attention_create(tensor, attention, nullptr);
        ASSERT_NE(ea_system, nullptr);

        /* Create emotion-consolidation integration */
        ec_system = emotion_consolidation_create(tensor, nullptr, nullptr);
        ASSERT_NE(ec_system, nullptr);
    }

    void TearDown() override {
        emotion_consolidation_destroy(ec_system);
        emotion_attention_destroy(ea_system);
        multihead_attention_destroy(attention);
        emotion_tensor_destroy(tensor);
    }
};

//=============================================================================
// Scenario: Fearful Attention Narrowing + Strong Consolidation
//=============================================================================

TEST_F(EmotionIntegrationTest, FearNarrowsAttentionAndBoostsConsolidation) {
    /* WHAT: Test fear triggers attention narrowing AND memory boost */
    /* WHY:  This is the biologically-inspired behavior */

    /* Scenario: Encounter a threat (high fear) */
    emotion_tensor_set_channel(tensor, TENSOR_FEAR, 0.9f, 1000);

    /* PART 1: Attention narrows to threat */
    emotion_attention_modulate(ea_system);
    float width = emotion_attention_get_width(ea_system);
    EXPECT_LT(width, 0.5f);  /* Narrowed focus */

    /* PART 2: Threat-related stimuli get salience boost */
    float threat_salience = emotion_attention_compute_salience(
        ea_system, 0.5f, TENSOR_FEAR
    );
    float neutral_salience = emotion_attention_compute_salience(
        ea_system, 0.5f, TENSOR_JOY
    );
    EXPECT_GT(threat_salience, neutral_salience);

    /* PART 3: Memory encoding during fear gets emotional tag */
    memory_emotion_tag_t tag;
    ASSERT_TRUE(emotion_consolidation_tag_memory(ec_system, &tag));
    EXPECT_TRUE(tag.is_emotionally_tagged);
    EXPECT_EQ(tag.dominant_emotion, TENSOR_FEAR);

    /* PART 4: Memory consolidation is strongly boosted */
    float consolidation = emotion_consolidation_compute_strength(ec_system, 0.3f, &tag);
    EXPECT_GT(consolidation, 0.6f);  /* Should be doubled or more */

    /* PART 5: Memory resists decay */
    float decay = emotion_consolidation_modulate_decay(ec_system, 0.5f, &tag);
    EXPECT_LT(decay, 0.4f);  /* Slower decay */
}

//=============================================================================
// Scenario: Joyful Attention Broadening + Moderate Consolidation
//=============================================================================

TEST_F(EmotionIntegrationTest, JoyBroadensAttentionAndModerateConsolidation) {
    /* WHAT: Test joy triggers attention broadening */
    /* WHY:  Positive emotions broaden cognition (Fredrickson) */

    /* Scenario: Positive event (high joy) */
    emotion_tensor_set_channel(tensor, TENSOR_JOY, 0.8f, 2000);

    /* PART 1: Attention broadens */
    emotion_attention_modulate(ea_system);
    float width = emotion_attention_get_width(ea_system);
    EXPECT_GT(width, 0.5f);  /* Broadened scope */

    /* PART 2: Joy-related stimuli get boost */
    float joy_salience = emotion_attention_compute_salience(
        ea_system, 0.5f, TENSOR_JOY
    );
    EXPECT_GT(joy_salience, 0.5f);

    /* PART 3: Memory gets emotional tag */
    memory_emotion_tag_t tag;
    ASSERT_TRUE(emotion_consolidation_tag_memory(ec_system, &tag));
    EXPECT_TRUE(tag.is_emotionally_tagged);  /* Joy has moderate arousal */

    /* PART 4: Consolidation moderately boosted */
    float consolidation = emotion_consolidation_compute_strength(ec_system, 0.3f, &tag);
    EXPECT_GT(consolidation, 0.3f);  /* Some boost */
    EXPECT_LT(consolidation, 0.9f);  /* But less than fear */
}

//=============================================================================
// Scenario: Neutral State
//=============================================================================

TEST_F(EmotionIntegrationTest, NeutralStateNormalBehavior) {
    /* WHAT: Test neutral emotion allows normal cognitive function */

    /* Scenario: No strong emotions */
    emotion_tensor_reset(tensor);

    /* PART 1: Attention at normal width */
    emotion_attention_modulate(ea_system);
    float width = emotion_attention_get_width(ea_system);
    EXPECT_NEAR(width, 0.5f, 0.2f);  /* Near neutral */

    /* PART 2: No salience bias */
    float salience1 = emotion_attention_compute_salience(ea_system, 0.5f, TENSOR_JOY);
    float salience2 = emotion_attention_compute_salience(ea_system, 0.5f, TENSOR_FEAR);
    EXPECT_NEAR(salience1, salience2, 0.1f);  /* Similar */

    /* PART 3: Memory not emotionally tagged */
    memory_emotion_tag_t tag;
    ASSERT_TRUE(emotion_consolidation_tag_memory(ec_system, &tag));
    EXPECT_FALSE(tag.is_emotionally_tagged);

    /* PART 4: Normal consolidation */
    float consolidation = emotion_consolidation_compute_strength(ec_system, 0.4f, &tag);
    EXPECT_NEAR(consolidation, 0.4f, 0.05f);  /* Unchanged */
}

//=============================================================================
// Scenario: Emotional Transition
//=============================================================================

TEST_F(EmotionIntegrationTest, EmotionalTransitionUpdatesAllSystems) {
    /* WHAT: Test emotion changes propagate through all systems */

    /* Phase 1: Start with fear */
    emotion_tensor_set_channel(tensor, TENSOR_FEAR, 0.8f, 3000);
    emotion_attention_modulate(ea_system);

    float fear_width = emotion_attention_get_width(ea_system);

    memory_emotion_tag_t fear_tag;
    emotion_consolidation_tag_memory(ec_system, &fear_tag);

    /* Phase 2: Transition to joy */
    emotion_tensor_reset(tensor);
    emotion_tensor_set_channel(tensor, TENSOR_JOY, 0.7f, 4000);
    emotion_attention_modulate(ea_system);

    float joy_width = emotion_attention_get_width(ea_system);

    memory_emotion_tag_t joy_tag;
    emotion_consolidation_tag_memory(ec_system, &joy_tag);

    /* Verify transition */
    EXPECT_NE(fear_tag.dominant_emotion, joy_tag.dominant_emotion);
    EXPECT_LT(fear_width, joy_width);  /* Width broadened */
    EXPECT_GT(joy_tag.valence, fear_tag.valence);  /* More positive */
}

//=============================================================================
// Scenario: Mixed Emotions (Bittersweet)
//=============================================================================

TEST_F(EmotionIntegrationTest, MixedEmotionsComplexModulation) {
    /* WHAT: Test mixed emotions produce complex behavior */

    /* Scenario: Bittersweet (joy + sadness) */
    emotion_tensor_set_channel(tensor, TENSOR_JOY, 0.6f, 5000);
    emotion_tensor_set_channel(tensor, TENSOR_SADNESS, 0.5f, 5000);

    /* Check compound emotion is active */
    float bittersweet = emotion_tensor_get_compound(tensor, COMPOUND_BITTERSWEETNESS);
    EXPECT_GT(bittersweet, 0.0f);

    /* Tag memory during mixed state */
    memory_emotion_tag_t tag;
    ASSERT_TRUE(emotion_consolidation_tag_memory(ec_system, &tag));

    /* Mixed emotions should still boost consolidation */
    EXPECT_TRUE(tag.is_emotionally_tagged);

    /* Consolidation should be moderate */
    float consolidation = emotion_consolidation_compute_strength(ec_system, 0.3f, &tag);
    EXPECT_GT(consolidation, 0.3f);
}

//=============================================================================
// Scenario: Stress Reaction (Fear + Anger)
//=============================================================================

TEST_F(EmotionIntegrationTest, StressReactionMaximalEffects) {
    /* WHAT: Test high-stress state (fear + anger) */

    /* Scenario: Extreme stress */
    emotion_tensor_set_channel(tensor, TENSOR_FEAR, 0.9f, 6000);
    emotion_tensor_set_channel(tensor, TENSOR_ANGER, 0.8f, 6000);

    /* PART 1: Attention maximally narrowed */
    emotion_attention_modulate(ea_system);
    float width = emotion_attention_get_width(ea_system);
    EXPECT_LT(width, 0.3f);  /* Very narrow */

    /* PART 2: Memory consolidation maximally boosted */
    memory_emotion_tag_t tag;
    ASSERT_TRUE(emotion_consolidation_tag_memory(ec_system, &tag));

    float consolidation = emotion_consolidation_compute_strength(ec_system, 0.3f, &tag);
    EXPECT_GT(consolidation, 0.7f);  /* Strong boost */

    /* PART 3: Anxiety compound active */
    float anxiety = emotion_tensor_get_compound(tensor, COMPOUND_ANXIETY);
    EXPECT_GT(anxiety, 0.5f);  /* High anxiety */
}

//=============================================================================
// Scenario: Statistics Tracking Across Systems
//=============================================================================

TEST_F(EmotionIntegrationTest, StatisticsTrackCrossSystemActivity) {
    /* WHAT: Test statistics correctly track activity across systems */

    /* Generate activity */
    for (int i = 0; i < 5; i++) {
        /* Vary emotions */
        emotion_primary_t emotion = (emotion_primary_t)(i % EMOTION_TENSOR_PRIMARY_COUNT);
        emotion_tensor_set_channel(tensor, emotion, 0.7f, 7000 + i * 100);

        /* Modulate attention */
        emotion_attention_modulate(ea_system);

        /* Tag memories */
        memory_emotion_tag_t tag;
        emotion_consolidation_tag_memory(ec_system, &tag);

        /* Compute salience */
        emotion_attention_compute_salience(ea_system, 0.5f, emotion);

        /* Compute consolidation */
        emotion_consolidation_compute_strength(ec_system, 0.4f, &tag);
    }

    /* Check attention stats */
    emotion_attention_stats_t att_stats;
    ASSERT_TRUE(emotion_attention_get_stats(ea_system, &att_stats));
    EXPECT_GT(att_stats.emotional_gating_events, 0);

    /* Check consolidation stats */
    emotion_consolidation_stats_t cons_stats;
    ASSERT_TRUE(emotion_consolidation_get_stats(ec_system, &cons_stats));
    EXPECT_GT(cons_stats.emotional_memories_tagged, 0);
}

//=============================================================================
// Scenario: Emotional Memory Persistence
//=============================================================================

TEST_F(EmotionIntegrationTest, EmotionalMemoriesPersistLonger) {
    /* WHAT: Test emotional memories decay slower than neutral */

    /* Create emotional memory */
    emotion_tensor_set_channel(tensor, TENSOR_SADNESS, 0.8f, 8000);

    memory_emotion_tag_t emotional_tag;
    ASSERT_TRUE(emotion_consolidation_tag_memory(ec_system, &emotional_tag));

    /* Create neutral memory */
    emotion_tensor_reset(tensor);

    memory_emotion_tag_t neutral_tag;
    ASSERT_TRUE(emotion_consolidation_tag_memory(ec_system, &neutral_tag));

    /* Compare decay rates */
    float base_decay = 0.5f;
    float emotional_decay = emotion_consolidation_modulate_decay(
        ec_system, base_decay, &emotional_tag
    );
    float neutral_decay = emotion_consolidation_modulate_decay(
        ec_system, base_decay, &neutral_tag
    );

    EXPECT_LT(emotional_decay, neutral_decay);  /* Emotional decays slower */
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
