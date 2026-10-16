/**
 * @file test_emotional_tagging_integration.cpp
 * @brief Integration Tests for Emotional Tagging System
 *
 * WHAT: Integration tests for emotional tagging with episodic memory and recovery
 * WHY:  Verify emotional tagging works correctly in realistic scenarios
 * HOW:  Test sequences of failures, memory prioritization, emotion-guided learning
 *
 * INTEGRATION AREAS:
 * - Episodic memory: Emotionally-tagged episodes stored with priority
 * - Recovery sequences: Multiple failures, emotional accumulation
 * - Memory consolidation: Emotion-based strength boosting
 * - Priority-based retrieval: High-emotion events retrieved first
 * - Real-world scenarios: SIGSEGV, timeouts, data corruption
 */

#include <gtest/gtest.h>
#include "cognitive/fault_tolerance/nimcp_emotional_tagging.h"
#include <vector>
#include <algorithm>
#include <cmath>

//=============================================================================
// Test Fixtures
//=============================================================================

class EmotionalTaggingIntegrationTest : public ::testing::Test {
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

    // Helper: Create episode
    nimcp_recovery_episode_t create_episode(
        const char* error_type, bool success,
        float data_loss_risk, uint64_t recovery_time_us,
        uint32_t retry_count
    ) {
        nimcp_recovery_episode_t episode;
        snprintf(episode.error_type, sizeof(episode.error_type), "%s", error_type);
        episode.success = success;
        episode.data_loss_risk = data_loss_risk;
        episode.recovery_time_us = recovery_time_us;
        episode.retry_count = retry_count;
        episode.timestamp_us = 1000000;
        return episode;
    }
};

//=============================================================================
// Realistic Failure Scenarios
//=============================================================================

TEST_F(EmotionalTaggingIntegrationTest, CriticalFailureSequence_SIGSEGV_HighEmotionalImpact) {
    // SCENARIO: SIGSEGV with high data loss risk, multiple recovery attempts

    std::vector<nimcp_recovery_episode_t> episodes = {
        create_episode("SIGSEGV", false, 0.9f, 5000000, 0),  // Initial failure
        create_episode("SIGSEGV", false, 0.85f, 8000000, 1), // Retry 1 fails
        create_episode("SIGSEGV", false, 0.8f, 10000000, 2), // Retry 2 fails
        create_episode("SIGSEGV", true, 0.75f, 500000, 3),   // Finally succeeds
    };

    std::vector<nimcp_emotional_tag_t> emotions;
    std::vector<float> memory_boosts;
    std::vector<float> priorities;

    // Tag all episodes
    for (const auto& episode : episodes) {
        nimcp_emotional_tag_t emotion;
        ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));
        emotions.push_back(emotion);
        memory_boosts.push_back(nimcp_emotional_memory_boost(&emotion));
        priorities.push_back(nimcp_emotional_priority(&emotion));
    }

    // ASSERT: All failures have high negative valence and arousal
    for (size_t i = 0; i < 3; i++) {
        EXPECT_LT(emotions[i].valence, -0.6f) << "Failure " << i;
        EXPECT_GT(emotions[i].arousal, 0.7f) << "Failure " << i;
        EXPECT_GT(emotions[i].fear, 0.6f) << "Failure " << i;
    }

    // ASSERT: Frustration increases with retries
    EXPECT_LT(emotions[0].frustration, emotions[1].frustration);
    EXPECT_LT(emotions[1].frustration, emotions[2].frustration);

    // ASSERT: Final success has relief
    EXPECT_GT(emotions[3].valence, 0.0f);
    EXPECT_GT(emotions[3].relief, 0.5f);
    EXPECT_NEAR(emotions[3].fear, 0.0f, 0.1f);

    // ASSERT: All episodes get strong memory boost (high arousal)
    for (float boost : memory_boosts) {
        EXPECT_GT(boost, 1.3f);
    }

    // ASSERT: All episodes have high priority
    for (float priority : priorities) {
        EXPECT_GT(priority, 0.6f);
    }
}

TEST_F(EmotionalTaggingIntegrationTest, DataCorruptionScenario_EscalatingThreat) {
    // SCENARIO: Data corruption detected, escalating severity

    std::vector<nimcp_recovery_episode_t> episodes = {
        create_episode("DATA_CORRUPTION", false, 0.3f, 2000000, 0), // Minor corruption
        create_episode("DATA_CORRUPTION", false, 0.6f, 4000000, 1), // Spreading
        create_episode("DATA_CORRUPTION", false, 0.9f, 8000000, 2), // Critical
    };

    std::vector<nimcp_emotional_tag_t> emotions;
    for (const auto& episode : episodes) {
        nimcp_emotional_tag_t emotion;
        ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));
        emotions.push_back(emotion);
    }

    // ASSERT: Fear escalates with data loss risk
    EXPECT_LT(emotions[0].fear, emotions[1].fear);
    EXPECT_LT(emotions[1].fear, emotions[2].fear);
    EXPECT_GT(emotions[2].fear, 0.8f);

    // ASSERT: Arousal increases with severity
    EXPECT_LT(emotions[0].arousal, emotions[1].arousal);
    EXPECT_LT(emotions[1].arousal, emotions[2].arousal);

    // ASSERT: Valence becomes more negative
    EXPECT_GT(emotions[0].valence, emotions[1].valence);
    EXPECT_GT(emotions[1].valence, emotions[2].valence);
}

TEST_F(EmotionalTaggingIntegrationTest, FastRecoverySuccess_PositiveReinforcement) {
    // SCENARIO: Series of fast, successful recoveries (good coping strategy)

    std::vector<nimcp_recovery_episode_t> episodes = {
        create_episode("TIMEOUT", true, 0.1f, 500, 0),
        create_episode("NULL_POINTER", true, 0.2f, 300, 0),
        create_episode("RESOURCE_BUSY", true, 0.15f, 800, 0),
    };

    std::vector<nimcp_emotional_tag_t> emotions;
    for (const auto& episode : episodes) {
        nimcp_emotional_tag_t emotion;
        ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));
        emotions.push_back(emotion);
    }

    // ASSERT: All have positive valence
    for (const auto& emotion : emotions) {
        EXPECT_GT(emotion.valence, 0.5f);
        EXPECT_GT(emotion.relief, 0.6f);
        EXPECT_NEAR(emotion.fear, 0.0f, 0.1f);
        EXPECT_NEAR(emotion.frustration, 0.0f, 0.1f);
    }

    // ASSERT: Memory boost for successful strategies
    for (const auto& emotion : emotions) {
        float boost = nimcp_emotional_memory_boost(&emotion);
        EXPECT_GT(boost, 1.2f); // Positive reinforcement
    }
}

//=============================================================================
// Memory Prioritization Integration
//=============================================================================

TEST_F(EmotionalTaggingIntegrationTest, MemoryPrioritization_CriticalVsMinorErrors) {
    // SCENARIO: Mix of critical and minor errors, verify priority ordering

    struct Episode {
        nimcp_recovery_episode_t episode;
        nimcp_emotional_tag_t emotion;
        float priority;
        float memory_boost;
    };

    std::vector<Episode> episodes = {
        { create_episode("MINOR_WARNING", true, 0.05f, 100, 0) },
        { create_episode("SIGSEGV", false, 0.95f, 10000000, 5) },
        { create_episode("TIMEOUT", false, 0.3f, 50000, 2) },
        { create_episode("DATA_CORRUPTION", false, 0.8f, 7000000, 0) },
        { create_episode("SUCCESS", true, 0.1f, 200, 0) },
    };

    // Compute emotions and priorities
    for (auto& ep : episodes) {
        ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &ep.episode, &ep.emotion));
        ep.priority = nimcp_emotional_priority(&ep.emotion);
        ep.memory_boost = nimcp_emotional_memory_boost(&ep.emotion);
    }

    // Sort by priority (descending)
    std::sort(episodes.begin(), episodes.end(),
              [](const Episode& a, const Episode& b) {
                  return a.priority > b.priority;
              });

    // ASSERT: SIGSEGV should be highest priority
    EXPECT_STREQ(episodes[0].episode.error_type, "SIGSEGV");
    EXPECT_GT(episodes[0].priority, 0.85f);
    EXPECT_GT(episodes[0].memory_boost, 1.8f);

    // ASSERT: DATA_CORRUPTION second
    EXPECT_STREQ(episodes[1].episode.error_type, "DATA_CORRUPTION");
    EXPECT_GT(episodes[1].priority, 0.7f);

    // ASSERT: Minor warning lowest priority
    EXPECT_STREQ(episodes[4].episode.error_type, "MINOR_WARNING");
    EXPECT_LT(episodes[4].priority, 0.3f);
}

TEST_F(EmotionalTaggingIntegrationTest, MemoryStrength_ProportionalToArousal) {
    // SCENARIO: Verify memory boost scales correctly with arousal

    std::vector<nimcp_recovery_episode_t> episodes = {
        create_episode("LOW", true, 0.1f, 1000, 0),      // Low arousal
        create_episode("MEDIUM", false, 0.5f, 5000, 3),  // Medium arousal
        create_episode("HIGH", false, 0.9f, 10000, 10),  // High arousal
    };

    std::vector<float> boosts;
    for (const auto& episode : episodes) {
        nimcp_emotional_tag_t emotion;
        ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));
        boosts.push_back(nimcp_emotional_memory_boost(&emotion));
    }

    // ASSERT: Monotonically increasing boost
    EXPECT_LT(boosts[0], boosts[1]);
    EXPECT_LT(boosts[1], boosts[2]);

    // ASSERT: Range check
    EXPECT_GE(boosts[0], 1.0f);
    EXPECT_LT(boosts[0], 1.4f);
    EXPECT_GT(boosts[2], 1.7f);
}

//=============================================================================
// Repeated Failure Patterns
//=============================================================================

TEST_F(EmotionalTaggingIntegrationTest, RepeatedFailures_FrustrationAccumulation) {
    // SCENARIO: Same error type repeatedly failing

    std::vector<nimcp_emotional_tag_t> emotions;
    for (uint32_t retry = 0; retry < 15; retry++) {
        nimcp_recovery_episode_t episode = create_episode(
            "DEADLOCK", false, 0.4f, 5000000, retry);

        nimcp_emotional_tag_t emotion;
        ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));
        emotions.push_back(emotion);
    }

    // ASSERT: Frustration increases with retry count
    EXPECT_LT(emotions[0].frustration, emotions[5].frustration);
    EXPECT_LT(emotions[5].frustration, emotions[10].frustration);
    EXPECT_LT(emotions[10].frustration, emotions[14].frustration);

    // ASSERT: High frustration after many retries
    EXPECT_GT(emotions[14].frustration, 0.7f);

    // ASSERT: Negative valence throughout
    for (const auto& emotion : emotions) {
        EXPECT_LT(emotion.valence, 0.0f);
    }
}

TEST_F(EmotionalTaggingIntegrationTest, IntermittentSuccess_MixedEmotions) {
    // SCENARIO: Alternating success and failure

    std::vector<nimcp_recovery_episode_t> episodes = {
        create_episode("NETWORK", false, 0.3f, 3000000, 0),
        create_episode("NETWORK", true, 0.2f, 1000, 0),
        create_episode("NETWORK", false, 0.4f, 5000000, 1),
        create_episode("NETWORK", true, 0.15f, 800, 0),
    };

    std::vector<nimcp_emotional_tag_t> emotions;
    for (const auto& episode : episodes) {
        nimcp_emotional_tag_t emotion;
        ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));
        emotions.push_back(emotion);
    }

    // ASSERT: Valence alternates
    EXPECT_LT(emotions[0].valence, 0.0f); // Failure
    EXPECT_GT(emotions[1].valence, 0.0f); // Success
    EXPECT_LT(emotions[2].valence, 0.0f); // Failure
    EXPECT_GT(emotions[3].valence, 0.0f); // Success

    // ASSERT: Relief only for successes
    EXPECT_NEAR(emotions[0].relief, 0.0f, 0.1f);
    EXPECT_GT(emotions[1].relief, 0.5f);
    EXPECT_NEAR(emotions[2].relief, 0.0f, 0.1f);
    EXPECT_GT(emotions[3].relief, 0.5f);
}

//=============================================================================
// Statistics Integration
//=============================================================================

TEST_F(EmotionalTaggingIntegrationTest, Statistics_TrackEmotionalPatterns) {
    // SCENARIO: Mixed workload, verify statistics tracking

    // Tag diverse episodes
    std::vector<nimcp_recovery_episode_t> episodes = {
        create_episode("SIGSEGV", false, 0.95f, 10000000, 0),   // High fear
        create_episode("SUCCESS", true, 0.05f, 100, 0),         // High relief
        create_episode("RETRY", false, 0.3f, 5000, 12),         // High frustration
        create_episode("NORMAL", true, 0.2f, 1000, 0),
        create_episode("ERROR", false, 0.4f, 3000, 0),
    };

    for (const auto& episode : episodes) {
        nimcp_emotional_tag_t emotion;
        ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));
    }

    // Get statistics
    nimcp_emotional_tagger_stats_t stats;
    ASSERT_TRUE(nimcp_emotional_tagger_get_stats(tagger, &stats));

    // ASSERT: Correct count
    EXPECT_EQ(stats.total_tags, 5);

    // ASSERT: Average valence is negative (more failures)
    EXPECT_LT(stats.avg_valence, 0.0f);

    // ASSERT: High emotion counts
    EXPECT_GE(stats.high_fear_count, 1);      // SIGSEGV
    EXPECT_GE(stats.high_relief_count, 1);    // Fast success
    EXPECT_GE(stats.high_frustration_count, 1); // Many retries
}

TEST_F(EmotionalTaggingIntegrationTest, Statistics_ResetFunctionality) {
    // Tag some episodes
    for (int i = 0; i < 3; i++) {
        nimcp_recovery_episode_t episode = create_episode("ERROR", false, 0.5f, 1000, i);
        nimcp_emotional_tag_t emotion;
        nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion);
    }

    // Verify non-zero stats
    nimcp_emotional_tagger_stats_t stats1;
    ASSERT_TRUE(nimcp_emotional_tagger_get_stats(tagger, &stats1));
    EXPECT_EQ(stats1.total_tags, 3);

    // Reset
    ASSERT_TRUE(nimcp_emotional_tagger_reset(tagger));

    // Verify cleared
    nimcp_emotional_tagger_stats_t stats2;
    ASSERT_TRUE(nimcp_emotional_tagger_get_stats(tagger, &stats2));
    EXPECT_EQ(stats2.total_tags, 0);
    EXPECT_NEAR(stats2.avg_valence, 0.0f, 0.01f);
    EXPECT_NEAR(stats2.avg_arousal, 0.0f, 0.01f);
}

//=============================================================================
// Edge Case Integration
//=============================================================================

TEST_F(EmotionalTaggingIntegrationTest, EdgeCase_ZeroDataLossRisk_MinimalEmotion) {
    nimcp_recovery_episode_t episode = create_episode("MINOR", true, 0.0f, 100, 0);
    nimcp_emotional_tag_t emotion;

    ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));

    EXPECT_NEAR(emotion.fear, 0.0f, 0.1f);
    EXPECT_GT(emotion.relief, 0.0f); // Success gives relief
    EXPECT_LT(emotion.arousal, 0.5f); // Low arousal
}

TEST_F(EmotionalTaggingIntegrationTest, EdgeCase_MaximalDataLossRisk_MaximalFear) {
    nimcp_recovery_episode_t episode = create_episode("CATASTROPHIC", false, 1.0f, 10000000, 0);
    nimcp_emotional_tag_t emotion;

    ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));

    EXPECT_GT(emotion.fear, 0.8f);
    EXPECT_GT(emotion.arousal, 0.8f);
    EXPECT_LT(emotion.valence, -0.7f);
}

TEST_F(EmotionalTaggingIntegrationTest, EdgeCase_VerySlowRecovery_ReducedRelief) {
    nimcp_recovery_episode_t episode = create_episode("SLOW", true, 0.3f, 100000000, 5);
    nimcp_emotional_tag_t emotion;

    ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));

    // Success but slow → reduced relief
    EXPECT_GT(emotion.valence, 0.0f); // Still positive
    EXPECT_LT(emotion.relief, 0.6f);  // But less relief than fast recovery
}

//=============================================================================
// Long-Running Scenarios
//=============================================================================

TEST_F(EmotionalTaggingIntegrationTest, LongRunning_100Episodes_ConsistentBehavior) {
    // SCENARIO: Tag 100 episodes, verify consistency

    for (int i = 0; i < 100; i++) {
        float data_loss = (i % 10) * 0.1f;
        bool success = (i % 3) == 0;
        uint32_t retries = i % 15;

        nimcp_recovery_episode_t episode = create_episode(
            "TEST", success, data_loss, 1000 * (i + 1), retries);

        nimcp_emotional_tag_t emotion;
        ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));

        // Verify bounds
        EXPECT_GE(emotion.valence, -1.0f);
        EXPECT_LE(emotion.valence, 1.0f);
        EXPECT_GE(emotion.arousal, 0.0f);
        EXPECT_LE(emotion.arousal, 1.0f);
    }

    // Verify statistics
    nimcp_emotional_tagger_stats_t stats;
    ASSERT_TRUE(nimcp_emotional_tagger_get_stats(tagger, &stats));
    EXPECT_EQ(stats.total_tags, 100);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
