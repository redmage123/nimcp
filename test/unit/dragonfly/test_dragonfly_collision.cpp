/**
 * @file test_dragonfly_collision.cpp
 * @brief Unit tests for collision avoidance module
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly_collision.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CollisionTest : public ::testing::Test {
protected:
    dragonfly_collision_t collision = nullptr;

    void SetUp() override {
        collision = dragonfly_collision_create(nullptr);
        ASSERT_NE(collision, nullptr);
    }

    void TearDown() override {
        if (collision) {
            dragonfly_collision_destroy(collision);
            collision = nullptr;
        }
    }

    detected_obstacle_t make_obstacle(uint32_t id, float x, float y, float z, float size) {
        detected_obstacle_t obs = {};
        obs.id = id;
        obs.position[0] = x;
        obs.position[1] = y;
        obs.position[2] = z;
        obs.extent[0] = size;
        obs.extent[1] = size;
        obs.extent[2] = size;
        obs.type = OBSTACLE_STRUCTURE;
        obs.threat = THREAT_LOW;
        return obs;
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(CollisionTest, DefaultConfig) {
    collision_config_t config = collision_default_config();
    EXPECT_GT(config.detection_range_m, 0.0f);
    EXPECT_GT(config.min_clearance_m, 0.0f);
    EXPECT_GT(config.ttc_critical_threshold_s, 0.0f);
}

TEST_F(CollisionTest, ValidateConfig) {
    collision_config_t config = collision_default_config();
    EXPECT_TRUE(collision_validate_config(&config));

    config.detection_range_m = 0.0f;
    EXPECT_FALSE(collision_validate_config(&config));

    config = collision_default_config();
    config.ttc_critical_threshold_s = -1.0f;
    EXPECT_FALSE(collision_validate_config(&config));

    EXPECT_FALSE(collision_validate_config(nullptr));
}

TEST_F(CollisionTest, CreateWithCustomConfig) {
    collision_config_t config = collision_default_config();
    config.detection_range_m = 20.0f;
    config.min_clearance_m = 0.5f;

    dragonfly_collision_t custom = dragonfly_collision_create(&config);
    ASSERT_NE(custom, nullptr);
    dragonfly_collision_destroy(custom);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(CollisionTest, CreateAndDestroy) {
    dragonfly_collision_t c = dragonfly_collision_create(nullptr);
    ASSERT_NE(c, nullptr);
    dragonfly_collision_destroy(c);
}

TEST_F(CollisionTest, DestroyNull) {
    dragonfly_collision_destroy(nullptr);  // Should not crash
}

TEST_F(CollisionTest, Reset) {
    EXPECT_EQ(dragonfly_collision_reset(collision), 0);
}

//=============================================================================
// Obstacle Management Tests
//=============================================================================

TEST_F(CollisionTest, AddObstacle) {
    detected_obstacle_t obs = make_obstacle(1, 10, 0, 0, 1.0f);
    EXPECT_EQ(dragonfly_collision_add_obstacle(collision, &obs), 0);
}

TEST_F(CollisionTest, RemoveObstacle) {
    detected_obstacle_t obs = make_obstacle(1, 10, 0, 0, 1.0f);
    dragonfly_collision_add_obstacle(collision, &obs);
    EXPECT_EQ(dragonfly_collision_remove_obstacle(collision, 1), 0);
}

TEST_F(CollisionTest, ClearObstacles) {
    detected_obstacle_t obs1 = make_obstacle(1, 10, 0, 0, 1.0f);
    detected_obstacle_t obs2 = make_obstacle(2, 20, 0, 0, 1.5f);
    dragonfly_collision_add_obstacle(collision, &obs1);
    dragonfly_collision_add_obstacle(collision, &obs2);

    EXPECT_EQ(dragonfly_collision_clear(collision), 0);
}

//=============================================================================
// Collision Detection Tests
//=============================================================================

TEST_F(CollisionTest, NoCollisionWhenClear) {
    float self_pos[3] = {0, 0, 0};
    float self_vel[3] = {10, 0, 0};
    // No obstacles added

    collision_summary_t summary;
    EXPECT_EQ(dragonfly_collision_analyze(collision, self_pos, self_vel, &summary), 0);
    EXPECT_TRUE(summary.path_clear);
}

TEST_F(CollisionTest, DetectDirectCollision) {
    // Add obstacle directly ahead
    detected_obstacle_t obs = make_obstacle(1, 5, 0, 0, 1.0f);
    dragonfly_collision_add_obstacle(collision, &obs);

    // Moving towards obstacle
    float self_pos[3] = {0, 0, 0};
    float self_vel[3] = {15, 0, 0};

    collision_summary_t summary;
    dragonfly_collision_analyze(collision, self_pos, self_vel, &summary);

    EXPECT_FALSE(summary.path_clear);
    EXPECT_GT(summary.max_threat, THREAT_NONE);
}

TEST_F(CollisionTest, TTCCalculation) {
    // Obstacle at 10m, moving at 10m/s = 1 second TTC
    detected_obstacle_t obs = make_obstacle(1, 10, 0, 0, 1.0f);
    dragonfly_collision_add_obstacle(collision, &obs);

    float self_pos[3] = {0, 0, 0};
    float self_vel[3] = {10, 0, 0};

    collision_summary_t summary;
    dragonfly_collision_analyze(collision, self_pos, self_vel, &summary);

    EXPECT_GT(summary.min_ttc_s, 0.0f);
    EXPECT_LT(summary.min_ttc_s, 2.0f);  // Should be around 1 second
}

TEST_F(CollisionTest, NoCollisionWhenMovingAway) {
    // Obstacle behind
    detected_obstacle_t obs = make_obstacle(1, -10, 0, 0, 1.0f);
    dragonfly_collision_add_obstacle(collision, &obs);

    // Moving forward (away from obstacle)
    float self_pos[3] = {0, 0, 0};
    float self_vel[3] = {10, 0, 0};

    collision_summary_t summary;
    dragonfly_collision_analyze(collision, self_pos, self_vel, &summary);

    EXPECT_TRUE(summary.path_clear);
}

//=============================================================================
// Avoidance Planning Tests
//=============================================================================

TEST_F(CollisionTest, ComputeAvoidanceManeuver) {
    detected_obstacle_t obs = make_obstacle(1, 5, 0, 0, 1.0f);
    dragonfly_collision_add_obstacle(collision, &obs);

    float self_pos[3] = {0, 0, 0};
    float self_vel[3] = {10, 0, 0};
    float pursuit_dir[3] = {1, 0, 0};

    avoidance_command_t command;
    EXPECT_EQ(dragonfly_collision_get_avoidance(collision, self_pos, self_vel,
                                                  pursuit_dir, &command), 0);

    if (command.action != AVOID_NONE) {
        // Should have non-zero urgency
        EXPECT_GT(command.urgency, 0.0f);
    }
}

TEST_F(CollisionTest, FindSafeDirection) {
    detected_obstacle_t obs = make_obstacle(1, 5, 0, 0, 1.0f);
    dragonfly_collision_add_obstacle(collision, &obs);

    float self_pos[3] = {0, 0, 0};
    float preferred_dir[3] = {1, 0, 0};
    float safe_dir[3];

    EXPECT_EQ(dragonfly_collision_find_safe_direction(collision, self_pos,
                                                        preferred_dir, safe_dir), 0);

    // Safe direction should not point at obstacle
    // (dot product of safe direction and obstacle direction should be low)
}

//=============================================================================
// Emergency Stop Tests
//=============================================================================

TEST_F(CollisionTest, EmergencyStopRequired) {
    // Very close obstacle
    detected_obstacle_t obs = make_obstacle(1, 1, 0, 0, 0.5f);
    dragonfly_collision_add_obstacle(collision, &obs);

    float self_pos[3] = {0, 0, 0};
    float self_vel[3] = {15, 0, 0};

    collision_summary_t summary;
    dragonfly_collision_analyze(collision, self_pos, self_vel, &summary);

    // Should trigger high threat response
    EXPECT_GE(summary.max_threat, THREAT_HIGH);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(CollisionTest, GetStats) {
    collision_stats_t stats;
    EXPECT_EQ(dragonfly_collision_get_stats(collision, &stats), 0);
}

TEST_F(CollisionTest, StatsAfterOperations) {
    detected_obstacle_t obs = make_obstacle(1, 10, 0, 0, 1.0f);
    dragonfly_collision_add_obstacle(collision, &obs);

    collision_stats_t stats;
    dragonfly_collision_get_stats(collision, &stats);
    EXPECT_GE(stats.obstacles_detected, 1u);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(CollisionTest, NullPointerHandling) {
    detected_obstacle_t obs = make_obstacle(1, 10, 0, 0, 1.0f);
    float pos[3] = {0, 0, 0};
    float vel[3] = {1, 0, 0};
    collision_summary_t summary;

    EXPECT_EQ(dragonfly_collision_analyze(nullptr, pos, vel, &summary), -1);
    EXPECT_EQ(dragonfly_collision_analyze(collision, nullptr, vel, &summary), -1);
    EXPECT_EQ(dragonfly_collision_add_obstacle(nullptr, &obs), -1);
    EXPECT_EQ(dragonfly_collision_add_obstacle(collision, nullptr), -1);
    EXPECT_EQ(dragonfly_collision_get_stats(nullptr, nullptr), -1);
}
