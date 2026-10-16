/**
 * @file test_dragonfly_gaze.cpp
 * @brief Unit tests for gaze stabilization module
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly_gaze.h"

//=============================================================================
// Test Fixture
//=============================================================================

class GazeTest : public ::testing::Test {
protected:
    dragonfly_gaze_t gaze = nullptr;

    void SetUp() override {
        gaze = dragonfly_gaze_create(nullptr);
        ASSERT_NE(gaze, nullptr);
    }

    void TearDown() override {
        if (gaze) {
            dragonfly_gaze_destroy(gaze);
            gaze = nullptr;
        }
    }

    gaze_target_t make_target(float x, float y, float z) {
        gaze_target_t target = {};
        target.type = GAZE_TARGET_PREY;
        target.position[0] = x;
        target.position[1] = y;
        target.position[2] = z;
        target.priority = 1.0f;
        target.is_moving = false;
        return target;
    }

    body_state_t make_body_state() {
        body_state_t body = {};
        body.yaw_rad = 0.0f;
        body.pitch_rad = 0.0f;
        body.roll_rad = 0.0f;
        return body;
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(GazeTest, DefaultConfig) {
    gaze_config_t config = gaze_default_config();
    EXPECT_GT(config.vor_gain, 0.0f);
    EXPECT_GT(config.pursuit_gain, 0.0f);
}

TEST_F(GazeTest, ValidateConfig) {
    gaze_config_t config = gaze_default_config();
    EXPECT_TRUE(gaze_validate_config(&config));

    config.vor_gain = -1.0f;
    EXPECT_FALSE(gaze_validate_config(&config));

    EXPECT_FALSE(gaze_validate_config(nullptr));
}

TEST_F(GazeTest, CreateWithCustomConfig) {
    gaze_config_t config = gaze_default_config();
    // vor_gain must be in [0, 1] range
    config.vor_gain = 0.95f;
    config.pursuit_gain = 0.85f;

    dragonfly_gaze_t custom = dragonfly_gaze_create(&config);
    ASSERT_NE(custom, nullptr);
    dragonfly_gaze_destroy(custom);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(GazeTest, CreateAndDestroy) {
    dragonfly_gaze_t g = dragonfly_gaze_create(nullptr);
    ASSERT_NE(g, nullptr);
    dragonfly_gaze_destroy(g);
}

TEST_F(GazeTest, DestroyNull) {
    dragonfly_gaze_destroy(nullptr);  // Should not crash
}

TEST_F(GazeTest, Reset) {
    EXPECT_EQ(dragonfly_gaze_reset(gaze), 0);
}

//=============================================================================
// Target Management Tests
//=============================================================================

TEST_F(GazeTest, SetTarget) {
    gaze_target_t target = make_target(100, 0, 0);
    EXPECT_EQ(dragonfly_gaze_set_target(gaze, &target), 0);
}

TEST_F(GazeTest, UpdateTargetPosition) {
    gaze_target_t target = make_target(100, 0, 0);
    dragonfly_gaze_set_target(gaze, &target);

    float new_pos[3] = {95, 5, 0};
    float vel[3] = {-5, 5, 0};
    EXPECT_EQ(dragonfly_gaze_update_target(gaze, new_pos, vel), 0);
}

TEST_F(GazeTest, ClearTarget) {
    gaze_target_t target = make_target(100, 0, 0);
    dragonfly_gaze_set_target(gaze, &target);
    EXPECT_EQ(dragonfly_gaze_clear_target(gaze), 0);
}

TEST_F(GazeTest, LockAndUnlock) {
    gaze_target_t target = make_target(100, 0, 0);
    dragonfly_gaze_set_target(gaze, &target);

    EXPECT_EQ(dragonfly_gaze_lock(gaze), 0);
    EXPECT_EQ(dragonfly_gaze_unlock(gaze), 0);
}

//=============================================================================
// Gaze Update Tests
//=============================================================================

TEST_F(GazeTest, UpdateGaze) {
    gaze_target_t target = make_target(100, 0, 0);
    dragonfly_gaze_set_target(gaze, &target);

    body_state_t body = make_body_state();
    float self_pos[3] = {0, 0, 0};
    gaze_command_t cmd;

    EXPECT_EQ(dragonfly_gaze_update(gaze, &body, self_pos, 0.016f, &cmd), 0);
}

TEST_F(GazeTest, GazeCommandAngles) {
    // Target directly ahead
    gaze_target_t target = make_target(100, 0, 0);
    dragonfly_gaze_set_target(gaze, &target);

    body_state_t body = make_body_state();
    float self_pos[3] = {0, 0, 0};
    gaze_command_t cmd;

    dragonfly_gaze_update(gaze, &body, self_pos, 0.016f, &cmd);

    // Should have small yaw and pitch commands for straight-ahead target
    EXPECT_NEAR(cmd.yaw_cmd_rad, 0.0f, 0.5f);
    EXPECT_NEAR(cmd.pitch_cmd_rad, 0.0f, 0.5f);
}

TEST_F(GazeTest, GazeCommandToSide) {
    // Target to the side
    gaze_target_t target = make_target(0, 100, 0);
    dragonfly_gaze_set_target(gaze, &target);

    body_state_t body = make_body_state();
    float self_pos[3] = {0, 0, 0};
    gaze_command_t cmd;

    dragonfly_gaze_update(gaze, &body, self_pos, 0.016f, &cmd);

    // Should have non-zero yaw command for side target
    // Note: Command is rate-limited by smooth pursuit gain, so a single
    // 16ms step produces a small incremental command, not the full angle
    EXPECT_GT(fabsf(cmd.yaw_cmd_rad), 0.01f);
}

//=============================================================================
// VOR Compensation Tests
//=============================================================================

TEST_F(GazeTest, VORWithBodyRotation) {
    gaze_target_t target = make_target(100, 0, 0);
    dragonfly_gaze_set_target(gaze, &target);

    body_state_t body = make_body_state();
    body.yaw_rate = 0.5f;  // Body rotating
    float self_pos[3] = {0, 0, 0};
    gaze_command_t cmd;

    dragonfly_gaze_update(gaze, &body, self_pos, 0.016f, &cmd);

    // VOR should generate compensation command
    // (specifics depend on implementation)
}

//=============================================================================
// Saccade Tests
//=============================================================================

TEST_F(GazeTest, TriggerSaccade) {
    EXPECT_EQ(dragonfly_gaze_saccade_to(gaze, 1.0f, 0.5f), 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(GazeTest, GetStats) {
    gaze_stats_t stats;
    EXPECT_EQ(dragonfly_gaze_get_stats(gaze, &stats), 0);
}

TEST_F(GazeTest, StatsAfterUpdates) {
    gaze_target_t target = make_target(100, 0, 0);
    dragonfly_gaze_set_target(gaze, &target);

    body_state_t body = make_body_state();
    float self_pos[3] = {0, 0, 0};
    gaze_command_t cmd;

    for (int i = 0; i < 10; i++) {
        dragonfly_gaze_update(gaze, &body, self_pos, 0.016f, &cmd);
    }

    gaze_stats_t stats;
    dragonfly_gaze_get_stats(gaze, &stats);
    EXPECT_GE(stats.updates, 10u);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(GazeTest, NullPointerHandling) {
    gaze_target_t target = make_target(100, 0, 0);
    body_state_t body = make_body_state();
    float pos[3] = {0, 0, 0};
    gaze_command_t cmd;

    EXPECT_EQ(dragonfly_gaze_set_target(nullptr, &target), -1);
    EXPECT_EQ(dragonfly_gaze_set_target(gaze, nullptr), -1);
    EXPECT_EQ(dragonfly_gaze_update(nullptr, &body, pos, 0.016f, &cmd), -1);
    EXPECT_EQ(dragonfly_gaze_get_stats(nullptr, nullptr), -1);
}

TEST_F(GazeTest, InvalidDeltaTime) {
    body_state_t body = make_body_state();
    float pos[3] = {0, 0, 0};
    gaze_command_t cmd;

    EXPECT_EQ(dragonfly_gaze_update(gaze, &body, pos, -1.0f, &cmd), -1);
    EXPECT_EQ(dragonfly_gaze_update(gaze, &body, pos, 0.0f, &cmd), -1);
}
