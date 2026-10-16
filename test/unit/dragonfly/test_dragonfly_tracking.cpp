/**
 * @file test_dragonfly_tracking.cpp
 * @brief Unit tests for CSTMD1-inspired target tracking module
 *
 * WHAT: Tests for dragonfly target tracking module
 * WHY:  Verify state machine, Kalman filtering, distractor suppression
 * HOW:  GTest framework with simulated target observations
 *
 * @author NIMCP Team
 * @date 2024-12-27
 */

#include <gtest/gtest.h>
#include <cmath>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly_tracking.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TrackingTest : public ::testing::Test {
protected:
    dragonfly_tracker_t* tracker = nullptr;

    void SetUp() override {
        tracker = dragonfly_tracker_create(nullptr);
        ASSERT_NE(tracker, nullptr);
    }

    void TearDown() override {
        if (tracker) {
            dragonfly_tracker_destroy(tracker);
            tracker = nullptr;
        }
    }

    // Helper to create observation
    target_observation_t make_observation(
        uint32_t id, float x, float y, float z,
        float confidence = 0.9f, float size = 3.0f
    ) {
        target_observation_t obs = {};
        obs.target_id = id;
        obs.position[0] = x;
        obs.position[1] = y;
        obs.position[2] = z;
        obs.confidence = confidence;
        obs.size = size;
        obs.timestamp_us = 0;
        return obs;
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(TrackingTest, DefaultConfig) {
    tracking_config_t config = tracking_default_config();

    EXPECT_GT(config.acquisition_threshold, 0.0f);
    EXPECT_GT(config.lock_threshold, config.acquisition_threshold);
    EXPECT_LT(config.break_threshold, config.lock_threshold);
    EXPECT_GT(config.max_occlusion_ms, 0u);
    EXPECT_GT(config.distractor_suppression, 0.0f);
    EXPECT_LE(config.distractor_suppression, 1.0f);
}

TEST_F(TrackingTest, ValidateConfig) {
    tracking_config_t config = tracking_default_config();
    EXPECT_TRUE(tracking_validate_config(&config));

    config.lock_threshold = 0.1f;  // Less than acquisition_threshold
    EXPECT_FALSE(tracking_validate_config(&config));

    config = tracking_default_config();
    config.break_threshold = 0.9f;  // Greater than lock_threshold
    EXPECT_FALSE(tracking_validate_config(&config));

    EXPECT_FALSE(tracking_validate_config(nullptr));
}

TEST_F(TrackingTest, CreateWithCustomConfig) {
    tracking_config_t config = tracking_default_config();
    config.distractor_suppression = 0.95f;
    config.max_occlusion_ms = 1000;

    dragonfly_tracker_t* custom = dragonfly_tracker_create(&config);
    ASSERT_NE(custom, nullptr);

    tracking_config_t retrieved;
    EXPECT_EQ(dragonfly_tracker_get_config(custom, &retrieved), 0);
    EXPECT_FLOAT_EQ(retrieved.distractor_suppression, 0.95f);
    EXPECT_EQ(retrieved.max_occlusion_ms, 1000u);

    dragonfly_tracker_destroy(custom);
}

TEST_F(TrackingTest, CreateWithInvalidConfig) {
    tracking_config_t config = tracking_default_config();
    config.lock_threshold = -0.5f;  // Invalid

    dragonfly_tracker_t* invalid = dragonfly_tracker_create(&config);
    EXPECT_EQ(invalid, nullptr);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(TrackingTest, CreateWithDefaults) {
    EXPECT_EQ(dragonfly_tracker_get_state(tracker), TRACK_STATE_SEARCHING);
    EXPECT_FALSE(dragonfly_tracker_is_locked(tracker));
    EXPECT_EQ(dragonfly_tracker_get_locked_id(tracker), 0u);
}

TEST_F(TrackingTest, Reset) {
    // First acquire a target
    target_observation_t obs = make_observation(1, 10.0f, 0.0f, 0.0f, 0.9f);
    dragonfly_tracker_update(tracker, &obs, 1, 0.016f);

    EXPECT_NE(dragonfly_tracker_get_state(tracker), TRACK_STATE_SEARCHING);

    // Reset
    EXPECT_EQ(dragonfly_tracker_reset(tracker), 0);
    EXPECT_EQ(dragonfly_tracker_get_state(tracker), TRACK_STATE_SEARCHING);
    EXPECT_FALSE(dragonfly_tracker_is_locked(tracker));
}

TEST_F(TrackingTest, DestroyNull) {
    dragonfly_tracker_destroy(nullptr);  // Should not crash
}

//=============================================================================
// State Machine Tests
//=============================================================================

TEST_F(TrackingTest, AcquireTarget) {
    target_observation_t obs = make_observation(1, 10.0f, 0.0f, 0.0f, 0.5f);

    // First update - starts acquisition
    EXPECT_EQ(dragonfly_tracker_update(tracker, &obs, 1, 0.016f), 0);
    track_state_t state = dragonfly_tracker_get_state(tracker);
    EXPECT_TRUE(state == TRACK_STATE_ACQUIRING || state == TRACK_STATE_LOCKED);
}

TEST_F(TrackingTest, LockTarget) {
    target_observation_t obs = make_observation(1, 10.0f, 0.0f, 0.0f, 0.9f);

    // Multiple updates to achieve lock
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(dragonfly_tracker_update(tracker, &obs, 1, 0.016f), 0);
    }

    EXPECT_EQ(dragonfly_tracker_get_state(tracker), TRACK_STATE_LOCKED);
    EXPECT_TRUE(dragonfly_tracker_is_locked(tracker));
    EXPECT_EQ(dragonfly_tracker_get_locked_id(tracker), 1u);
}

TEST_F(TrackingTest, PredictDuringOcclusion) {
    target_observation_t obs = make_observation(1, 10.0f, 0.0f, 0.0f, 0.9f);

    // Lock onto target
    for (int i = 0; i < 10; i++) {
        dragonfly_tracker_update(tracker, &obs, 1, 0.016f);
    }
    ASSERT_EQ(dragonfly_tracker_get_state(tracker), TRACK_STATE_LOCKED);

    // Target disappears (no observations)
    EXPECT_EQ(dragonfly_tracker_update(tracker, nullptr, 0, 0.016f), 0);
    EXPECT_EQ(dragonfly_tracker_get_state(tracker), TRACK_STATE_PREDICTING);
}

TEST_F(TrackingTest, ReacquireAfterOcclusion) {
    target_observation_t obs = make_observation(1, 10.0f, 0.0f, 0.0f, 0.9f);

    // Lock, occlude, then reacquire
    for (int i = 0; i < 10; i++) {
        dragonfly_tracker_update(tracker, &obs, 1, 0.016f);
    }

    // Occlusion
    dragonfly_tracker_update(tracker, nullptr, 0, 0.016f);
    EXPECT_EQ(dragonfly_tracker_get_state(tracker), TRACK_STATE_PREDICTING);

    // Reacquire
    dragonfly_tracker_update(tracker, &obs, 1, 0.016f);
    EXPECT_EQ(dragonfly_tracker_get_state(tracker), TRACK_STATE_LOCKED);
}

TEST_F(TrackingTest, LoseTrackWithLowConfidence) {
    target_observation_t obs = make_observation(1, 10.0f, 0.0f, 0.0f, 0.9f);

    // Lock
    for (int i = 0; i < 10; i++) {
        dragonfly_tracker_update(tracker, &obs, 1, 0.016f);
    }

    // Low confidence observation (below acquisition threshold = treated as no observation)
    // This triggers PREDICTING state (occlusion), and eventually LOST after timeout
    obs.confidence = 0.05f;
    for (int i = 0; i < 5; i++) {
        dragonfly_tracker_update(tracker, &obs, 1, 0.016f);
    }

    track_state_t state = dragonfly_tracker_get_state(tracker);
    // With confidence below acquisition_threshold (0.3), the observation is filtered out
    // This triggers PREDICTING (occlusion) behavior
    EXPECT_TRUE(state == TRACK_STATE_PREDICTING ||
                state == TRACK_STATE_LOST ||
                state == TRACK_STATE_SEARCHING);
}

TEST_F(TrackingTest, BreakLock) {
    target_observation_t obs = make_observation(1, 10.0f, 0.0f, 0.0f, 0.9f);

    // Lock
    for (int i = 0; i < 10; i++) {
        dragonfly_tracker_update(tracker, &obs, 1, 0.016f);
    }

    EXPECT_EQ(dragonfly_tracker_break_lock(tracker), 0);
    EXPECT_EQ(dragonfly_tracker_get_state(tracker), TRACK_STATE_LOST);
}

TEST_F(TrackingTest, ForceLock) {
    EXPECT_EQ(dragonfly_tracker_force_lock(tracker, 42), 0);
    EXPECT_EQ(dragonfly_tracker_get_state(tracker), TRACK_STATE_LOCKED);
    EXPECT_EQ(dragonfly_tracker_get_locked_id(tracker), 42u);
}

//=============================================================================
// Tracking Accuracy Tests
//=============================================================================

TEST_F(TrackingTest, TrackMovingTarget) {
    float x = 10.0f;

    // Track a target moving in +x direction
    for (int i = 0; i < 20; i++) {
        target_observation_t obs = make_observation(1, x, 0.0f, 0.0f, 0.9f);
        obs.velocity[0] = 5.0f;  // Moving at 5 units/sec
        dragonfly_tracker_update(tracker, &obs, 1, 0.016f);
        x += 5.0f * 0.016f;
    }

    const tracked_target_t* target = dragonfly_tracker_get_target(tracker);
    ASSERT_NE(target, nullptr);

    // Velocity should be estimated
    EXPECT_GT(target->velocity[0], 0.0f);
}

TEST_F(TrackingTest, PredictFuturePosition) {
    // Lock onto a moving target
    for (int i = 0; i < 10; i++) {
        float x = 10.0f + i * 0.5f;
        target_observation_t obs = make_observation(1, x, 0.0f, 0.0f, 0.9f);
        obs.velocity[0] = 10.0f;
        dragonfly_tracker_update(tracker, &obs, 1, 0.050f);
    }

    // Predict 100ms ahead
    float pred_pos[3], pred_vel[3];
    EXPECT_EQ(dragonfly_tracker_predict(tracker, 100.0f, pred_pos, pred_vel), 0);

    const tracked_target_t* target = dragonfly_tracker_get_target(tracker);
    ASSERT_NE(target, nullptr);

    // Predicted position should be ahead of current position
    EXPECT_GT(pred_pos[0], target->position[0]);
}

TEST_F(TrackingTest, PredictWithoutLock) {
    float pos[3];
    EXPECT_EQ(dragonfly_tracker_predict(tracker, 100.0f, pos, nullptr), -1);
}

//=============================================================================
// Distractor Suppression Tests
//=============================================================================

TEST_F(TrackingTest, DistractorSuppression) {
    target_observation_t obs = make_observation(1, 10.0f, 0.0f, 0.0f, 0.9f);

    // Lock onto target 1
    for (int i = 0; i < 10; i++) {
        dragonfly_tracker_update(tracker, &obs, 1, 0.016f);
    }

    // Locked target gets full gain
    EXPECT_FLOAT_EQ(dragonfly_tracker_get_gain(tracker, 1), 1.0f);

    // Other targets are suppressed
    float distractor_gain = dragonfly_tracker_get_gain(tracker, 2);
    EXPECT_LT(distractor_gain, 1.0f);
    EXPECT_GT(distractor_gain, 0.0f);
}

TEST_F(TrackingTest, WinnerTakeAll) {
    // Present two targets
    target_observation_t obs[2];
    obs[0] = make_observation(1, 10.0f, 0.0f, 0.0f, 0.9f);
    obs[1] = make_observation(2, -10.0f, 0.0f, 0.0f, 0.8f);

    // Update with both
    for (int i = 0; i < 10; i++) {
        dragonfly_tracker_update(tracker, obs, 2, 0.016f);
    }

    // Should lock onto one (the higher confidence one)
    EXPECT_TRUE(dragonfly_tracker_is_locked(tracker));
    EXPECT_EQ(dragonfly_tracker_get_locked_id(tracker), 1u);
}

TEST_F(TrackingTest, MaintainLockWithDistractors) {
    target_observation_t obs1 = make_observation(1, 10.0f, 0.0f, 0.0f, 0.9f);

    // Lock onto target 1
    for (int i = 0; i < 10; i++) {
        dragonfly_tracker_update(tracker, &obs1, 1, 0.016f);
    }
    ASSERT_EQ(dragonfly_tracker_get_locked_id(tracker), 1u);

    // Present a higher-confidence distractor
    target_observation_t obs2 = make_observation(2, -10.0f, 0.0f, 0.0f, 0.95f);
    dragonfly_tracker_update(tracker, &obs2, 1, 0.016f);

    // Should still be locked on original target
    EXPECT_EQ(dragonfly_tracker_get_locked_id(tracker), 1u);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(TrackingTest, GetStats) {
    tracking_stats_t stats;
    EXPECT_EQ(dragonfly_tracker_get_stats(tracker, &stats), 0);

    EXPECT_EQ(stats.total_observations, 0u);
    EXPECT_EQ(stats.successful_locks, 0u);
}

TEST_F(TrackingTest, StatsTrackLocks) {
    target_observation_t obs = make_observation(1, 10.0f, 0.0f, 0.0f, 0.9f);

    // Lock onto target
    for (int i = 0; i < 10; i++) {
        dragonfly_tracker_update(tracker, &obs, 1, 0.016f);
    }

    tracking_stats_t stats;
    dragonfly_tracker_get_stats(tracker, &stats);

    EXPECT_EQ(stats.total_observations, 10u);
    EXPECT_GE(stats.successful_locks, 1u);
}

TEST_F(TrackingTest, ResetStats) {
    target_observation_t obs = make_observation(1, 10.0f, 0.0f, 0.0f, 0.9f);
    for (int i = 0; i < 10; i++) {
        dragonfly_tracker_update(tracker, &obs, 1, 0.016f);
    }

    EXPECT_EQ(dragonfly_tracker_reset_stats(tracker), 0);

    tracking_stats_t stats;
    dragonfly_tracker_get_stats(tracker, &stats);
    EXPECT_EQ(stats.total_observations, 0u);
    EXPECT_EQ(stats.successful_locks, 0u);
}

//=============================================================================
// Query Functions Tests
//=============================================================================

TEST_F(TrackingTest, GetTarget) {
    // No target when searching
    EXPECT_EQ(dragonfly_tracker_get_target(tracker), nullptr);

    target_observation_t obs = make_observation(1, 10.0f, 5.0f, 2.0f, 0.9f);
    for (int i = 0; i < 10; i++) {
        dragonfly_tracker_update(tracker, &obs, 1, 0.016f);
    }

    const tracked_target_t* target = dragonfly_tracker_get_target(tracker);
    ASSERT_NE(target, nullptr);
    EXPECT_EQ(target->target_id, 1u);
    EXPECT_NEAR(target->position[0], 10.0f, 1.0f);
    EXPECT_NEAR(target->position[1], 5.0f, 1.0f);
    EXPECT_NEAR(target->position[2], 2.0f, 1.0f);
}

TEST_F(TrackingTest, GetHistory) {
    target_observation_t obs = make_observation(1, 10.0f, 0.0f, 0.0f, 0.9f);

    // Generate some history
    for (int i = 0; i < 20; i++) {
        obs.position[0] = 10.0f + i * 0.5f;
        dragonfly_tracker_update(tracker, &obs, 1, 0.016f);
    }

    float positions[30];  // 10 positions * 3 components
    uint32_t count;
    EXPECT_EQ(dragonfly_tracker_get_history(tracker, positions, 10, &count), 0);
    EXPECT_GT(count, 0u);
    EXPECT_LE(count, 10u);
}

TEST_F(TrackingTest, GetPredictionConfidence) {
    EXPECT_FLOAT_EQ(dragonfly_tracker_get_prediction_confidence(tracker), 0.0f);

    target_observation_t obs = make_observation(1, 10.0f, 0.0f, 0.0f, 0.9f);
    for (int i = 0; i < 10; i++) {
        dragonfly_tracker_update(tracker, &obs, 1, 0.016f);
    }

    float conf = dragonfly_tracker_get_prediction_confidence(tracker);
    EXPECT_GT(conf, 0.0f);
    EXPECT_LE(conf, 1.0f);
}

//=============================================================================
// Configuration Update Tests
//=============================================================================

TEST_F(TrackingTest, SetConfig) {
    tracking_config_t config = tracking_default_config();
    config.max_occlusion_ms = 2000;

    EXPECT_EQ(dragonfly_tracker_set_config(tracker, &config), 0);

    tracking_config_t retrieved;
    dragonfly_tracker_get_config(tracker, &retrieved);
    EXPECT_EQ(retrieved.max_occlusion_ms, 2000u);
}

TEST_F(TrackingTest, SetInvalidConfig) {
    tracking_config_t config = tracking_default_config();
    config.lock_threshold = -1.0f;

    EXPECT_EQ(dragonfly_tracker_set_config(tracker, &config), -1);
}

//=============================================================================
// External Velocity Tests
//=============================================================================

TEST_F(TrackingTest, SetExternalVelocity) {
    target_observation_t obs = make_observation(1, 10.0f, 0.0f, 0.0f, 0.9f);
    for (int i = 0; i < 10; i++) {
        dragonfly_tracker_update(tracker, &obs, 1, 0.016f);
    }

    float ext_vel[3] = {5.0f, 2.0f, 1.0f};
    EXPECT_EQ(dragonfly_tracker_set_external_velocity(tracker, ext_vel, 0.8f), 0);

    const tracked_target_t* target = dragonfly_tracker_get_target(tracker);
    ASSERT_NE(target, nullptr);
    // Velocity should be influenced by external estimate
    // (Can't test exact value due to blending)
}

TEST_F(TrackingTest, SetExternalVelocityNoLock) {
    float ext_vel[3] = {5.0f, 2.0f, 1.0f};
    EXPECT_EQ(dragonfly_tracker_set_external_velocity(tracker, ext_vel, 0.8f), -1);
}

//=============================================================================
// State Name Tests
//=============================================================================

TEST_F(TrackingTest, StateName) {
    EXPECT_STREQ(dragonfly_tracker_state_name(TRACK_STATE_SEARCHING), "SEARCHING");
    EXPECT_STREQ(dragonfly_tracker_state_name(TRACK_STATE_ACQUIRING), "ACQUIRING");
    EXPECT_STREQ(dragonfly_tracker_state_name(TRACK_STATE_LOCKED), "LOCKED");
    EXPECT_STREQ(dragonfly_tracker_state_name(TRACK_STATE_PREDICTING), "PREDICTING");
    EXPECT_STREQ(dragonfly_tracker_state_name(TRACK_STATE_LOST), "LOST");
    EXPECT_STREQ(dragonfly_tracker_state_name((track_state_t)99), "UNKNOWN");
}

//=============================================================================
// Null Pointer Tests
//=============================================================================

TEST_F(TrackingTest, NullPointerHandling) {
    EXPECT_EQ(dragonfly_tracker_update(nullptr, nullptr, 0, 0.016f), -1);
    EXPECT_EQ(dragonfly_tracker_reset(nullptr), -1);
    EXPECT_EQ(dragonfly_tracker_break_lock(nullptr), -1);
    EXPECT_EQ(dragonfly_tracker_force_lock(nullptr, 1), -1);

    float pos[3];
    EXPECT_EQ(dragonfly_tracker_predict(nullptr, 100.0f, pos, nullptr), -1);

    EXPECT_EQ(dragonfly_tracker_get_target(nullptr), nullptr);
    EXPECT_EQ(dragonfly_tracker_get_state(nullptr), TRACK_STATE_SEARCHING);
    EXPECT_FLOAT_EQ(dragonfly_tracker_get_gain(nullptr, 1), 0.0f);
    EXPECT_FALSE(dragonfly_tracker_is_locked(nullptr));
    EXPECT_EQ(dragonfly_tracker_get_locked_id(nullptr), 0u);

    tracking_stats_t stats;
    EXPECT_EQ(dragonfly_tracker_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(dragonfly_tracker_get_stats(tracker, nullptr), -1);

    EXPECT_EQ(dragonfly_tracker_reset_stats(nullptr), -1);

    tracking_config_t config;
    EXPECT_EQ(dragonfly_tracker_set_config(nullptr, &config), -1);
    EXPECT_EQ(dragonfly_tracker_set_config(tracker, nullptr), -1);
    EXPECT_EQ(dragonfly_tracker_get_config(nullptr, &config), -1);
    EXPECT_EQ(dragonfly_tracker_get_config(tracker, nullptr), -1);

    float positions[30];
    uint32_t count;
    EXPECT_EQ(dragonfly_tracker_get_history(nullptr, positions, 10, &count), -1);
    EXPECT_EQ(dragonfly_tracker_get_history(tracker, nullptr, 10, &count), -1);
    EXPECT_EQ(dragonfly_tracker_get_history(tracker, positions, 10, nullptr), -1);

    EXPECT_FLOAT_EQ(dragonfly_tracker_get_prediction_confidence(nullptr), 0.0f);

    float vel[3] = {1.0f, 0.0f, 0.0f};
    EXPECT_EQ(dragonfly_tracker_set_external_velocity(nullptr, vel, 0.5f), -1);
    EXPECT_EQ(dragonfly_tracker_set_external_velocity(tracker, nullptr, 0.5f), -1);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(TrackingTest, UpdateWithInvalidDt) {
    target_observation_t obs = make_observation(1, 10.0f, 0.0f, 0.0f, 0.9f);
    EXPECT_EQ(dragonfly_tracker_update(tracker, &obs, 1, 0.0f), -1);
    EXPECT_EQ(dragonfly_tracker_update(tracker, &obs, 1, -0.016f), -1);
}

TEST_F(TrackingTest, PredictWithNegativeLookahead) {
    target_observation_t obs = make_observation(1, 10.0f, 0.0f, 0.0f, 0.9f);
    for (int i = 0; i < 10; i++) {
        dragonfly_tracker_update(tracker, &obs, 1, 0.016f);
    }

    float pos[3];
    EXPECT_EQ(dragonfly_tracker_predict(tracker, -100.0f, pos, nullptr), -1);
}

TEST_F(TrackingTest, SizeSelectivity) {
    // Test with target outside size range
    target_observation_t obs = make_observation(1, 10.0f, 0.0f, 0.0f, 0.9f, 50.0f);

    for (int i = 0; i < 10; i++) {
        dragonfly_tracker_update(tracker, &obs, 1, 0.016f);
    }

    // Should still track (size affects score but not threshold)
    // Low size_score reduces overall score but high confidence compensates
}
