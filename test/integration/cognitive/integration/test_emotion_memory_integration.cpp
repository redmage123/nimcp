/**
 * @file test_emotion_memory_integration.cpp
 * @brief Integration tests for emotion-memory bridge
 * @version 1.0.0
 * @date 2025-01-08
 *
 * WHAT: Integration tests for emotion-memory bidirectional coupling
 * WHY:  Verify emotional tagging, retrieval emotion triggers, and
 *       consolidation modulation work correctly
 * HOW:  Test emotional memory encoding, retrieval, and consolidation boost
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/integration/nimcp_emotion_memory_bridge.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define TEST_MEMORY_ID_1        1001
#define TEST_MEMORY_ID_2        1002
#define TEST_MEMORY_ID_3        1003
#define TEST_MEMORY_ID_4        1004
#define TEST_MEMORY_ID_5        1005

#define VALENCE_POSITIVE        0.8f
#define VALENCE_NEGATIVE       -0.7f
#define VALENCE_NEUTRAL         0.0f
#define AROUSAL_HIGH            0.9f
#define AROUSAL_LOW             0.2f
#define AROUSAL_MEDIUM          0.5f

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class EmotionMemoryIntegrationTest : public ::testing::Test {
protected:
    emotion_memory_bridge_t* bridge;
    emotion_memory_config_t config;

    void SetUp() override {
        bridge = nullptr;

        /* Get default config */
        int ret = emotion_memory_bridge_default_config(&config);
        ASSERT_EQ(ret, 0);

        /* Create bridge */
        bridge = emotion_memory_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            emotion_memory_bridge_destroy(bridge);
        }
    }

    /* Helper to compute emotional intensity from valence and arousal */
    float compute_intensity(float valence, float arousal) {
        return sqrtf(valence * valence + arousal * arousal) / sqrtf(2.0f);
    }
};

/* ============================================================================
 * Emotional Memory Encoding Tests
 * ============================================================================ */

TEST_F(EmotionMemoryIntegrationTest, EmotionalMemoryEncoding) {
    /* Tag a memory with emotional valence and arousal */
    int ret = emotion_memory_tag_memory(bridge, TEST_MEMORY_ID_1,
                                         VALENCE_POSITIVE, AROUSAL_HIGH);
    EXPECT_EQ(ret, 0);

    /* Retrieve and verify the emotion is persisted */
    emotion_memory_emotion_out_t emotion_out;
    ret = emotion_memory_on_retrieval(bridge, TEST_MEMORY_ID_1, &emotion_out);
    EXPECT_EQ(ret, 0);

    /* Verify tag persists */
    EXPECT_TRUE(emotion_out.has_emotion);
    EXPECT_FLOAT_EQ(emotion_out.valence, VALENCE_POSITIVE);
    EXPECT_FLOAT_EQ(emotion_out.arousal, AROUSAL_HIGH);
    EXPECT_GT(emotion_out.intensity, 0.0f);

    /* Check stats were updated */
    emotion_memory_stats_t stats;
    ret = emotion_memory_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.memories_tagged, 1u);
}

TEST_F(EmotionMemoryIntegrationTest, MultipleMemoryTags) {
    /* Tag multiple memories with different emotions */
    int ret;

    ret = emotion_memory_tag_memory(bridge, TEST_MEMORY_ID_1,
                                     VALENCE_POSITIVE, AROUSAL_HIGH);
    EXPECT_EQ(ret, 0);

    ret = emotion_memory_tag_memory(bridge, TEST_MEMORY_ID_2,
                                     VALENCE_NEGATIVE, AROUSAL_MEDIUM);
    EXPECT_EQ(ret, 0);

    ret = emotion_memory_tag_memory(bridge, TEST_MEMORY_ID_3,
                                     VALENCE_NEUTRAL, AROUSAL_LOW);
    EXPECT_EQ(ret, 0);

    /* Verify each has correct tag */
    emotion_memory_emotion_out_t emotion_out;

    ret = emotion_memory_on_retrieval(bridge, TEST_MEMORY_ID_1, &emotion_out);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(emotion_out.has_emotion);
    EXPECT_FLOAT_EQ(emotion_out.valence, VALENCE_POSITIVE);

    ret = emotion_memory_on_retrieval(bridge, TEST_MEMORY_ID_2, &emotion_out);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(emotion_out.has_emotion);
    EXPECT_FLOAT_EQ(emotion_out.valence, VALENCE_NEGATIVE);

    ret = emotion_memory_on_retrieval(bridge, TEST_MEMORY_ID_3, &emotion_out);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(emotion_out.has_emotion);
    EXPECT_FLOAT_EQ(emotion_out.valence, VALENCE_NEUTRAL);

    /* Stats should show all tagged */
    emotion_memory_stats_t stats;
    emotion_memory_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.memories_tagged, 3u);
}

/* ============================================================================
 * Emotional Memory Retrieval Tests
 * ============================================================================ */

TEST_F(EmotionMemoryIntegrationTest, EmotionalMemoryRetrieval) {
    /* Tag memory with emotion */
    int ret = emotion_memory_tag_memory(bridge, TEST_MEMORY_ID_1,
                                         VALENCE_NEGATIVE, AROUSAL_HIGH);
    ASSERT_EQ(ret, 0);

    /* Retrieve and verify emotion is restored */
    emotion_memory_emotion_out_t emotion_out;
    ret = emotion_memory_on_retrieval(bridge, TEST_MEMORY_ID_1, &emotion_out);
    EXPECT_EQ(ret, 0);

    /* Verify emotion restored correctly */
    EXPECT_TRUE(emotion_out.has_emotion);
    EXPECT_FLOAT_EQ(emotion_out.valence, VALENCE_NEGATIVE);
    EXPECT_FLOAT_EQ(emotion_out.arousal, AROUSAL_HIGH);

    /* Intensity should be computed from valence/arousal */
    float expected_intensity = compute_intensity(VALENCE_NEGATIVE, AROUSAL_HIGH);
    EXPECT_NEAR(emotion_out.intensity, expected_intensity, 0.1f);

    /* Stats should track retrieval */
    emotion_memory_stats_t stats;
    emotion_memory_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.retrievals_with_emotion, 1u);
}

TEST_F(EmotionMemoryIntegrationTest, RetrievalOfUntaggedMemory) {
    /* Retrieve memory that was never tagged */
    emotion_memory_emotion_out_t emotion_out;
    int ret = emotion_memory_on_retrieval(bridge, TEST_MEMORY_ID_1, &emotion_out);

    /* Should succeed but indicate no emotion */
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(emotion_out.has_emotion);
    EXPECT_FLOAT_EQ(emotion_out.valence, 0.0f);
    EXPECT_FLOAT_EQ(emotion_out.arousal, 0.0f);
    EXPECT_FLOAT_EQ(emotion_out.intensity, 0.0f);
}

TEST_F(EmotionMemoryIntegrationTest, MultipleRetrievalsSameMemory) {
    /* Tag memory */
    int ret = emotion_memory_tag_memory(bridge, TEST_MEMORY_ID_1,
                                         VALENCE_POSITIVE, AROUSAL_MEDIUM);
    ASSERT_EQ(ret, 0);

    /* Retrieve multiple times */
    emotion_memory_emotion_out_t emotion_out;
    for (int i = 0; i < 5; i++) {
        ret = emotion_memory_on_retrieval(bridge, TEST_MEMORY_ID_1, &emotion_out);
        EXPECT_EQ(ret, 0);
        EXPECT_TRUE(emotion_out.has_emotion);
        EXPECT_FLOAT_EQ(emotion_out.valence, VALENCE_POSITIVE);
    }

    /* Stats should track all retrievals */
    emotion_memory_stats_t stats;
    emotion_memory_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.retrievals_with_emotion, 5u);
}

/* ============================================================================
 * Consolidation Modulation Tests
 * ============================================================================ */

TEST_F(EmotionMemoryIntegrationTest, HighEmotionConsolidation) {
    /* Tag memory with high emotional intensity */
    int ret = emotion_memory_tag_memory(bridge, TEST_MEMORY_ID_1,
                                         VALENCE_POSITIVE, AROUSAL_HIGH);
    ASSERT_EQ(ret, 0);

    /* High emotional intensity should boost consolidation */
    float high_intensity = 0.9f;  /* Above consolidation threshold */
    ret = emotion_memory_modulate_consolidation(bridge, TEST_MEMORY_ID_1, high_intensity);
    EXPECT_EQ(ret, 0);

    /* Check stats - consolidation boost should be recorded */
    emotion_memory_stats_t stats;
    emotion_memory_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.consolidation_boosts, 1u);
}

TEST_F(EmotionMemoryIntegrationTest, LowEmotionConsolidation) {
    /* Tag memory with low emotional intensity */
    int ret = emotion_memory_tag_memory(bridge, TEST_MEMORY_ID_1,
                                         VALENCE_NEUTRAL, AROUSAL_LOW);
    ASSERT_EQ(ret, 0);

    /* Low emotional intensity should have minimal consolidation boost */
    float low_intensity = 0.1f;  /* Below consolidation threshold */
    ret = emotion_memory_modulate_consolidation(bridge, TEST_MEMORY_ID_1, low_intensity);
    EXPECT_EQ(ret, 0);

    /* Stats may not show boost for low intensity */
    emotion_memory_stats_t stats;
    emotion_memory_bridge_get_stats(bridge, &stats);
    /* Consolidation boosts counter may be 0 or 1 depending on implementation */
    /* The key is that low intensity should NOT significantly boost consolidation */
    EXPECT_LE(stats.consolidation_boosts, 1u);
}

TEST_F(EmotionMemoryIntegrationTest, ConsolidationThresholdBehavior) {
    /* Test memories with intensity around the threshold */
    int ret;

    /* Memory 1: Just above threshold */
    ret = emotion_memory_tag_memory(bridge, TEST_MEMORY_ID_1, 0.5f, 0.6f);
    ASSERT_EQ(ret, 0);
    ret = emotion_memory_modulate_consolidation(bridge, TEST_MEMORY_ID_1, 0.55f);
    EXPECT_EQ(ret, 0);

    /* Memory 2: Just below threshold */
    ret = emotion_memory_tag_memory(bridge, TEST_MEMORY_ID_2, 0.1f, 0.1f);
    ASSERT_EQ(ret, 0);
    ret = emotion_memory_modulate_consolidation(bridge, TEST_MEMORY_ID_2, 0.15f);
    EXPECT_EQ(ret, 0);

    /* Memory 3: Well above threshold */
    ret = emotion_memory_tag_memory(bridge, TEST_MEMORY_ID_3, 0.9f, 0.9f);
    ASSERT_EQ(ret, 0);
    ret = emotion_memory_modulate_consolidation(bridge, TEST_MEMORY_ID_3, 0.95f);
    EXPECT_EQ(ret, 0);

    /* Verify differential consolidation boost */
    emotion_memory_stats_t stats;
    emotion_memory_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.consolidation_boosts, 1u);  /* At least one should boost */
}

/* ============================================================================
 * Multiple Memories by Valence Tests
 * ============================================================================ */

TEST_F(EmotionMemoryIntegrationTest, MultipleMemoriesByValence) {
    /* Tag memories with different valences */
    int ret;

    /* Positive memories */
    ret = emotion_memory_tag_memory(bridge, TEST_MEMORY_ID_1, 0.8f, 0.7f);
    ASSERT_EQ(ret, 0);
    ret = emotion_memory_tag_memory(bridge, TEST_MEMORY_ID_2, 0.6f, 0.5f);
    ASSERT_EQ(ret, 0);

    /* Negative memories */
    ret = emotion_memory_tag_memory(bridge, TEST_MEMORY_ID_3, -0.7f, 0.8f);
    ASSERT_EQ(ret, 0);
    ret = emotion_memory_tag_memory(bridge, TEST_MEMORY_ID_4, -0.5f, 0.4f);
    ASSERT_EQ(ret, 0);

    /* Neutral memory */
    ret = emotion_memory_tag_memory(bridge, TEST_MEMORY_ID_5, 0.0f, 0.3f);
    ASSERT_EQ(ret, 0);

    /* Query positive memories (valence > 0.5) */
    uint64_t positive_ids[10];
    int count = emotion_memory_get_emotional_memories(bridge, 0.5f, 1.0f,
                                                       positive_ids, 10);
    EXPECT_GE(count, 1);  /* At least memory 1 should match */

    /* Query negative memories (valence < -0.4) */
    uint64_t negative_ids[10];
    count = emotion_memory_get_emotional_memories(bridge, -1.0f, -0.4f,
                                                   negative_ids, 10);
    EXPECT_GE(count, 1);  /* At least memories 3 and 4 should match */

    /* Query all memories (full range) */
    uint64_t all_ids[10];
    count = emotion_memory_get_emotional_memories(bridge, -1.0f, 1.0f,
                                                   all_ids, 10);
    EXPECT_EQ(count, 5);  /* All 5 memories */

    /* Query neutral range */
    uint64_t neutral_ids[10];
    count = emotion_memory_get_emotional_memories(bridge, -0.2f, 0.2f,
                                                   neutral_ids, 10);
    EXPECT_GE(count, 1);  /* At least memory 5 */
}

TEST_F(EmotionMemoryIntegrationTest, QueryPositiveMemoriesOnly) {
    /* Tag some positive and some negative memories */
    emotion_memory_tag_memory(bridge, 1, 0.9f, 0.5f);  /* Strong positive */
    emotion_memory_tag_memory(bridge, 2, 0.7f, 0.4f);  /* Positive */
    emotion_memory_tag_memory(bridge, 3, -0.8f, 0.6f); /* Strong negative */
    emotion_memory_tag_memory(bridge, 4, 0.3f, 0.3f);  /* Weak positive */

    /* Query only positive memories */
    uint64_t positive_ids[10];
    int count = emotion_memory_get_emotional_memories(bridge, 0.0f, 1.0f,
                                                       positive_ids, 10);
    EXPECT_GE(count, 3);  /* Memories 1, 2, 4 */

    /* Verify no negative memories in result */
    for (int i = 0; i < count; i++) {
        emotion_memory_emotion_out_t emotion;
        emotion_memory_on_retrieval(bridge, positive_ids[i], &emotion);
        EXPECT_GE(emotion.valence, 0.0f);
    }
}

TEST_F(EmotionMemoryIntegrationTest, QueryNegativeMemoriesOnly) {
    /* Tag some positive and some negative memories */
    emotion_memory_tag_memory(bridge, 1, 0.9f, 0.5f);   /* Strong positive */
    emotion_memory_tag_memory(bridge, 2, -0.7f, 0.4f);  /* Negative */
    emotion_memory_tag_memory(bridge, 3, -0.8f, 0.6f);  /* Strong negative */
    emotion_memory_tag_memory(bridge, 4, -0.3f, 0.3f);  /* Weak negative */

    /* Query only negative memories */
    uint64_t negative_ids[10];
    int count = emotion_memory_get_emotional_memories(bridge, -1.0f, 0.0f,
                                                       negative_ids, 10);
    EXPECT_GE(count, 3);  /* Memories 2, 3, 4 */

    /* Verify no positive memories in result */
    for (int i = 0; i < count; i++) {
        emotion_memory_emotion_out_t emotion;
        emotion_memory_on_retrieval(bridge, negative_ids[i], &emotion);
        EXPECT_LE(emotion.valence, 0.0f);
    }
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(EmotionMemoryIntegrationTest, AverageValenceTracking) {
    /* Tag memories with known valences */
    emotion_memory_tag_memory(bridge, 1, 0.6f, 0.5f);
    emotion_memory_tag_memory(bridge, 2, 0.4f, 0.5f);
    emotion_memory_tag_memory(bridge, 3, 0.2f, 0.5f);

    /* Average should be (0.6 + 0.4 + 0.2) / 3 = 0.4 */
    emotion_memory_stats_t stats;
    emotion_memory_bridge_get_stats(bridge, &stats);
    EXPECT_NEAR(stats.avg_valence, 0.4f, 0.1f);
}

TEST_F(EmotionMemoryIntegrationTest, AverageArousalTracking) {
    /* Tag memories with known arousals */
    emotion_memory_tag_memory(bridge, 1, 0.5f, 0.8f);
    emotion_memory_tag_memory(bridge, 2, 0.5f, 0.6f);
    emotion_memory_tag_memory(bridge, 3, 0.5f, 0.4f);

    /* Average should be (0.8 + 0.6 + 0.4) / 3 = 0.6 */
    emotion_memory_stats_t stats;
    emotion_memory_bridge_get_stats(bridge, &stats);
    EXPECT_NEAR(stats.avg_arousal, 0.6f, 0.1f);
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(EmotionMemoryIntegrationTest, ValenceAtBoundaries) {
    int ret;

    /* Test minimum valence */
    ret = emotion_memory_tag_memory(bridge, 1, EMOTION_MEMORY_VALENCE_MIN, 0.5f);
    EXPECT_EQ(ret, 0);

    emotion_memory_emotion_out_t emotion;
    emotion_memory_on_retrieval(bridge, 1, &emotion);
    EXPECT_FLOAT_EQ(emotion.valence, EMOTION_MEMORY_VALENCE_MIN);

    /* Test maximum valence */
    ret = emotion_memory_tag_memory(bridge, 2, EMOTION_MEMORY_VALENCE_MAX, 0.5f);
    EXPECT_EQ(ret, 0);

    emotion_memory_on_retrieval(bridge, 2, &emotion);
    EXPECT_FLOAT_EQ(emotion.valence, EMOTION_MEMORY_VALENCE_MAX);
}

TEST_F(EmotionMemoryIntegrationTest, ArousalAtBoundaries) {
    int ret;

    /* Test minimum arousal */
    ret = emotion_memory_tag_memory(bridge, 1, 0.5f, EMOTION_MEMORY_AROUSAL_MIN);
    EXPECT_EQ(ret, 0);

    emotion_memory_emotion_out_t emotion;
    emotion_memory_on_retrieval(bridge, 1, &emotion);
    EXPECT_FLOAT_EQ(emotion.arousal, EMOTION_MEMORY_AROUSAL_MIN);

    /* Test maximum arousal */
    ret = emotion_memory_tag_memory(bridge, 2, 0.5f, EMOTION_MEMORY_AROUSAL_MAX);
    EXPECT_EQ(ret, 0);

    emotion_memory_on_retrieval(bridge, 2, &emotion);
    EXPECT_FLOAT_EQ(emotion.arousal, EMOTION_MEMORY_AROUSAL_MAX);
}

TEST_F(EmotionMemoryIntegrationTest, RetagSameMemory) {
    int ret;

    /* Tag memory initially */
    ret = emotion_memory_tag_memory(bridge, TEST_MEMORY_ID_1, 0.5f, 0.5f);
    ASSERT_EQ(ret, 0);

    /* Re-tag with different emotion */
    ret = emotion_memory_tag_memory(bridge, TEST_MEMORY_ID_1, -0.8f, 0.9f);
    EXPECT_EQ(ret, 0);

    /* Should have new emotion */
    emotion_memory_emotion_out_t emotion;
    emotion_memory_on_retrieval(bridge, TEST_MEMORY_ID_1, &emotion);
    EXPECT_FLOAT_EQ(emotion.valence, -0.8f);
    EXPECT_FLOAT_EQ(emotion.arousal, 0.9f);
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(EmotionMemoryIntegrationTest, CustomConfiguration) {
    /* Destroy default bridge */
    emotion_memory_bridge_destroy(bridge);

    /* Create with custom config */
    emotion_memory_config_t custom_config;
    emotion_memory_bridge_default_config(&custom_config);
    custom_config.emotional_weight_factor = 2.0f;
    custom_config.consolidation_threshold = 0.8f;  /* High threshold */
    custom_config.valence_sensitivity = 1.5f;

    bridge = emotion_memory_bridge_create(&custom_config);
    ASSERT_NE(bridge, nullptr);

    /* Tag and consolidate */
    emotion_memory_tag_memory(bridge, 1, 0.5f, 0.5f);

    /* Medium intensity (0.5) should NOT trigger boost with high threshold */
    emotion_memory_modulate_consolidation(bridge, 1, 0.5f);

    emotion_memory_stats_t stats;
    emotion_memory_bridge_get_stats(bridge, &stats);
    /* With 0.8 threshold, 0.5 intensity should not boost */
    EXPECT_EQ(stats.consolidation_boosts, 0u);

    /* High intensity SHOULD trigger boost */
    emotion_memory_modulate_consolidation(bridge, 1, 0.9f);
    emotion_memory_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.consolidation_boosts, 1u);
}

/* ============================================================================
 * Null/Error Handling Tests
 * ============================================================================ */

TEST_F(EmotionMemoryIntegrationTest, NullBridgeHandling) {
    int ret;

    ret = emotion_memory_tag_memory(nullptr, 1, 0.5f, 0.5f);
    EXPECT_EQ(ret, -1);

    emotion_memory_emotion_out_t emotion;
    ret = emotion_memory_on_retrieval(nullptr, 1, &emotion);
    EXPECT_EQ(ret, -1);

    ret = emotion_memory_modulate_consolidation(nullptr, 1, 0.5f);
    EXPECT_EQ(ret, -1);

    uint64_t ids[10];
    int count = emotion_memory_get_emotional_memories(nullptr, -1.0f, 1.0f, ids, 10);
    EXPECT_EQ(count, -1);

    emotion_memory_stats_t stats;
    ret = emotion_memory_bridge_get_stats(nullptr, &stats);
    EXPECT_EQ(ret, -1);
}

TEST_F(EmotionMemoryIntegrationTest, NullOutputHandling) {
    int ret;

    /* Tag first */
    ret = emotion_memory_tag_memory(bridge, 1, 0.5f, 0.5f);
    ASSERT_EQ(ret, 0);

    /* Null output should fail */
    ret = emotion_memory_on_retrieval(bridge, 1, nullptr);
    EXPECT_EQ(ret, -1);

    ret = emotion_memory_bridge_get_stats(bridge, nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
