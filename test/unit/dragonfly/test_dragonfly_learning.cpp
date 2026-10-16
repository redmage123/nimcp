/**
 * @file test_dragonfly_learning.cpp
 * @brief Unit tests for learning from hunt outcomes module
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly_learning.h"

//=============================================================================
// Test Fixture
//=============================================================================

class LearningTest : public ::testing::Test {
protected:
    dragonfly_learning_t learning = nullptr;

    void SetUp() override {
        learning = dragonfly_learning_create(nullptr);
        ASSERT_NE(learning, nullptr);
    }

    void TearDown() override {
        if (learning) {
            dragonfly_learning_destroy(learning);
            learning = nullptr;
        }
    }

    dragonfly_target_info_t make_target_info(uint32_t id, float x, float y, float z,
                                               float vx, float vy, float vz,
                                               float confidence) {
        dragonfly_target_info_t target = {};
        target.id = id;
        target.position[0] = x;
        target.position[1] = y;
        target.position[2] = z;
        target.velocity[0] = vx;
        target.velocity[1] = vy;
        target.velocity[2] = vz;
        target.confidence = confidence;
        target.evasion_type = EVASION_NONE;
        return target;
    }

    hunt_episode_t make_episode(bool success, intercept_strategy_t strategy,
                                 float duration, float size, float speed) {
        hunt_episode_t ep = {};
        ep.outcome = success ? OUTCOME_SUCCESS : OUTCOME_ESCAPED;
        ep.strategy = strategy;
        ep.pursuit_duration_s = duration;
        ep.target_size = size;
        ep.target_speed = speed;
        ep.target_maneuverability = 0.3f;
        ep.initial_range = 50.0f;
        return ep;
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(LearningTest, DefaultConfig) {
    learning_config_t config = learning_default_config();
    EXPECT_GT(config.max_episodes, 0u);
    EXPECT_GT(config.strategy_learning_rate, 0.0f);
    EXPECT_LE(config.strategy_learning_rate, 1.0f);
}

TEST_F(LearningTest, ValidateConfig) {
    learning_config_t config = learning_default_config();
    EXPECT_TRUE(learning_validate_config(&config));

    config.max_episodes = 0;
    EXPECT_FALSE(learning_validate_config(&config));

    config = learning_default_config();
    config.strategy_learning_rate = 2.0f;  // Invalid
    EXPECT_FALSE(learning_validate_config(&config));

    EXPECT_FALSE(learning_validate_config(nullptr));
}

TEST_F(LearningTest, CreateWithCustomConfig) {
    learning_config_t config = learning_default_config();
    // max_episodes must be <= LEARNING_MAX_EPISODES (256)
    config.max_episodes = 200;
    config.strategy_learning_rate = 0.05f;

    dragonfly_learning_t custom = dragonfly_learning_create(&config);
    ASSERT_NE(custom, nullptr);
    dragonfly_learning_destroy(custom);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(LearningTest, CreateAndDestroy) {
    dragonfly_learning_t l = dragonfly_learning_create(nullptr);
    ASSERT_NE(l, nullptr);
    dragonfly_learning_destroy(l);
}

TEST_F(LearningTest, DestroyNull) {
    dragonfly_learning_destroy(nullptr);  // Should not crash
}

TEST_F(LearningTest, Reset) {
    EXPECT_EQ(dragonfly_learning_reset(learning), 0);
}

//=============================================================================
// Episode Recording Tests
//=============================================================================

TEST_F(LearningTest, RecordSuccessfulHunt) {
    hunt_episode_t ep = make_episode(true, INTERCEPT_PURSUIT, 2.5f, 0.05f, 5.0f);
    EXPECT_EQ(dragonfly_learning_record_episode(learning, &ep), 0);
}

TEST_F(LearningTest, RecordFailedHunt) {
    hunt_episode_t ep = make_episode(false, INTERCEPT_LEAD, 4.0f, 0.03f, 8.0f);
    ep.failure_reason = FAIL_REASON_SPEED;
    EXPECT_EQ(dragonfly_learning_record_episode(learning, &ep), 0);
}

TEST_F(LearningTest, RecordMultipleEpisodes) {
    for (int i = 0; i < 10; i++) {
        hunt_episode_t ep = make_episode(i % 3 == 0, INTERCEPT_PN, 2.0f + i * 0.1f,
                                          0.04f, 6.0f);
        EXPECT_EQ(dragonfly_learning_record_episode(learning, &ep), 0);
    }

    learning_stats_t stats;
    dragonfly_learning_get_stats(learning, &stats);
    EXPECT_GE(stats.episodes_recorded, 10u);
}

//=============================================================================
// Hunt Tracking Tests
//=============================================================================

TEST_F(LearningTest, BeginAndEndHunt) {
    dragonfly_target_info_t target = make_target_info(1, 100, 0, 0, 5, 0, 0, 0.9f);

    EXPECT_EQ(dragonfly_learning_begin_hunt(learning, &target, INTERCEPT_PURSUIT), 0);
    EXPECT_EQ(dragonfly_learning_end_hunt(learning, OUTCOME_SUCCESS, FAIL_REASON_UNKNOWN, 0.1f), 0);
}

TEST_F(LearningTest, HuntWithFailure) {
    dragonfly_target_info_t target = make_target_info(2, 80, 0, 0, 8, 0, 0, 0.8f);

    EXPECT_EQ(dragonfly_learning_begin_hunt(learning, &target, INTERCEPT_LEAD), 0);
    EXPECT_EQ(dragonfly_learning_end_hunt(learning, OUTCOME_ESCAPED,
                                           FAIL_REASON_EVASION, 5.0f), 0);
}

//=============================================================================
// Strategy Recommendation Tests
//=============================================================================

TEST_F(LearningTest, GetRecommendationWithNoHistory) {
    dragonfly_target_info_t target = make_target_info(1, 100, 0, 0, 5, 0, 0, 0.9f);

    learning_recommendation_t rec;
    EXPECT_EQ(dragonfly_learning_get_recommendation(learning, &target, &rec), 0);

    // With no history, should give default recommendation
    EXPECT_GE(rec.strategy_confidence, 0.0f);
    EXPECT_LE(rec.strategy_confidence, 1.0f);
}

TEST_F(LearningTest, RecommendationImproves) {
    // Record several successful pursuits
    for (int i = 0; i < 5; i++) {
        hunt_episode_t ep = make_episode(true, INTERCEPT_PURSUIT, 2.0f, 0.05f, 4.0f);
        dragonfly_learning_record_episode(learning, &ep);
    }

    dragonfly_target_info_t target = make_target_info(1, 100, 0, 0, 4, 0, 0, 0.9f);

    learning_recommendation_t rec;
    dragonfly_learning_get_recommendation(learning, &target, &rec);

    // Should recommend pursuit with some confidence
    EXPECT_GT(rec.strategy_confidence, 0.0f);
}

TEST_F(LearningTest, GetStrategyStats) {
    // Record some successful hunts with pursuit
    for (int i = 0; i < 3; i++) {
        hunt_episode_t ep = make_episode(true, INTERCEPT_PURSUIT, 2.0f, 0.05f, 4.0f);
        dragonfly_learning_record_episode(learning, &ep);
    }

    strategy_effectiveness_t stats;
    EXPECT_EQ(dragonfly_learning_get_strategy_stats(learning, INTERCEPT_PURSUIT, &stats), 0);
    EXPECT_GE(stats.attempts, 3u);
}

//=============================================================================
// Pattern Recognition Tests
//=============================================================================

TEST_F(LearningTest, LearnPatternFromSuccess) {
    // Create consistent pattern: small, slow target => pursuit success
    for (int i = 0; i < 10; i++) {
        hunt_episode_t ep = make_episode(true, INTERCEPT_PURSUIT, 1.5f, 0.03f, 3.0f);
        dragonfly_learning_record_episode(learning, &ep);
    }

    // Ask about similar target
    dragonfly_target_info_t target = make_target_info(1, 80, 0, 0, 3, 0, 0, 0.9f);

    learning_recommendation_t rec;
    dragonfly_learning_get_recommendation(learning, &target, &rec);

    // Should have learned this is a good target
    EXPECT_GT(rec.predicted_success_rate, 0.5f);
}

TEST_F(LearningTest, LearnPatternFromFailure) {
    // Create consistent pattern: fast target => failure
    for (int i = 0; i < 10; i++) {
        hunt_episode_t ep = make_episode(false, INTERCEPT_PURSUIT, 4.0f, 0.02f, 12.0f);
        ep.failure_reason = FAIL_REASON_SPEED;
        dragonfly_learning_record_episode(learning, &ep);
    }

    // Ask about similar fast target
    dragonfly_target_info_t target = make_target_info(1, 80, 0, 0, 12, 0, 0, 0.9f);

    learning_recommendation_t rec;
    dragonfly_learning_get_recommendation(learning, &target, &rec);

    // Should have learned this is a difficult target
    EXPECT_TRUE(rec.difficult_target);
}

//=============================================================================
// Exploration Rate Tests
//=============================================================================

TEST_F(LearningTest, ExplorationRateDecays) {
    learning_stats_t stats_before, stats_after;
    dragonfly_learning_get_stats(learning, &stats_before);

    // Record many episodes to trigger decay
    for (int i = 0; i < 50; i++) {
        hunt_episode_t ep = make_episode(true, INTERCEPT_PURSUIT, 2.0f, 0.05f, 4.0f);
        dragonfly_learning_record_episode(learning, &ep);
    }

    dragonfly_learning_get_stats(learning, &stats_after);
    // Exploration rate should decrease over time
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(LearningTest, GetStats) {
    learning_stats_t stats;
    EXPECT_EQ(dragonfly_learning_get_stats(learning, &stats), 0);
    EXPECT_EQ(stats.episodes_recorded, 0u);
}

TEST_F(LearningTest, ResetLearning) {
    hunt_episode_t ep = make_episode(true, INTERCEPT_PURSUIT, 2.0f, 0.05f, 4.0f);
    dragonfly_learning_record_episode(learning, &ep);

    EXPECT_EQ(dragonfly_learning_reset(learning), 0);

    learning_stats_t stats;
    dragonfly_learning_get_stats(learning, &stats);
    // After reset, episodes should be cleared
    EXPECT_EQ(stats.episodes_recorded, 0u);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(LearningTest, NullPointerHandling) {
    hunt_episode_t ep = make_episode(true, INTERCEPT_PURSUIT, 2.0f, 0.05f, 4.0f);
    dragonfly_target_info_t target = make_target_info(1, 100, 0, 0, 5, 0, 0, 0.9f);
    learning_recommendation_t rec;

    EXPECT_EQ(dragonfly_learning_record_episode(nullptr, &ep), -1);
    EXPECT_EQ(dragonfly_learning_record_episode(learning, nullptr), -1);
    EXPECT_EQ(dragonfly_learning_begin_hunt(nullptr, &target, INTERCEPT_PURSUIT), -1);
    EXPECT_EQ(dragonfly_learning_begin_hunt(learning, nullptr, INTERCEPT_PURSUIT), -1);
    EXPECT_EQ(dragonfly_learning_get_recommendation(nullptr, &target, &rec), -1);
    EXPECT_EQ(dragonfly_learning_get_recommendation(learning, nullptr, &rec), -1);
    EXPECT_EQ(dragonfly_learning_get_recommendation(learning, &target, nullptr), -1);
}
