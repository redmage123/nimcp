/**
 * @file test_emotion_memory_bridge.cpp
 * @brief Unit tests for Emotion-Memory Integration Bridge
 * @version 1.0.0
 * @date 2025-12-31
 */

#include <gtest/gtest.h>

#include "cognitive/integration/nimcp_emotion_memory_bridge.h"

/**
 * @brief Test fixture for Emotion-Memory bridge tests
 */
class EmotionMemoryBridgeTest : public ::testing::Test {
protected:
    emotion_memory_bridge_t* bridge;
    emotion_memory_config_t config;

    void SetUp() override {
        ASSERT_EQ(0, emotion_memory_bridge_default_config(&config));
        bridge = emotion_memory_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }

    void TearDown() override {
        if (bridge) {
            emotion_memory_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

/**
 * @brief Test bridge creation and destruction
 */
TEST_F(EmotionMemoryBridgeTest, BridgeCreation) {
    // Bridge already created in SetUp, verify it's valid
    EXPECT_NE(nullptr, bridge);

    // Create another bridge with NULL config (uses defaults)
    emotion_memory_bridge_t* bridge2 = emotion_memory_bridge_create(nullptr);
    EXPECT_NE(nullptr, bridge2);
    emotion_memory_bridge_destroy(bridge2);

    // Destroy NULL should be safe
    emotion_memory_bridge_destroy(nullptr);
}

/**
 * @brief Test default configuration values
 */
TEST_F(EmotionMemoryBridgeTest, DefaultConfig) {
    emotion_memory_config_t default_config;
    ASSERT_EQ(0, emotion_memory_bridge_default_config(&default_config));

    // Verify sensible defaults (emotional weight, consolidation threshold, valence sensitivity)
    EXPECT_GT(default_config.emotional_weight_factor, 0.0f);
    EXPECT_LE(default_config.emotional_weight_factor, 1.0f);
    EXPECT_GT(default_config.consolidation_threshold, 0.0f);
    EXPECT_LE(default_config.consolidation_threshold, 1.0f);
    EXPECT_GT(default_config.valence_sensitivity, 0.0f);
    EXPECT_LE(default_config.valence_sensitivity, 1.0f);

    // NULL config should return error
    EXPECT_EQ(-1, emotion_memory_bridge_default_config(nullptr));
}

/* ============================================================================
 * Emotion -> Memory Direction Tests
 * ============================================================================ */

/**
 * @brief Test tagging a memory with emotional valence and arousal
 */
TEST_F(EmotionMemoryBridgeTest, TagMemory) {
    uint64_t memory_id = 100;
    float valence = 0.8f;   // Positive emotion
    float arousal = 0.6f;   // Moderate arousal

    // Tag the memory
    EXPECT_EQ(0, emotion_memory_tag_memory(bridge, memory_id, valence, arousal));

    // Verify the tag by retrieving it
    emotion_memory_emotion_out_t emotion_out;
    EXPECT_EQ(0, emotion_memory_on_retrieval(bridge, memory_id, &emotion_out));
    EXPECT_TRUE(emotion_out.has_emotion);
    EXPECT_FLOAT_EQ(valence, emotion_out.valence);
    EXPECT_FLOAT_EQ(arousal, emotion_out.arousal);

    // Test with negative valence
    uint64_t memory_id2 = 101;
    EXPECT_EQ(0, emotion_memory_tag_memory(bridge, memory_id2, -0.5f, 0.9f));

    // NULL bridge should fail
    EXPECT_EQ(-1, emotion_memory_tag_memory(nullptr, memory_id, valence, arousal));
}

/* ============================================================================
 * Memory -> Emotion Direction Tests
 * ============================================================================ */

/**
 * @brief Test retrieving emotional tag from memory
 */
TEST_F(EmotionMemoryBridgeTest, OnRetrieval) {
    uint64_t memory_id = 200;
    float valence = -0.3f;  // Slightly negative
    float arousal = 0.7f;   // High arousal

    // Tag first
    EXPECT_EQ(0, emotion_memory_tag_memory(bridge, memory_id, valence, arousal));

    // Retrieve
    emotion_memory_emotion_out_t emotion_out;
    EXPECT_EQ(0, emotion_memory_on_retrieval(bridge, memory_id, &emotion_out));

    EXPECT_TRUE(emotion_out.has_emotion);
    EXPECT_FLOAT_EQ(valence, emotion_out.valence);
    EXPECT_FLOAT_EQ(arousal, emotion_out.arousal);
    EXPECT_GT(emotion_out.intensity, 0.0f);  // Intensity should be computed

    // NULL checks
    EXPECT_EQ(-1, emotion_memory_on_retrieval(nullptr, memory_id, &emotion_out));
    EXPECT_EQ(-1, emotion_memory_on_retrieval(bridge, memory_id, nullptr));
}

/**
 * @brief Test retrieval of untagged memory returns appropriate indication
 */
TEST_F(EmotionMemoryBridgeTest, RetrievalNotFound) {
    uint64_t untagged_memory_id = 999;

    emotion_memory_emotion_out_t emotion_out;
    // Retrieval of untagged memory should return -1 or indicate no emotion
    int result = emotion_memory_on_retrieval(bridge, untagged_memory_id, &emotion_out);

    // Either returns -1 OR returns 0 with has_emotion=false
    if (result == 0) {
        EXPECT_FALSE(emotion_out.has_emotion);
    } else {
        EXPECT_EQ(-1, result);
    }
}

/* ============================================================================
 * Consolidation Tests
 * ============================================================================ */

/**
 * @brief Test modulation of memory consolidation based on emotional intensity
 */
TEST_F(EmotionMemoryBridgeTest, ModulateConsolidation) {
    uint64_t memory_id = 300;
    float high_intensity = 0.9f;

    // Tag memory first
    EXPECT_EQ(0, emotion_memory_tag_memory(bridge, memory_id, 0.7f, 0.8f));

    // Modulate consolidation with high emotional intensity
    EXPECT_EQ(0, emotion_memory_modulate_consolidation(bridge, memory_id, high_intensity));

    // Check stats to verify consolidation boost was recorded
    emotion_memory_stats_t stats;
    EXPECT_EQ(0, emotion_memory_bridge_get_stats(bridge, &stats));
    EXPECT_GT(stats.consolidation_boosts, 0u);

    // NULL bridge should fail
    EXPECT_EQ(-1, emotion_memory_modulate_consolidation(nullptr, memory_id, high_intensity));
}

/* ============================================================================
 * Query Tests
 * ============================================================================ */

/**
 * @brief Test querying memories by valence range
 */
TEST_F(EmotionMemoryBridgeTest, GetEmotionalMemories) {
    // Tag several memories with different valences
    EXPECT_EQ(0, emotion_memory_tag_memory(bridge, 1, 0.9f, 0.5f));   // Very positive
    EXPECT_EQ(0, emotion_memory_tag_memory(bridge, 2, 0.5f, 0.5f));   // Moderately positive
    EXPECT_EQ(0, emotion_memory_tag_memory(bridge, 3, 0.1f, 0.5f));   // Slightly positive
    EXPECT_EQ(0, emotion_memory_tag_memory(bridge, 4, -0.3f, 0.5f));  // Slightly negative
    EXPECT_EQ(0, emotion_memory_tag_memory(bridge, 5, -0.8f, 0.5f));  // Very negative

    // Query positive memories (valence > 0.4)
    uint64_t memory_ids[10];
    int count = emotion_memory_get_emotional_memories(bridge, 0.4f, 1.0f, memory_ids, 10);
    EXPECT_GE(count, 2);  // Should find at least memories 1 and 2

    // Query negative memories (valence < -0.2)
    count = emotion_memory_get_emotional_memories(bridge, -1.0f, -0.2f, memory_ids, 10);
    EXPECT_GE(count, 2);  // Should find at least memories 4 and 5

    // Query all memories
    count = emotion_memory_get_emotional_memories(bridge, -1.0f, 1.0f, memory_ids, 10);
    EXPECT_EQ(5, count);

    // NULL checks
    EXPECT_EQ(-1, emotion_memory_get_emotional_memories(nullptr, 0.0f, 1.0f, memory_ids, 10));
    EXPECT_EQ(-1, emotion_memory_get_emotional_memories(bridge, 0.0f, 1.0f, nullptr, 10));
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

/**
 * @brief Test statistics tracking
 */
TEST_F(EmotionMemoryBridgeTest, StatsTracking) {
    emotion_memory_stats_t stats;

    // Get initial stats
    EXPECT_EQ(0, emotion_memory_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(0u, stats.memories_tagged);

    // Tag some memories
    EXPECT_EQ(0, emotion_memory_tag_memory(bridge, 1, 0.5f, 0.6f));
    EXPECT_EQ(0, emotion_memory_tag_memory(bridge, 2, -0.3f, 0.4f));
    EXPECT_EQ(0, emotion_memory_tag_memory(bridge, 3, 0.7f, 0.8f));

    // Check stats updated
    EXPECT_EQ(0, emotion_memory_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(3u, stats.memories_tagged);

    // Retrieve some memories
    emotion_memory_emotion_out_t emotion_out;
    EXPECT_EQ(0, emotion_memory_on_retrieval(bridge, 1, &emotion_out));
    EXPECT_EQ(0, emotion_memory_on_retrieval(bridge, 2, &emotion_out));

    // Check retrieval stats
    EXPECT_EQ(0, emotion_memory_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(2u, stats.retrievals_with_emotion);

    // Verify average calculations
    // Average valence: (0.5 + (-0.3) + 0.7) / 3 = 0.3
    float expected_avg_valence = (0.5f + (-0.3f) + 0.7f) / 3.0f;
    EXPECT_NEAR(expected_avg_valence, stats.avg_valence, 0.01f);

    // Average arousal: (0.6 + 0.4 + 0.8) / 3 = 0.6
    float expected_avg_arousal = (0.6f + 0.4f + 0.8f) / 3.0f;
    EXPECT_NEAR(expected_avg_arousal, stats.avg_arousal, 0.01f);

    // NULL checks
    EXPECT_EQ(-1, emotion_memory_bridge_get_stats(nullptr, &stats));
    EXPECT_EQ(-1, emotion_memory_bridge_get_stats(bridge, nullptr));
}
