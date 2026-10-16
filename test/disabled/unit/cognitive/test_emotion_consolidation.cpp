/**
 * @file test_emotion_consolidation.cpp
 * @brief Unit tests for emotion-modulated consolidation system
 *
 * WHAT: Comprehensive unit tests for emotion-consolidation integration
 * WHY:  Ensure emotion tensor correctly modulates memory consolidation
 * HOW:  Test all APIs, edge cases, and bio-async integration
 *
 * @author NIMCP Development Team
 * @date 2025-12-09
 */

#include <gtest/gtest.h>
#include "cognitive/consolidation/nimcp_emotion_consolidation.h"
#include "cognitive/nimcp_emotion_tensor.h"
#include "utils/time/nimcp_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EmotionConsolidationTest : public ::testing::Test {
protected:
    emotion_tensor_system_t* tensor_system;
    emotion_consolidation_system_t* ec_system;

    void SetUp() override {
        /* Create emotion tensor system */
        tensor_system = emotion_tensor_create(nullptr);
        ASSERT_NE(tensor_system, nullptr);

        /* Create emotion-consolidation system */
        ec_system = emotion_consolidation_create(tensor_system, nullptr, nullptr);
        ASSERT_NE(ec_system, nullptr);
    }

    void TearDown() override {
        emotion_consolidation_destroy(ec_system);
        emotion_tensor_destroy(tensor_system);
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(EmotionConsolidationTest, CreateWithDefaults) {
    /* WHAT: Test creation with default config */
    EXPECT_NE(ec_system, nullptr);

    /* WHAT: Verify initial boost is neutral */
    float boost = emotion_consolidation_get_boost(ec_system);
    EXPECT_NEAR(boost, 1.0f, 0.01f);  /* Should start neutral */
}

TEST_F(EmotionConsolidationTest, CreateWithCustomConfig) {
    /* WHAT: Create with custom config */
    emotion_consolidation_config_t config = emotion_consolidation_default_config();
    config.arousal_consolidation_boost = 3.0f;
    config.max_consolidation_boost = 5.0f;
    config.decay_inhibition_factor = 0.5f;

    emotion_consolidation_system_t* custom = emotion_consolidation_create(
        tensor_system, nullptr, &config
    );
    ASSERT_NE(custom, nullptr);

    emotion_consolidation_destroy(custom);
}

TEST_F(EmotionConsolidationTest, CreateWithNullTensor) {
    /* WHAT: Test null tensor pointer handling */
    EXPECT_EQ(emotion_consolidation_create(nullptr, nullptr, nullptr), nullptr);
}

TEST_F(EmotionConsolidationTest, DestroyNullSafe) {
    /* WHAT: Destroy should handle NULL safely */
    emotion_consolidation_destroy(nullptr);  /* Should not crash */
}

//=============================================================================
// Memory Tagging Tests
//=============================================================================

TEST_F(EmotionConsolidationTest, TagMemoryWithNeutralEmotion) {
    /* WHAT: Test tagging memory with neutral emotion */
    memory_emotion_tag_t tag;
    ASSERT_TRUE(emotion_consolidation_tag_memory(ec_system, &tag));

    /* Check tag values */
    EXPECT_GE(tag.arousal, 0.0f);
    EXPECT_LE(tag.arousal, 1.0f);
    EXPECT_GE(tag.valence, -1.0f);
    EXPECT_LE(tag.valence, 1.0f);
    EXPECT_FALSE(tag.is_emotionally_tagged);  /* Low arousal = not tagged */
}

TEST_F(EmotionConsolidationTest, TagMemoryWithHighArousal) {
    /* WHAT: Test tagging memory during high arousal */

    /* Set high fear */
    emotion_tensor_set_channel(tensor_system, TENSOR_FEAR, 0.8f, 0);

    memory_emotion_tag_t tag;
    ASSERT_TRUE(emotion_consolidation_tag_memory(ec_system, &tag));

    /* Check tag marked as emotional */
    EXPECT_GT(tag.arousal, 0.5f);
    EXPECT_TRUE(tag.is_emotionally_tagged);
    EXPECT_EQ(tag.dominant_emotion, TENSOR_FEAR);
}

TEST_F(EmotionConsolidationTest, TagMemoryTimestamp) {
    /* WHAT: Test tag includes timestamp */
    uint64_t before = nimcp_time_monotonic_ms();

    memory_emotion_tag_t tag;
    ASSERT_TRUE(emotion_consolidation_tag_memory(ec_system, &tag));

    uint64_t after = nimcp_time_monotonic_ms();

    EXPECT_GE(tag.encoding_timestamp_ms, before);
    EXPECT_LE(tag.encoding_timestamp_ms, after);
}

TEST_F(EmotionConsolidationTest, TagMemoryWithNullInputs) {
    /* WHAT: Test null pointer handling */
    memory_emotion_tag_t tag;

    EXPECT_FALSE(emotion_consolidation_tag_memory(nullptr, &tag));
    EXPECT_FALSE(emotion_consolidation_tag_memory(ec_system, nullptr));
}

//=============================================================================
// Consolidation Strength Tests
//=============================================================================

TEST_F(EmotionConsolidationTest, HighArousalBoostsConsolidation) {
    /* WHAT: Test high arousal strengthens consolidation */

    /* Create emotional tag with high arousal */
    emotion_tensor_set_channel(tensor_system, TENSOR_FEAR, 0.9f, 0);

    memory_emotion_tag_t tag;
    ASSERT_TRUE(emotion_consolidation_tag_memory(ec_system, &tag));

    /* Compute strength */
    float base_strength = 0.3f;
    float boosted = emotion_consolidation_compute_strength(ec_system, base_strength, &tag);

    EXPECT_GT(boosted, base_strength);  /* Should be boosted */
    EXPECT_LE(boosted, 1.0f);  /* Should not exceed 1.0 */
}

TEST_F(EmotionConsolidationTest, NeutralEmotionNoBoost) {
    /* WHAT: Test neutral emotion doesn't boost consolidation */

    /* Reset to neutral */
    emotion_tensor_reset(tensor_system);

    memory_emotion_tag_t tag;
    ASSERT_TRUE(emotion_consolidation_tag_memory(ec_system, &tag));

    float base_strength = 0.4f;
    float result = emotion_consolidation_compute_strength(ec_system, base_strength, &tag);

    EXPECT_NEAR(result, base_strength, 0.01f);  /* Should be unchanged */
}

TEST_F(EmotionConsolidationTest, NegativeValenceSlightBoost) {
    /* WHAT: Test negative valence gives small additional boost */

    /* Set sadness (negative valence) */
    emotion_tensor_set_channel(tensor_system, TENSOR_SADNESS, 0.7f, 0);

    memory_emotion_tag_t tag;
    ASSERT_TRUE(emotion_consolidation_tag_memory(ec_system, &tag));

    float base_strength = 0.3f;
    float boosted = emotion_consolidation_compute_strength(ec_system, base_strength, &tag);

    EXPECT_GT(boosted, base_strength);  /* Should get some boost */
}

TEST_F(EmotionConsolidationTest, ConsolidationClampsAtMax) {
    /* WHAT: Test consolidation doesn't exceed max boost */

    /* Create config with low max boost */
    emotion_consolidation_config_t config = emotion_consolidation_default_config();
    config.max_consolidation_boost = 2.0f;

    emotion_consolidation_system_t* limited = emotion_consolidation_create(
        tensor_system, nullptr, &config
    );

    /* Set extreme arousal */
    emotion_tensor_set_channel(tensor_system, TENSOR_FEAR, 1.0f, 0);
    emotion_tensor_set_channel(tensor_system, TENSOR_ANGER, 1.0f, 0);

    memory_emotion_tag_t tag;
    ASSERT_TRUE(emotion_consolidation_tag_memory(limited, &tag));

    float result = emotion_consolidation_compute_strength(limited, 0.6f, &tag);

    /* Should not exceed 1.0 even with high boost */
    EXPECT_LE(result, 1.0f);

    emotion_consolidation_destroy(limited);
}

TEST_F(EmotionConsolidationTest, ComputeStrengthClampsInput) {
    /* WHAT: Test strength computation clamps input/output */

    memory_emotion_tag_t tag;
    tag.is_emotionally_tagged = false;

    /* Test negative input */
    float result1 = emotion_consolidation_compute_strength(ec_system, -0.5f, &tag);
    EXPECT_GE(result1, 0.0f);

    /* Test >1.0 input */
    float result2 = emotion_consolidation_compute_strength(ec_system, 1.5f, &tag);
    EXPECT_LE(result2, 1.0f);
}

//=============================================================================
// Memory Prioritization Tests
//=============================================================================

TEST_F(EmotionConsolidationTest, PrioritizeEmotionalMemories) {
    /* WHAT: Test emotional memories are prioritized */

    /* High arousal memory */
    emotion_tensor_set_channel(tensor_system, TENSOR_ANGER, 0.8f, 0);

    memory_emotion_tag_t emotional_tag;
    ASSERT_TRUE(emotion_consolidation_tag_memory(ec_system, &emotional_tag));

    EXPECT_TRUE(emotion_consolidation_should_prioritize(ec_system, &emotional_tag));

    /* Neutral memory */
    emotion_tensor_reset(tensor_system);

    memory_emotion_tag_t neutral_tag;
    ASSERT_TRUE(emotion_consolidation_tag_memory(ec_system, &neutral_tag));

    EXPECT_FALSE(emotion_consolidation_should_prioritize(ec_system, &neutral_tag));
}

TEST_F(EmotionConsolidationTest, PrioritizationDisabled) {
    /* WHAT: Test prioritization can be disabled */

    emotion_consolidation_config_t config = emotion_consolidation_default_config();
    config.prioritize_emotional = false;

    emotion_consolidation_system_t* no_priority = emotion_consolidation_create(
        tensor_system, nullptr, &config
    );

    emotion_tensor_set_channel(tensor_system, TENSOR_FEAR, 0.9f, 0);

    memory_emotion_tag_t tag;
    ASSERT_TRUE(emotion_consolidation_tag_memory(no_priority, &tag));

    EXPECT_FALSE(emotion_consolidation_should_prioritize(no_priority, &tag));

    emotion_consolidation_destroy(no_priority);
}

//=============================================================================
// Decay Modulation Tests
//=============================================================================

TEST_F(EmotionConsolidationTest, EmotionalMemoriesDecaySlower) {
    /* WHAT: Test emotional memories resist decay */

    /* Create emotional tag */
    emotion_tensor_set_channel(tensor_system, TENSOR_SADNESS, 0.8f, 0);

    memory_emotion_tag_t emotional_tag;
    ASSERT_TRUE(emotion_consolidation_tag_memory(ec_system, &emotional_tag));

    float base_decay = 0.5f;
    float modulated = emotion_consolidation_modulate_decay(
        ec_system, base_decay, &emotional_tag
    );

    EXPECT_LT(modulated, base_decay);  /* Should decay slower */
    EXPECT_GE(modulated, 0.0f);
}

TEST_F(EmotionConsolidationTest, NeutralMemoriesDecayNormally) {
    /* WHAT: Test neutral memories decay normally */

    emotion_tensor_reset(tensor_system);

    memory_emotion_tag_t neutral_tag;
    ASSERT_TRUE(emotion_consolidation_tag_memory(ec_system, &neutral_tag));

    float base_decay = 0.4f;
    float modulated = emotion_consolidation_modulate_decay(
        ec_system, base_decay, &neutral_tag
    );

    EXPECT_NEAR(modulated, base_decay, 0.01f);  /* Should be unchanged */
}

TEST_F(EmotionConsolidationTest, DecayClampsToZero) {
    /* WHAT: Test decay doesn't go negative */

    emotion_tensor_set_channel(tensor_system, TENSOR_JOY, 0.9f, 0);

    memory_emotion_tag_t tag;
    ASSERT_TRUE(emotion_consolidation_tag_memory(ec_system, &tag));

    float result = emotion_consolidation_modulate_decay(ec_system, -0.2f, &tag);
    EXPECT_GE(result, 0.0f);
}

//=============================================================================
// Boost Factor Tests
//=============================================================================

TEST_F(EmotionConsolidationTest, BoostFactorUpdatesWithEmotion) {
    /* WHAT: Test boost factor reflects current emotion */

    /* Initially neutral */
    float initial_boost = emotion_consolidation_get_boost(ec_system);
    EXPECT_NEAR(initial_boost, 1.0f, 0.1f);

    /* Set high arousal emotion */
    emotion_tensor_set_channel(tensor_system, TENSOR_ANGER, 0.9f, 0);

    /* NOTE: Boost won't update until bio-async message is processed */
    /* For unit test, we can check that boost formula works via compute_strength */
    memory_emotion_tag_t tag;
    ASSERT_TRUE(emotion_consolidation_tag_memory(ec_system, &tag));

    float strength = emotion_consolidation_compute_strength(ec_system, 0.5f, &tag);
    EXPECT_GT(strength, 0.5f);  /* Should be boosted */
}

TEST_F(EmotionConsolidationTest, BoostWithNullSystem) {
    /* WHAT: Test boost query with NULL */
    float boost = emotion_consolidation_get_boost(nullptr);
    EXPECT_NEAR(boost, 1.0f, 0.01f);  /* Should return neutral */
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(EmotionConsolidationTest, StatisticsInitiallyZero) {
    /* WHAT: Test statistics start at zero */
    emotion_consolidation_stats_t stats;
    ASSERT_TRUE(emotion_consolidation_get_stats(ec_system, &stats));

    EXPECT_EQ(stats.emotion_updates_received, 0);
    EXPECT_EQ(stats.emotional_memories_tagged, 0);
    EXPECT_EQ(stats.emotional_boosts_applied, 0);
    EXPECT_NEAR(stats.current_consolidation_boost, 1.0f, 0.01f);
}

TEST_F(EmotionConsolidationTest, StatisticsTrackTagging) {
    /* WHAT: Test statistics track memory tagging */

    /* Tag some emotional memories */
    emotion_tensor_set_channel(tensor_system, TENSOR_FEAR, 0.7f, 0);

    for (int i = 0; i < 5; i++) {
        memory_emotion_tag_t tag;
        ASSERT_TRUE(emotion_consolidation_tag_memory(ec_system, &tag));
    }

    emotion_consolidation_stats_t stats;
    ASSERT_TRUE(emotion_consolidation_get_stats(ec_system, &stats));

    EXPECT_EQ(stats.emotional_memories_tagged, 5);
}

TEST_F(EmotionConsolidationTest, StatisticsTrackBoosts) {
    /* WHAT: Test statistics track consolidation boosts */

    emotion_tensor_set_channel(tensor_system, TENSOR_ANGER, 0.8f, 0);

    memory_emotion_tag_t tag;
    ASSERT_TRUE(emotion_consolidation_tag_memory(ec_system, &tag));

    /* Apply boosts */
    for (int i = 0; i < 3; i++) {
        emotion_consolidation_compute_strength(ec_system, 0.5f, &tag);
    }

    emotion_consolidation_stats_t stats;
    ASSERT_TRUE(emotion_consolidation_get_stats(ec_system, &stats));

    EXPECT_EQ(stats.emotional_boosts_applied, 3);
}

TEST_F(EmotionConsolidationTest, StatisticsReset) {
    /* WHAT: Test statistics can be reset */

    /* Accumulate some stats */
    emotion_tensor_set_channel(tensor_system, TENSOR_JOY, 0.7f, 0);

    memory_emotion_tag_t tag;
    emotion_consolidation_tag_memory(ec_system, &tag);
    emotion_consolidation_compute_strength(ec_system, 0.5f, &tag);

    /* Reset */
    emotion_consolidation_reset_stats(ec_system);

    /* Check stats cleared */
    emotion_consolidation_stats_t stats;
    ASSERT_TRUE(emotion_consolidation_get_stats(ec_system, &stats));

    EXPECT_EQ(stats.emotion_updates_received, 0);
    EXPECT_EQ(stats.emotional_memories_tagged, 0);
    EXPECT_EQ(stats.emotional_boosts_applied, 0);
}

TEST_F(EmotionConsolidationTest, GetStatsWithNullInputs) {
    /* WHAT: Test null pointer handling */
    emotion_consolidation_stats_t stats;

    EXPECT_FALSE(emotion_consolidation_get_stats(nullptr, &stats));
    EXPECT_FALSE(emotion_consolidation_get_stats(ec_system, nullptr));
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(EmotionConsolidationTest, FullEmotionalConsolidationCycle) {
    /* WHAT: Test complete cycle: encoding → tagging → consolidation → decay */

    /* Phase 1: Encoding during high arousal */
    emotion_tensor_set_channel(tensor_system, TENSOR_FEAR, 0.9f, 0);

    memory_emotion_tag_t tag;
    ASSERT_TRUE(emotion_consolidation_tag_memory(ec_system, &tag));
    EXPECT_TRUE(tag.is_emotionally_tagged);

    /* Phase 2: Consolidation with emotional boost */
    float consolidation = emotion_consolidation_compute_strength(ec_system, 0.4f, &tag);
    EXPECT_GT(consolidation, 0.4f);
    EXPECT_LE(consolidation, 1.0f);

    /* Phase 3: Check prioritization */
    EXPECT_TRUE(emotion_consolidation_should_prioritize(ec_system, &tag));

    /* Phase 4: Decay inhibition */
    float decay = emotion_consolidation_modulate_decay(ec_system, 0.5f, &tag);
    EXPECT_LT(decay, 0.5f);  /* Slower decay for emotional memory */
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
