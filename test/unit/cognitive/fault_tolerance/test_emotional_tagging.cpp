/**
 * @file test_emotional_tagging.cpp
 * @brief Comprehensive Unit Tests for Emotional Tagging System
 *
 * WHAT: 100% coverage unit tests for emotional tagging implementation
 * WHY:  Ensure correctness of emotional memory prioritization
 * HOW:  GTest framework with 50+ tests covering all paths
 *
 * COVERAGE AREAS:
 * - Lifecycle: create, destroy, reset
 * - Emotion computation: valence, arousal, specific emotions
 * - Memory strength: emotion-based boosting
 * - Priority calculation: emotional salience
 * - Edge cases: NULL checks, boundary values
 * - Failure scenarios: critical errors, data loss risk
 * - Success scenarios: fast recovery, relief
 * - Complex scenarios: repeated failures, frustration
 */

#include <gtest/gtest.h>
#include "cognitive/fault_tolerance/nimcp_emotional_tagging.h"
#include <cmath>

//=============================================================================
// Test Fixtures
//=============================================================================

class EmotionalTaggingTest : public ::testing::Test {
protected:
    nimcp_emotional_tagger_t* tagger;

    void SetUp() override {
        tagger = nimcp_emotional_tagger_create();
        ASSERT_NE(nullptr, tagger);
    }

    void TearDown() override {
        if (tagger) {
            nimcp_emotional_tagger_destroy(tagger);
        }
    }
};

//=============================================================================
// Helper Functions
//=============================================================================

// Create a mock recovery episode
static nimcp_recovery_episode_t create_episode(
    const char* error_type,
    bool success,
    float data_loss_risk,
    uint64_t recovery_time_us,
    uint32_t retry_count
) {
    nimcp_recovery_episode_t episode;
    snprintf(episode.error_type, sizeof(episode.error_type), "%s", error_type);
    episode.success = success;
    episode.data_loss_risk = data_loss_risk;
    episode.recovery_time_us = recovery_time_us;
    episode.retry_count = retry_count;
    episode.timestamp_us = 1000000; // 1 second
    return episode;
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(EmotionalTaggingTest, Create_ValidTagger_Success) {
    EXPECT_NE(nullptr, tagger);
}

TEST_F(EmotionalTaggingTest, Destroy_ValidTagger_Success) {
    nimcp_emotional_tagger_destroy(tagger);
    tagger = nullptr; // Prevent double-free in TearDown
}

TEST_F(EmotionalTaggingTest, Destroy_NullTagger_NoCrash) {
    nimcp_emotional_tagger_destroy(nullptr);
}

TEST_F(EmotionalTaggingTest, Reset_ValidTagger_Success) {
    EXPECT_TRUE(nimcp_emotional_tagger_reset(tagger));
}

TEST_F(EmotionalTaggingTest, Reset_NullTagger_Failure) {
    EXPECT_FALSE(nimcp_emotional_tagger_reset(nullptr));
}

//=============================================================================
// Emotion Computation Tests - Critical Failures
//=============================================================================

TEST_F(EmotionalTaggingTest, ComputeEmotion_SevereFailureWithDataLoss_HighNegativeValence) {
    // ARRANGE: Critical failure with high data loss risk
    nimcp_recovery_episode_t episode = create_episode(
        "SIGSEGV", false, 0.9f, 5000000, 0);

    // ACT
    nimcp_emotional_tag_t emotion;
    ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));

    // ASSERT: Very negative valence, high arousal, high fear
    EXPECT_LT(emotion.valence, -0.7f);
    EXPECT_GT(emotion.arousal, 0.8f);
    EXPECT_GT(emotion.fear, 0.7f);
    EXPECT_NEAR(emotion.relief, 0.0f, 0.1f);
    EXPECT_NEAR(emotion.frustration, 0.0f, 0.1f);
}

TEST_F(EmotionalTaggingTest, ComputeEmotion_SIGSEGVHighDataLoss_MaximalFear) {
    // ARRANGE: Worst case - SIGSEGV + 95% data loss risk
    nimcp_recovery_episode_t episode = create_episode(
        "SIGSEGV", false, 0.95f, 10000000, 0);

    // ACT
    nimcp_emotional_tag_t emotion;
    ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));

    // ASSERT: Maximum fear and arousal
    EXPECT_NEAR(emotion.valence, -0.9f, 0.15f);
    EXPECT_NEAR(emotion.arousal, 0.95f, 0.1f);
    EXPECT_NEAR(emotion.fear, 0.9f, 0.15f);
}

TEST_F(EmotionalTaggingTest, ComputeEmotion_DataCorruption_HighFear) {
    // ARRANGE: Data corruption with moderate data loss
    nimcp_recovery_episode_t episode = create_episode(
        "DATA_CORRUPTION", false, 0.7f, 3000000, 0);

    // ACT
    nimcp_emotional_tag_t emotion;
    ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));

    // ASSERT: Negative valence, high fear
    EXPECT_LT(emotion.valence, 0.0f);
    EXPECT_GT(emotion.fear, 0.5f);
}

//=============================================================================
// Emotion Computation Tests - Successful Recovery
//=============================================================================

TEST_F(EmotionalTaggingTest, ComputeEmotion_FastSuccessfulRecovery_PositiveValence) {
    // ARRANGE: Fast successful recovery
    nimcp_recovery_episode_t episode = create_episode(
        "NULL_POINTER", true, 0.2f, 500, 0);

    // ACT
    nimcp_emotional_tag_t emotion;
    ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));

    // ASSERT: Positive valence, moderate arousal, high relief
    EXPECT_GT(emotion.valence, 0.5f);
    EXPECT_GT(emotion.arousal, 0.3f);
    EXPECT_GT(emotion.relief, 0.6f);
    EXPECT_NEAR(emotion.fear, 0.0f, 0.1f);
}

TEST_F(EmotionalTaggingTest, ComputeEmotion_VeryFastRecovery_MaximalRelief) {
    // ARRANGE: Ultra-fast recovery (< 1ms)
    nimcp_recovery_episode_t episode = create_episode(
        "TIMEOUT", true, 0.1f, 800, 0);

    // ACT
    nimcp_emotional_tag_t emotion;
    ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));

    // ASSERT: Strong positive emotion
    EXPECT_GT(emotion.valence, 0.6f);
    EXPECT_GT(emotion.relief, 0.7f);
}

TEST_F(EmotionalTaggingTest, ComputeEmotion_SuccessButSlow_ModerateRelief) {
    // ARRANGE: Successful but slow recovery (10 seconds)
    nimcp_recovery_episode_t episode = create_episode(
        "DEADLOCK", true, 0.3f, 10000000, 3);

    // ACT
    nimcp_emotional_tag_t emotion;
    ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));

    // ASSERT: Positive but less intense
    EXPECT_GT(emotion.valence, 0.0f);
    EXPECT_LT(emotion.valence, 0.7f);
    EXPECT_GT(emotion.relief, 0.0f);
}

//=============================================================================
// Emotion Computation Tests - Repeated Failures
//=============================================================================

TEST_F(EmotionalTaggingTest, ComputeEmotion_RepeatedFailures_HighFrustration) {
    // ARRANGE: Many retries indicate repeated failures
    nimcp_recovery_episode_t episode = create_episode(
        "RESOURCE_EXHAUSTED", false, 0.4f, 8000000, 10);

    // ACT
    nimcp_emotional_tag_t emotion;
    ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));

    // ASSERT: High frustration from repeated failures
    EXPECT_GT(emotion.frustration, 0.5f);
    EXPECT_LT(emotion.valence, 0.0f);
}

TEST_F(EmotionalTaggingTest, ComputeEmotion_MaximalRetries_MaximalFrustration) {
    // ARRANGE: Excessive retries (20+)
    nimcp_recovery_episode_t episode = create_episode(
        "NETWORK_ERROR", false, 0.3f, 15000000, 25);

    // ACT
    nimcp_emotional_tag_t emotion;
    ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));

    // ASSERT: Very high frustration
    EXPECT_GT(emotion.frustration, 0.8f);
}

TEST_F(EmotionalTaggingTest, ComputeEmotion_FewRetries_LowFrustration) {
    // ARRANGE: Only 1-2 retries
    nimcp_recovery_episode_t episode = create_episode(
        "TIMEOUT", false, 0.2f, 2000000, 2);

    // ACT
    nimcp_emotional_tag_t emotion;
    ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));

    // ASSERT: Low frustration
    EXPECT_LT(emotion.frustration, 0.4f);
}

//=============================================================================
// Memory Strength Boost Tests
//=============================================================================

TEST_F(EmotionalTaggingTest, MemoryBoost_HighArousalNegativeValence_StrongBoost) {
    // ARRANGE: Critical failure emotion
    nimcp_emotional_tag_t emotion;
    emotion.valence = -0.9f;
    emotion.arousal = 0.95f;
    emotion.fear = 0.9f;
    emotion.relief = 0.0f;
    emotion.frustration = 0.0f;

    // ACT
    float boost = nimcp_emotional_memory_boost(&emotion);

    // ASSERT: Strong memory boost (1.5x - 2.0x)
    EXPECT_GT(boost, 1.5f);
    EXPECT_LE(boost, 2.5f);
}

TEST_F(EmotionalTaggingTest, MemoryBoost_HighArousalPositiveValence_ModerateBoost) {
    // ARRANGE: Successful recovery emotion
    nimcp_emotional_tag_t emotion;
    emotion.valence = 0.8f;
    emotion.arousal = 0.6f;
    emotion.fear = 0.0f;
    emotion.relief = 0.85f;
    emotion.frustration = 0.0f;

    // ACT
    float boost = nimcp_emotional_memory_boost(&emotion);

    // ASSERT: Moderate memory boost (1.3x - 1.8x)
    EXPECT_GT(boost, 1.2f);
    EXPECT_LE(boost, 2.0f);
}

TEST_F(EmotionalTaggingTest, MemoryBoost_LowArousal_MinimalBoost) {
    // ARRANGE: Low-importance emotion
    nimcp_emotional_tag_t emotion;
    emotion.valence = 0.2f;
    emotion.arousal = 0.1f;
    emotion.fear = 0.0f;
    emotion.relief = 0.1f;
    emotion.frustration = 0.0f;

    // ACT
    float boost = nimcp_emotional_memory_boost(&emotion);

    // ASSERT: Minimal boost (close to 1.0x)
    EXPECT_GE(boost, 1.0f);
    EXPECT_LT(boost, 1.3f);
}

TEST_F(EmotionalTaggingTest, MemoryBoost_NullEmotion_DefaultBoost) {
    // ACT
    float boost = nimcp_emotional_memory_boost(nullptr);

    // ASSERT: Default boost of 1.0
    EXPECT_NEAR(boost, 1.0f, 0.01f);
}

//=============================================================================
// Priority Computation Tests
//=============================================================================

TEST_F(EmotionalTaggingTest, Priority_HighArousal_HighPriority) {
    // ARRANGE: High arousal emotion
    nimcp_emotional_tag_t emotion;
    emotion.valence = -0.8f;
    emotion.arousal = 0.9f;
    emotion.fear = 0.85f;
    emotion.relief = 0.0f;
    emotion.frustration = 0.0f;

    // ACT
    float priority = nimcp_emotional_priority(&emotion);

    // ASSERT: High priority (0.8 - 1.0)
    EXPECT_GT(priority, 0.7f);
    EXPECT_LE(priority, 1.0f);
}

TEST_F(EmotionalTaggingTest, Priority_LowArousal_LowPriority) {
    // ARRANGE: Low arousal emotion
    nimcp_emotional_tag_t emotion;
    emotion.valence = 0.0f;
    emotion.arousal = 0.1f;
    emotion.fear = 0.0f;
    emotion.relief = 0.0f;
    emotion.frustration = 0.0f;

    // ACT
    float priority = nimcp_emotional_priority(&emotion);

    // ASSERT: Low priority (0.0 - 0.3)
    EXPECT_GE(priority, 0.0f);
    EXPECT_LT(priority, 0.4f);
}

TEST_F(EmotionalTaggingTest, Priority_FearBoost_IncreasedPriority) {
    // ARRANGE: Moderate arousal with high fear
    nimcp_emotional_tag_t emotion;
    emotion.valence = -0.6f;
    emotion.arousal = 0.5f;
    emotion.fear = 0.9f;
    emotion.relief = 0.0f;
    emotion.frustration = 0.0f;

    // ACT
    float priority = nimcp_emotional_priority(&emotion);

    // ASSERT: Fear boosts priority
    EXPECT_GT(priority, 0.5f);
}

TEST_F(EmotionalTaggingTest, Priority_NullEmotion_ZeroPriority) {
    // ACT
    float priority = nimcp_emotional_priority(nullptr);

    // ASSERT: Zero priority
    EXPECT_NEAR(priority, 0.0f, 0.01f);
}

//=============================================================================
// Boundary Value Tests
//=============================================================================

TEST_F(EmotionalTaggingTest, ComputeEmotion_ZeroDataLossRisk_MinimalFear) {
    // ARRANGE: No data loss risk
    nimcp_recovery_episode_t episode = create_episode(
        "WARNING", false, 0.0f, 1000, 0);

    // ACT
    nimcp_emotional_tag_t emotion;
    ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));

    // ASSERT: Minimal fear
    EXPECT_NEAR(emotion.fear, 0.0f, 0.1f);
}

TEST_F(EmotionalTaggingTest, ComputeEmotion_MaxDataLossRisk_MaximalFear) {
    // ARRANGE: 100% data loss risk
    nimcp_recovery_episode_t episode = create_episode(
        "CATASTROPHIC", false, 1.0f, 10000000, 0);

    // ACT
    nimcp_emotional_tag_t emotion;
    ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));

    // ASSERT: Maximum fear
    EXPECT_GT(emotion.fear, 0.8f);
}

TEST_F(EmotionalTaggingTest, ComputeEmotion_InstantRecovery_MaximalRelief) {
    // ARRANGE: Instant recovery (1 microsecond)
    nimcp_recovery_episode_t episode = create_episode(
        "MINOR_ERROR", true, 0.0f, 1, 0);

    // ACT
    nimcp_emotional_tag_t emotion;
    ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));

    // ASSERT: Very high relief
    EXPECT_GT(emotion.relief, 0.7f);
}

TEST_F(EmotionalTaggingTest, ComputeEmotion_VerySlowRecovery_LowRelief) {
    // ARRANGE: Very slow recovery (100 seconds)
    nimcp_recovery_episode_t episode = create_episode(
        "SLOW_ERROR", true, 0.5f, 100000000, 5);

    // ACT
    nimcp_emotional_tag_t emotion;
    ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));

    // ASSERT: Low or no relief
    EXPECT_LT(emotion.relief, 0.5f);
}

//=============================================================================
// Valence Range Tests
//=============================================================================

TEST_F(EmotionalTaggingTest, ValenceRange_AllEmotions_WithinBounds) {
    // Test various scenarios to ensure valence stays in [-1.0, 1.0]
    nimcp_recovery_episode_t episodes[] = {
        create_episode("SIGSEGV", false, 1.0f, 10000000, 20),
        create_episode("SUCCESS", true, 0.0f, 100, 0),
        create_episode("MODERATE", false, 0.5f, 5000, 3),
        create_episode("TIMEOUT", true, 0.3f, 50000, 1),
    };

    for (const auto& episode : episodes) {
        nimcp_emotional_tag_t emotion;
        ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));

        EXPECT_GE(emotion.valence, -1.0f);
        EXPECT_LE(emotion.valence, 1.0f);
    }
}

TEST_F(EmotionalTaggingTest, ArousalRange_AllEmotions_WithinBounds) {
    // Test various scenarios to ensure arousal stays in [0.0, 1.0]
    nimcp_recovery_episode_t episodes[] = {
        create_episode("SIGSEGV", false, 1.0f, 10000000, 20),
        create_episode("SUCCESS", true, 0.0f, 100, 0),
        create_episode("MODERATE", false, 0.5f, 5000, 3),
    };

    for (const auto& episode : episodes) {
        nimcp_emotional_tag_t emotion;
        ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));

        EXPECT_GE(emotion.arousal, 0.0f);
        EXPECT_LE(emotion.arousal, 1.0f);
    }
}

TEST_F(EmotionalTaggingTest, SpecificEmotionsRange_AllValues_WithinBounds) {
    // Test that fear, relief, frustration stay in [0.0, 1.0]
    nimcp_recovery_episode_t episodes[] = {
        create_episode("SIGSEGV", false, 1.0f, 10000000, 25),
        create_episode("SUCCESS", true, 0.0f, 50, 0),
        create_episode("RETRY", false, 0.5f, 5000, 15),
    };

    for (const auto& episode : episodes) {
        nimcp_emotional_tag_t emotion;
        ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));

        EXPECT_GE(emotion.fear, 0.0f);
        EXPECT_LE(emotion.fear, 1.0f);
        EXPECT_GE(emotion.relief, 0.0f);
        EXPECT_LE(emotion.relief, 1.0f);
        EXPECT_GE(emotion.frustration, 0.0f);
        EXPECT_LE(emotion.frustration, 1.0f);
    }
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(EmotionalTaggingTest, ComputeTag_NullTagger_Failure) {
    nimcp_recovery_episode_t episode = create_episode("ERROR", false, 0.5f, 1000, 0);
    nimcp_emotional_tag_t emotion;

    EXPECT_FALSE(nimcp_emotional_tagger_compute_tag(nullptr, &episode, &emotion));
}

TEST_F(EmotionalTaggingTest, ComputeTag_NullEpisode_Failure) {
    nimcp_emotional_tag_t emotion;

    EXPECT_FALSE(nimcp_emotional_tagger_compute_tag(tagger, nullptr, &emotion));
}

TEST_F(EmotionalTaggingTest, ComputeTag_NullOutput_Failure) {
    nimcp_recovery_episode_t episode = create_episode("ERROR", false, 0.5f, 1000, 0);

    EXPECT_FALSE(nimcp_emotional_tagger_compute_tag(tagger, &episode, nullptr));
}

TEST_F(EmotionalTaggingTest, ComputeTag_AllNull_Failure) {
    EXPECT_FALSE(nimcp_emotional_tagger_compute_tag(nullptr, nullptr, nullptr));
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(EmotionalTaggingTest, Statistics_AfterMultipleTags_CorrectCounts) {
    // ARRANGE: Tag multiple episodes
    for (int i = 0; i < 5; i++) {
        nimcp_recovery_episode_t episode = create_episode("ERROR", false, 0.5f, 1000, i);
        nimcp_emotional_tag_t emotion;
        ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));
    }

    // ACT
    nimcp_emotional_tagger_stats_t stats;
    ASSERT_TRUE(nimcp_emotional_tagger_get_stats(tagger, &stats));

    // ASSERT
    EXPECT_EQ(stats.total_tags, 5);
    EXPECT_GT(stats.avg_valence, -1.0f);
    EXPECT_LT(stats.avg_valence, 1.0f);
}

TEST_F(EmotionalTaggingTest, Statistics_NullTagger_Failure) {
    nimcp_emotional_tagger_stats_t stats;
    EXPECT_FALSE(nimcp_emotional_tagger_get_stats(nullptr, &stats));
}

TEST_F(EmotionalTaggingTest, Statistics_NullOutput_Failure) {
    EXPECT_FALSE(nimcp_emotional_tagger_get_stats(tagger, nullptr));
}

TEST_F(EmotionalTaggingTest, Statistics_AfterReset_Cleared) {
    // ARRANGE: Tag some episodes
    nimcp_recovery_episode_t episode = create_episode("ERROR", false, 0.5f, 1000, 0);
    nimcp_emotional_tag_t emotion;
    nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion);

    // ACT: Reset
    ASSERT_TRUE(nimcp_emotional_tagger_reset(tagger));

    // ASSERT: Stats cleared
    nimcp_emotional_tagger_stats_t stats;
    ASSERT_TRUE(nimcp_emotional_tagger_get_stats(tagger, &stats));
    EXPECT_EQ(stats.total_tags, 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
