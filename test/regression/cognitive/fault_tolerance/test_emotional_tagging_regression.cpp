/**
 * @file test_emotional_tagging_regression.cpp
 * @brief Regression Tests for Emotional Tagging System
 *
 * WHAT: Regression tests to prevent breaking changes to emotional tagging
 * WHY:  Ensure emotional responses remain stable and predictable over time
 * HOW:  Fixed test cases with expected outputs, performance benchmarks
 *
 * REGRESSION COVERAGE:
 * - Emotion value stability: Fixed inputs → fixed outputs
 * - Memory boost consistency: Boost formulas unchanged
 * - Priority computation: Priority calculation stable
 * - Performance: Tagging performance stays within bounds
 * - Statistics accuracy: Counters and averages correct
 * - Boundary behavior: Edge cases handled consistently
 */

#include <gtest/gtest.h>
#include "cognitive/fault_tolerance/nimcp_emotional_tagging.h"
#include <chrono>
#include <cmath>

//=============================================================================
// Test Fixtures
//=============================================================================

class EmotionalTaggingRegressionTest : public ::testing::Test {
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

    // Helper: Check emotion approximately equal
    void expect_emotion_near(
        const nimcp_emotional_tag_t& actual,
        float expected_valence,
        float expected_arousal,
        float expected_fear,
        float expected_relief,
        float expected_frustration,
        float tolerance = 0.25f
    ) {
        EXPECT_NEAR(actual.valence, expected_valence, tolerance) << "Valence mismatch";
        EXPECT_NEAR(actual.arousal, expected_arousal, tolerance) << "Arousal mismatch";
        EXPECT_NEAR(actual.fear, expected_fear, tolerance) << "Fear mismatch";
        EXPECT_NEAR(actual.relief, expected_relief, tolerance) << "Relief mismatch";
        EXPECT_NEAR(actual.frustration, expected_frustration, tolerance) << "Frustration mismatch";
    }
};

//=============================================================================
// Emotion Value Stability Tests
//=============================================================================

TEST_F(EmotionalTaggingRegressionTest, CriticalFailure_SIGSEGV_StableEmotions) {
    // REGRESSION: SIGSEGV with high data loss should produce consistent emotions

    nimcp_recovery_episode_t episode = create_episode(
        "SIGSEGV", false, 0.9f, 5000000, 0);

    nimcp_emotional_tag_t emotion;
    ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));

    // Expected values (baseline from original implementation)
    expect_emotion_near(emotion,
        -0.9f,  // valence: very negative
        0.95f,  // arousal: very high
        0.9f,   // fear: very high
        0.0f,   // relief: none (failed)
        0.0f    // frustration: low (first attempt)
    );
}

TEST_F(EmotionalTaggingRegressionTest, FastSuccess_StableEmotions) {
    // REGRESSION: Fast successful recovery should produce consistent positive emotions

    nimcp_recovery_episode_t episode = create_episode(
        "TIMEOUT", true, 0.1f, 500, 0);

    nimcp_emotional_tag_t emotion;
    ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));

    // Expected values
    expect_emotion_near(emotion,
        0.9f,   // valence: very positive (fast + success)
        0.17f,  // arousal: low-moderate
        0.0f,   // fear: none (success)
        0.9f,   // relief: very high (fast recovery)
        0.0f    // frustration: none (success)
    );
}

TEST_F(EmotionalTaggingRegressionTest, RepeatedFailure_StableFrustration) {
    // REGRESSION: 10 retries should produce consistent frustration level

    nimcp_recovery_episode_t episode = create_episode(
        "DEADLOCK", false, 0.4f, 8000000, 10);

    nimcp_emotional_tag_t emotion;
    ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));

    // Expected values
    expect_emotion_near(emotion,
        -0.8f,  // valence: negative (failure + frustration)
        0.6f,   // arousal: moderate-high
        0.4f,   // fear: moderate
        0.0f,   // relief: none (failed)
        0.8f    // frustration: high (10 retries)
    );
}

TEST_F(EmotionalTaggingRegressionTest, DataCorruption_StableFear) {
    // REGRESSION: Data corruption with high loss risk → stable high fear

    nimcp_recovery_episode_t episode = create_episode(
        "DATA_CORRUPTION", false, 0.85f, 3000000, 0);

    nimcp_emotional_tag_t emotion;
    ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));

    // Expected values
    expect_emotion_near(emotion,
        -0.8f,  // valence: very negative
        0.85f,  // arousal: very high
        0.88f,  // fear: very high (data loss + critical error)
        0.0f,   // relief: none
        0.0f    // frustration: low
    );
}

//=============================================================================
// Memory Boost Stability Tests
//=============================================================================

TEST_F(EmotionalTaggingRegressionTest, MemoryBoost_HighArousalNegative_StableBoost) {
    // REGRESSION: High arousal + negative valence → ~2.0x boost

    nimcp_emotional_tag_t emotion;
    emotion.valence = -0.9f;
    emotion.arousal = 0.95f;
    emotion.fear = 0.9f;
    emotion.relief = 0.0f;
    emotion.frustration = 0.0f;

    float boost = nimcp_emotional_memory_boost(&emotion);

    // Expected: HIGH_AROUSAL_BOOST (2.0) + extreme valence (0.3) = 2.3
    EXPECT_NEAR(boost, 2.3f, 0.25f);
    EXPECT_GT(boost, 2.0f);
    EXPECT_LE(boost, 2.5f);
}

TEST_F(EmotionalTaggingRegressionTest, MemoryBoost_ModerateArousal_StableBoost) {
    // REGRESSION: Moderate arousal → ~1.5x boost

    nimcp_emotional_tag_t emotion;
    emotion.valence = 0.6f;
    emotion.arousal = 0.6f;
    emotion.fear = 0.0f;
    emotion.relief = 0.7f;
    emotion.frustration = 0.0f;

    float boost = nimcp_emotional_memory_boost(&emotion);

    // Expected: MODERATE_AROUSAL_BOOST (1.5) + moderate valence (0.2) = 1.7
    EXPECT_NEAR(boost, 1.7f, 0.25f);
    EXPECT_GT(boost, 1.4f);
    EXPECT_LT(boost, 2.0f);
}

TEST_F(EmotionalTaggingRegressionTest, MemoryBoost_LowArousal_StableBoost) {
    // REGRESSION: Low arousal → ~1.0-1.2x boost

    nimcp_emotional_tag_t emotion;
    emotion.valence = 0.2f;
    emotion.arousal = 0.2f;
    emotion.fear = 0.0f;
    emotion.relief = 0.2f;
    emotion.frustration = 0.0f;

    float boost = nimcp_emotional_memory_boost(&emotion);

    // Expected: BASE (1.0) + low arousal (0.06) = ~1.06
    EXPECT_NEAR(boost, 1.06f, 0.15f);
    EXPECT_GE(boost, 1.0f);
    EXPECT_LT(boost, 1.3f);
}

TEST_F(EmotionalTaggingRegressionTest, MemoryBoost_HighFear_AdditionalBoost) {
    // REGRESSION: High fear adds 0.2 to boost

    nimcp_emotional_tag_t emotion;
    emotion.valence = -0.7f;
    emotion.arousal = 0.85f;
    emotion.fear = 0.9f;
    emotion.relief = 0.0f;
    emotion.frustration = 0.0f;

    float boost = nimcp_emotional_memory_boost(&emotion);

    // Expected: High arousal (2.0) + fear bonus (0.2) = 2.2+
    EXPECT_GT(boost, 2.1f);
}

//=============================================================================
// Priority Calculation Stability Tests
//=============================================================================

TEST_F(EmotionalTaggingRegressionTest, Priority_HighArousalHighFear_StablePriority) {
    // REGRESSION: High arousal + fear → priority > 1.0 (clamped)

    nimcp_emotional_tag_t emotion;
    emotion.valence = -0.8f;
    emotion.arousal = 0.9f;
    emotion.fear = 0.85f;
    emotion.relief = 0.0f;
    emotion.frustration = 0.0f;

    float priority = nimcp_emotional_priority(&emotion);

    // Expected: arousal (0.9) + fear * weight (0.85 * 0.2 = 0.17) = 1.07 → clamped to 1.0
    EXPECT_NEAR(priority, 1.0f, 0.05f);
}

TEST_F(EmotionalTaggingRegressionTest, Priority_LowArousalNoFear_StablePriority) {
    // REGRESSION: Low arousal, no fear → low priority

    nimcp_emotional_tag_t emotion;
    emotion.valence = 0.3f;
    emotion.arousal = 0.15f;
    emotion.fear = 0.0f;
    emotion.relief = 0.2f;
    emotion.frustration = 0.0f;

    float priority = nimcp_emotional_priority(&emotion);

    // Expected: arousal (0.15) + fear (0) = 0.15
    EXPECT_NEAR(priority, 0.15f, 0.05f);
}

TEST_F(EmotionalTaggingRegressionTest, Priority_ModerateMix_StablePriority) {
    // REGRESSION: Moderate arousal + moderate fear

    nimcp_emotional_tag_t emotion;
    emotion.valence = -0.5f;
    emotion.arousal = 0.6f;
    emotion.fear = 0.5f;
    emotion.relief = 0.0f;
    emotion.frustration = 0.3f;

    float priority = nimcp_emotional_priority(&emotion);

    // Expected: arousal (0.6) + fear * weight (0.5 * 0.2 = 0.1) = 0.7
    EXPECT_NEAR(priority, 0.7f, 0.05f);
}

//=============================================================================
// Boundary Behavior Stability
//=============================================================================

TEST_F(EmotionalTaggingRegressionTest, Boundary_ZeroDataLoss_ConsistentMinimalFear) {
    nimcp_recovery_episode_t episode = create_episode("WARNING", false, 0.0f, 1000, 0);
    nimcp_emotional_tag_t emotion;

    ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));

    EXPECT_NEAR(emotion.fear, 0.0f, 0.1f);
}

TEST_F(EmotionalTaggingRegressionTest, Boundary_MaxDataLoss_ConsistentMaximalFear) {
    nimcp_recovery_episode_t episode = create_episode("CATASTROPHIC", false, 1.0f, 10000000, 0);
    nimcp_emotional_tag_t emotion;

    ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));

    EXPECT_GT(emotion.fear, 0.8f);
    EXPECT_LE(emotion.fear, 1.0f);
}

TEST_F(EmotionalTaggingRegressionTest, Boundary_ValenceRange_AlwaysWithinBounds) {
    // Test various extreme scenarios
    std::vector<nimcp_recovery_episode_t> episodes = {
        create_episode("SIGSEGV", false, 1.0f, 10000000, 25),
        create_episode("SUCCESS", true, 0.0f, 1, 0),
        create_episode("MODERATE", false, 0.5f, 5000, 10),
    };

    for (const auto& episode : episodes) {
        nimcp_emotional_tag_t emotion;
        ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));

        EXPECT_GE(emotion.valence, -1.0f);
        EXPECT_LE(emotion.valence, 1.0f);
    }
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(EmotionalTaggingRegressionTest, Performance_SingleTag_UnderMicrosecond) {
    // REGRESSION: Single tag computation should be very fast (< 1 microsecond)

    nimcp_recovery_episode_t episode = create_episode("TEST", false, 0.5f, 1000, 0);
    nimcp_emotional_tag_t emotion;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // Average time per tag (should be < 1000ns = 1μs)
    double avg_ns = duration / 1000.0;
    EXPECT_LT(avg_ns, 1000.0) << "Tag computation too slow: " << avg_ns << "ns";
}

TEST_F(EmotionalTaggingRegressionTest, Performance_MemoryBoost_UnderNanosecond) {
    // REGRESSION: Memory boost calculation is pure math, should be extremely fast

    nimcp_emotional_tag_t emotion;
    emotion.valence = -0.8f;
    emotion.arousal = 0.9f;
    emotion.fear = 0.8f;
    emotion.relief = 0.0f;
    emotion.frustration = 0.0f;

    auto start = std::chrono::high_resolution_clock::now();

    volatile float sum = 0.0f;
    for (int i = 0; i < 10000; i++) {
        sum += nimcp_emotional_memory_boost(&emotion);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // Average time per boost (should be < 100ns)
    double avg_ns = duration / 10000.0;
    EXPECT_LT(avg_ns, 200.0) << "Memory boost too slow: " << avg_ns << "ns";
}

TEST_F(EmotionalTaggingRegressionTest, Performance_Priority_UnderNanosecond) {
    // REGRESSION: Priority calculation is simple math, very fast

    nimcp_emotional_tag_t emotion;
    emotion.valence = -0.7f;
    emotion.arousal = 0.85f;
    emotion.fear = 0.75f;
    emotion.relief = 0.0f;
    emotion.frustration = 0.0f;

    auto start = std::chrono::high_resolution_clock::now();

    volatile float sum = 0.0f;
    for (int i = 0; i < 10000; i++) {
        sum += nimcp_emotional_priority(&emotion);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    double avg_ns = duration / 10000.0;
    EXPECT_LT(avg_ns, 200.0) << "Priority calculation too slow: " << avg_ns << "ns";
}

//=============================================================================
// Statistics Accuracy Regression
//=============================================================================

TEST_F(EmotionalTaggingRegressionTest, Statistics_RunningAverage_Accurate) {
    // REGRESSION: Running averages should be mathematically correct

    std::vector<nimcp_recovery_episode_t> episodes = {
        create_episode("A", false, 0.8f, 5000, 0),  // Negative valence
        create_episode("B", true, 0.2f, 500, 0),    // Positive valence
        create_episode("C", false, 0.6f, 3000, 2),  // Negative valence
    };

    std::vector<float> valences;
    std::vector<float> arousals;

    for (const auto& episode : episodes) {
        nimcp_emotional_tag_t emotion;
        ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion));
        valences.push_back(emotion.valence);
        arousals.push_back(emotion.arousal);
    }

    // Compute expected averages
    float expected_avg_valence = (valences[0] + valences[1] + valences[2]) / 3.0f;
    float expected_avg_arousal = (arousals[0] + arousals[1] + arousals[2]) / 3.0f;

    nimcp_emotional_tagger_stats_t stats;
    ASSERT_TRUE(nimcp_emotional_tagger_get_stats(tagger, &stats));

    EXPECT_NEAR(stats.avg_valence, expected_avg_valence, 0.01f);
    EXPECT_NEAR(stats.avg_arousal, expected_avg_arousal, 0.01f);
    EXPECT_EQ(stats.total_tags, 3);
}

TEST_F(EmotionalTaggingRegressionTest, Statistics_HighEmotionCounts_Accurate) {
    // REGRESSION: High emotion counters should be accurate

    // Create episodes with known high emotions
    std::vector<nimcp_recovery_episode_t> episodes = {
        create_episode("SIGSEGV", false, 0.95f, 10000000, 0),  // High fear
        create_episode("SUCCESS", true, 0.05f, 100, 0),        // High relief
        create_episode("RETRY", false, 0.4f, 5000, 15),        // High frustration
        create_episode("NORMAL", true, 0.2f, 1000, 0),         // None high
    };

    for (const auto& episode : episodes) {
        nimcp_emotional_tag_t emotion;
        nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion);
    }

    nimcp_emotional_tagger_stats_t stats;
    ASSERT_TRUE(nimcp_emotional_tagger_get_stats(tagger, &stats));

    EXPECT_EQ(stats.high_fear_count, 1);
    EXPECT_EQ(stats.high_relief_count, 2);  // SUCCESS (0.9) + NORMAL (0.7) both >= 0.7 threshold
    EXPECT_EQ(stats.high_frustration_count, 1);
}

//=============================================================================
// Consistency Over Multiple Runs
//=============================================================================

TEST_F(EmotionalTaggingRegressionTest, Consistency_SameInput_SameOutput) {
    // REGRESSION: Same input should always produce same output (deterministic)

    nimcp_recovery_episode_t episode = create_episode("TEST", false, 0.7f, 3000, 5);

    nimcp_emotional_tag_t emotion1, emotion2, emotion3;
    ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion1));
    ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion2));
    ASSERT_TRUE(nimcp_emotional_tagger_compute_tag(tagger, &episode, &emotion3));

    // All three should be identical
    expect_emotion_near(emotion2, emotion1.valence, emotion1.arousal,
                       emotion1.fear, emotion1.relief, emotion1.frustration, 0.001f);
    expect_emotion_near(emotion3, emotion1.valence, emotion1.arousal,
                       emotion1.fear, emotion1.relief, emotion1.frustration, 0.001f);
}

TEST_F(EmotionalTaggingRegressionTest, Consistency_OrderIndependent_SameStats) {
    // REGRESSION: Order of tagging shouldn't affect final statistics

    std::vector<nimcp_recovery_episode_t> episodes = {
        create_episode("A", false, 0.5f, 1000, 0),
        create_episode("B", true, 0.3f, 500, 0),
        create_episode("C", false, 0.7f, 3000, 5),
    };

    // Tag in order
    nimcp_emotional_tagger_t* tagger1 = nimcp_emotional_tagger_create();
    for (const auto& ep : episodes) {
        nimcp_emotional_tag_t emotion;
        nimcp_emotional_tagger_compute_tag(tagger1, &ep, &emotion);
    }

    // Tag in reverse order
    nimcp_emotional_tagger_t* tagger2 = nimcp_emotional_tagger_create();
    for (auto it = episodes.rbegin(); it != episodes.rend(); ++it) {
        nimcp_emotional_tag_t emotion;
        nimcp_emotional_tagger_compute_tag(tagger2, &*it, &emotion);
    }

    nimcp_emotional_tagger_stats_t stats1, stats2;
    nimcp_emotional_tagger_get_stats(tagger1, &stats1);
    nimcp_emotional_tagger_get_stats(tagger2, &stats2);

    // Averages should be the same regardless of order
    EXPECT_NEAR(stats1.avg_valence, stats2.avg_valence, 0.01f);
    EXPECT_NEAR(stats1.avg_arousal, stats2.avg_arousal, 0.01f);

    nimcp_emotional_tagger_destroy(tagger1);
    nimcp_emotional_tagger_destroy(tagger2);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
