/**
 * @file test_dragonfly_parietal_bridge.cpp
 * @brief Unit tests for Dragonfly Parietal Bridge module
 *
 * Tests spatial coordinate transforms, attention maps, and visuomotor commands.
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly_parietal_bridge.h"
#include "dragonfly/nimcp_dragonfly.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class ParietalBridgeTest : public ::testing::Test {
protected:
    dragonfly_parietal_bridge_t* bridge = nullptr;
    dragonfly_system_t* dragonfly = nullptr;

    void SetUp() override {
        dragonfly = dragonfly_system_create(nullptr);
        bridge = dragonfly_parietal_bridge_create(dragonfly, nullptr, nullptr, nullptr);
    }

    void TearDown() override {
        dragonfly_parietal_bridge_destroy(bridge);
        dragonfly_system_destroy(dragonfly);
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(ParietalBridgeTest, DefaultConfig) {
    parietal_bridge_config_t config = parietal_bridge_default_config();

    EXPECT_TRUE(config.auto_transform);
    EXPECT_EQ(config.default_output_frame, COORD_FRAME_BODY);
    EXPECT_TRUE(config.enable_attention);
    EXPECT_GT(config.attention_map_width, 0u);
    EXPECT_GT(config.attention_map_height, 0u);
    EXPECT_TRUE(config.generate_motor_commands);
    EXPECT_GT(config.saccade_threshold, 0);
    EXPECT_GT(config.pursuit_gain, 0);
}

TEST_F(ParietalBridgeTest, ValidateConfig) {
    parietal_bridge_config_t config = parietal_bridge_default_config();
    EXPECT_TRUE(parietal_bridge_validate_config(&config));

    // Invalid attention map size
    config.attention_map_width = 0;
    EXPECT_FALSE(parietal_bridge_validate_config(&config));
    config = parietal_bridge_default_config();

    // Invalid decay
    config.attention_decay = 1.5f;
    EXPECT_FALSE(parietal_bridge_validate_config(&config));
    config = parietal_bridge_default_config();

    // Invalid gain field sigma
    config.gain_field_sigma = -1.0f;
    EXPECT_FALSE(parietal_bridge_validate_config(&config));
}

TEST_F(ParietalBridgeTest, ValidateNullConfig) {
    EXPECT_FALSE(parietal_bridge_validate_config(nullptr));
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(ParietalBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(ParietalBridgeTest, CreateWithNullDragonfly) {
    dragonfly_parietal_bridge_t* b = dragonfly_parietal_bridge_create(nullptr, nullptr, nullptr, nullptr);
    ASSERT_NE(b, nullptr);
    dragonfly_parietal_bridge_destroy(b);
}

TEST_F(ParietalBridgeTest, CreateWithCustomConfig) {
    parietal_bridge_config_t config = parietal_bridge_default_config();
    config.enable_attention = false;
    config.motor_latency_ms = 200.0f;

    dragonfly_parietal_bridge_t* b = dragonfly_parietal_bridge_create(dragonfly, nullptr, nullptr, &config);
    ASSERT_NE(b, nullptr);

    parietal_bridge_config_t retrieved;
    EXPECT_EQ(dragonfly_parietal_bridge_get_config(b, &retrieved), 0);
    EXPECT_FALSE(retrieved.enable_attention);
    EXPECT_FLOAT_EQ(retrieved.motor_latency_ms, 200.0f);

    dragonfly_parietal_bridge_destroy(b);
}

TEST_F(ParietalBridgeTest, CreateWithInvalidConfig) {
    parietal_bridge_config_t config = parietal_bridge_default_config();
    config.attention_map_height = 0;

    dragonfly_parietal_bridge_t* b = dragonfly_parietal_bridge_create(dragonfly, nullptr, nullptr, &config);
    EXPECT_EQ(b, nullptr);
}

TEST_F(ParietalBridgeTest, Reset) {
    EXPECT_EQ(dragonfly_parietal_bridge_reset(bridge), 0);
}

//=============================================================================
// Observer State Tests
//=============================================================================

TEST_F(ParietalBridgeTest, SetGetObserver) {
    observer_state_t obs = {};
    obs.position = {1.0f, 2.0f, 3.0f};
    obs.orientation = {1.0f, 0.0f, 0.0f, 0.0f};  // Identity
    obs.heading = 0.5f;
    obs.pitch = 0.1f;
    obs.roll = 0.0f;
    obs.frame = COORD_FRAME_WORLD;

    EXPECT_EQ(dragonfly_parietal_bridge_set_observer(bridge, &obs), 0);

    observer_state_t retrieved;
    EXPECT_EQ(dragonfly_parietal_bridge_get_observer(bridge, &retrieved), 0);
    EXPECT_FLOAT_EQ(retrieved.position.x, 1.0f);
    EXPECT_FLOAT_EQ(retrieved.position.y, 2.0f);
    EXPECT_FLOAT_EQ(retrieved.position.z, 3.0f);
    EXPECT_FLOAT_EQ(retrieved.heading, 0.5f);
}

//=============================================================================
// Quaternion Tests
//=============================================================================

TEST_F(ParietalBridgeTest, QuaternionNormalize) {
    parietal_quat_t q = {2.0f, 0.0f, 0.0f, 0.0f};
    dragonfly_parietal_quat_normalize(&q);
    EXPECT_FLOAT_EQ(q.w, 1.0f);
    EXPECT_FLOAT_EQ(q.x, 0.0f);
    EXPECT_FLOAT_EQ(q.y, 0.0f);
    EXPECT_FLOAT_EQ(q.z, 0.0f);
}

TEST_F(ParietalBridgeTest, QuaternionFromEuler) {
    // Identity rotation (no rotation)
    parietal_quat_t q = dragonfly_parietal_quat_from_euler(0, 0, 0);
    EXPECT_NEAR(q.w, 1.0f, 0.01f);
    EXPECT_NEAR(q.x, 0.0f, 0.01f);
    EXPECT_NEAR(q.y, 0.0f, 0.01f);
    EXPECT_NEAR(q.z, 0.0f, 0.01f);

    // 90 degree yaw
    q = dragonfly_parietal_quat_from_euler(0, 0, M_PI / 2);
    EXPECT_NEAR(q.w, std::cos(M_PI / 4), 0.01f);
    EXPECT_NEAR(q.z, std::sin(M_PI / 4), 0.01f);
}

TEST_F(ParietalBridgeTest, QuaternionRotateVector) {
    // Identity rotation should preserve vector
    parietal_quat_t identity = {1.0f, 0.0f, 0.0f, 0.0f};
    parietal_vec3_t v = {1.0f, 0.0f, 0.0f};

    parietal_vec3_t rotated = dragonfly_parietal_quat_rotate_vec(&identity, &v);
    EXPECT_NEAR(rotated.x, 1.0f, 0.01f);
    EXPECT_NEAR(rotated.y, 0.0f, 0.01f);
    EXPECT_NEAR(rotated.z, 0.0f, 0.01f);

    // 90 degree rotation around Z should transform (1,0,0) to (0,1,0)
    parietal_quat_t rot_z = dragonfly_parietal_quat_from_euler(0, 0, M_PI / 2);
    rotated = dragonfly_parietal_quat_rotate_vec(&rot_z, &v);
    EXPECT_NEAR(rotated.x, 0.0f, 0.01f);
    EXPECT_NEAR(rotated.y, 1.0f, 0.01f);
    EXPECT_NEAR(rotated.z, 0.0f, 0.01f);
}

//=============================================================================
// Coordinate Transform Tests
//=============================================================================

TEST_F(ParietalBridgeTest, TransformPositionSameFrame) {
    parietal_vec3_t pos = {1.0f, 2.0f, 3.0f};
    EXPECT_EQ(dragonfly_parietal_bridge_transform_position(bridge, &pos,
        COORD_FRAME_WORLD, COORD_FRAME_WORLD), 0);

    // Should be unchanged
    EXPECT_FLOAT_EQ(pos.x, 1.0f);
    EXPECT_FLOAT_EQ(pos.y, 2.0f);
    EXPECT_FLOAT_EQ(pos.z, 3.0f);
}

TEST_F(ParietalBridgeTest, TransformWorldToBody) {
    // Set observer at origin, facing forward
    observer_state_t obs = {};
    obs.position = {0.0f, 0.0f, 0.0f};
    obs.orientation = {1.0f, 0.0f, 0.0f, 0.0f};  // Identity
    EXPECT_EQ(dragonfly_parietal_bridge_set_observer(bridge, &obs), 0);

    // Position in front should transform to same position in body frame
    parietal_vec3_t pos = {0.0f, 0.0f, 5.0f};  // 5m ahead
    EXPECT_EQ(dragonfly_parietal_bridge_transform_position(bridge, &pos,
        COORD_FRAME_WORLD, COORD_FRAME_BODY), 0);

    EXPECT_NEAR(pos.x, 0.0f, 0.01f);
    EXPECT_NEAR(pos.y, 0.0f, 0.01f);
    EXPECT_NEAR(pos.z, 5.0f, 0.01f);
}

TEST_F(ParietalBridgeTest, TransformWithObserverOffset) {
    // Set observer at (1, 0, 0), facing forward
    observer_state_t obs = {};
    obs.position = {1.0f, 0.0f, 0.0f};
    obs.orientation = {1.0f, 0.0f, 0.0f, 0.0f};
    EXPECT_EQ(dragonfly_parietal_bridge_set_observer(bridge, &obs), 0);

    // Position at (1, 0, 5) in world = (0, 0, 5) in body
    parietal_vec3_t pos = {1.0f, 0.0f, 5.0f};
    EXPECT_EQ(dragonfly_parietal_bridge_transform_position(bridge, &pos,
        COORD_FRAME_WORLD, COORD_FRAME_BODY), 0);

    EXPECT_NEAR(pos.x, 0.0f, 0.01f);
    EXPECT_NEAR(pos.y, 0.0f, 0.01f);
    EXPECT_NEAR(pos.z, 5.0f, 0.01f);
}

TEST_F(ParietalBridgeTest, ComputeAngles) {
    // Set observer at origin, facing forward (+Z)
    observer_state_t obs = {};
    obs.position = {0.0f, 0.0f, 0.0f};
    obs.orientation = {1.0f, 0.0f, 0.0f, 0.0f};
    EXPECT_EQ(dragonfly_parietal_bridge_set_observer(bridge, &obs), 0);

    // Target straight ahead
    parietal_vec3_t pos = {0.0f, 0.0f, 10.0f};
    float azimuth, elevation, distance;
    EXPECT_EQ(dragonfly_parietal_bridge_compute_angles(bridge, &pos, &azimuth, &elevation, &distance), 0);

    EXPECT_NEAR(azimuth, 0.0f, 0.01f);
    EXPECT_NEAR(elevation, 0.0f, 0.01f);
    EXPECT_NEAR(distance, 10.0f, 0.01f);

    // Target to the right
    pos = {10.0f, 0.0f, 0.0f};
    EXPECT_EQ(dragonfly_parietal_bridge_compute_angles(bridge, &pos, &azimuth, &elevation, &distance), 0);
    EXPECT_NEAR(azimuth, M_PI / 2, 0.01f);  // 90 degrees right
    EXPECT_NEAR(distance, 10.0f, 0.01f);

    // Target above
    pos = {0.0f, 10.0f, 0.0f};
    EXPECT_EQ(dragonfly_parietal_bridge_compute_angles(bridge, &pos, &azimuth, &elevation, &distance), 0);
    EXPECT_NEAR(elevation, M_PI / 2, 0.01f);  // 90 degrees up
}

//=============================================================================
// Attention Map Tests
//=============================================================================

TEST_F(ParietalBridgeTest, CreateDestroyAttentionMap) {
    parietal_attention_map_t* map = parietal_attention_map_create(64, 32);
    ASSERT_NE(map, nullptr);
    EXPECT_EQ(map->width, 64u);
    EXPECT_EQ(map->height, 32u);
    EXPECT_NE(map->weights, nullptr);
    parietal_attention_map_destroy(map);
}

TEST_F(ParietalBridgeTest, AttentionMapSetGet) {
    parietal_attention_map_t* map = parietal_attention_map_create(64, 32);
    ASSERT_NE(map, nullptr);

    // Set attention at center (0, 0)
    EXPECT_EQ(parietal_attention_map_set(map, 0.0f, 0.0f, 1.0f), 0);

    // Get it back
    float weight = parietal_attention_map_sample(map, 0.0f, 0.0f);
    EXPECT_NEAR(weight, 1.0f, 0.01f);

    parietal_attention_map_destroy(map);
}

TEST_F(ParietalBridgeTest, AttentionMapFindPeak) {
    parietal_attention_map_t* map = parietal_attention_map_create(64, 32);
    ASSERT_NE(map, nullptr);

    // Set highest attention to the right
    parietal_attention_map_set(map, 1.0f, 0.0f, 0.9f);
    parietal_attention_map_set(map, -1.0f, 0.0f, 0.3f);
    parietal_attention_map_set(map, 0.0f, 0.5f, 0.5f);

    float az, el, weight;
    EXPECT_EQ(parietal_attention_map_find_peak(map, &az, &el, &weight), 0);
    EXPECT_GT(az, 0.5f);  // Peak should be to the right
    EXPECT_NEAR(weight, 0.9f, 0.01f);

    parietal_attention_map_destroy(map);
}

TEST_F(ParietalBridgeTest, UpdateAttention) {
    parietal_attention_map_t* map = parietal_attention_map_create(64, 32);
    ASSERT_NE(map, nullptr);

    EXPECT_EQ(dragonfly_parietal_bridge_update_attention(bridge, map), 0);

    parietal_attention_map_destroy(map);
}

//=============================================================================
// Motor Command Tests
//=============================================================================

TEST_F(ParietalBridgeTest, GenerateSaccade) {
    motor_command_t cmd;
    EXPECT_EQ(dragonfly_parietal_bridge_generate_saccade(bridge, 0.3f, 0.1f, &cmd), 0);

    EXPECT_EQ(cmd.type, MOTOR_CMD_SACCADE);
    EXPECT_GT(cmd.amplitude, 0);
    EXPECT_GT(cmd.duration_ms, 0);
}

TEST_F(ParietalBridgeTest, GenerateSaccadeLargeAmplitude) {
    motor_command_t cmd;
    // Large saccade
    EXPECT_EQ(dragonfly_parietal_bridge_generate_saccade(bridge, 1.0f, 0.5f, &cmd), 0);

    // Larger saccade should take longer
    float duration = cmd.duration_ms;

    motor_command_t cmd2;
    EXPECT_EQ(dragonfly_parietal_bridge_generate_saccade(bridge, 0.1f, 0.05f, &cmd2), 0);
    EXPECT_LT(cmd2.duration_ms, duration);
}

//=============================================================================
// Intercept Path Tests
//=============================================================================

TEST_F(ParietalBridgeTest, ComputeInterceptPathNoDragonfly) {
    dragonfly_parietal_bridge_t* b = dragonfly_parietal_bridge_create(nullptr, nullptr, nullptr, nullptr);
    ASSERT_NE(b, nullptr);

    parietal_waypoint_t waypoints[16];
    int count = dragonfly_parietal_bridge_compute_intercept_path(b, 1, waypoints, 16);
    EXPECT_EQ(count, 0);  // No dragonfly = no waypoints

    dragonfly_parietal_bridge_destroy(b);
}

//=============================================================================
// Gain Field Tests
//=============================================================================

TEST_F(ParietalBridgeTest, ComputeGainField) {
    parietal_target_t target = {};
    target.id = 1;
    target.position = {0.0f, 0.0f, 10.0f};  // Straight ahead
    target.azimuth = 0.0f;
    target.elevation = 0.0f;

    gain_field_t gain;
    EXPECT_EQ(dragonfly_parietal_bridge_compute_gain_field(bridge, &target, &gain), 0);

    // Preferred direction should point forward
    EXPECT_NEAR(gain.preferred_direction[2], 1.0f, 0.01f);
    EXPECT_GT(gain.modulation_strength, 0);
}

TEST_F(ParietalBridgeTest, ApplyGainField) {
    motor_command_t cmd = {};
    cmd.velocity = {1.0f, 0.0f, 0.0f};
    cmd.urgency = 0.8f;

    gain_field_t gain = {};
    gain.modulation_strength = 0.5f;

    EXPECT_EQ(dragonfly_parietal_bridge_apply_gain_field(&cmd, &gain), 0);
    EXPECT_FLOAT_EQ(cmd.velocity.x, 0.5f);  // Modulated
    EXPECT_FLOAT_EQ(cmd.urgency, 0.4f);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(ParietalBridgeTest, GetStats) {
    parietal_bridge_stats_t stats;
    EXPECT_EQ(dragonfly_parietal_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.targets_processed, 0u);
    EXPECT_EQ(stats.motor_commands_generated, 0u);
}

TEST_F(ParietalBridgeTest, StatsTrackCommands) {
    motor_command_t cmd;
    dragonfly_parietal_bridge_generate_saccade(bridge, 0.3f, 0.1f, &cmd);
    dragonfly_parietal_bridge_generate_saccade(bridge, -0.2f, 0.0f, &cmd);

    parietal_bridge_stats_t stats;
    EXPECT_EQ(dragonfly_parietal_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.motor_commands_generated, 2u);
}

TEST_F(ParietalBridgeTest, ResetStats) {
    motor_command_t cmd;
    dragonfly_parietal_bridge_generate_saccade(bridge, 0.3f, 0.1f, &cmd);

    EXPECT_EQ(dragonfly_parietal_bridge_reset_stats(bridge), 0);

    parietal_bridge_stats_t stats;
    EXPECT_EQ(dragonfly_parietal_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.motor_commands_generated, 0u);
}

//=============================================================================
// Configuration Update Tests
//=============================================================================

TEST_F(ParietalBridgeTest, SetConfig) {
    parietal_bridge_config_t config = parietal_bridge_default_config();
    config.saccade_threshold = 0.2f;

    EXPECT_EQ(dragonfly_parietal_bridge_set_config(bridge, &config), 0);

    parietal_bridge_config_t retrieved;
    EXPECT_EQ(dragonfly_parietal_bridge_get_config(bridge, &retrieved), 0);
    EXPECT_FLOAT_EQ(retrieved.saccade_threshold, 0.2f);
}

TEST_F(ParietalBridgeTest, SetInvalidConfig) {
    parietal_bridge_config_t config = parietal_bridge_default_config();
    config.query_radius_default = -5.0f;

    EXPECT_EQ(dragonfly_parietal_bridge_set_config(bridge, &config), -1);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(ParietalBridgeTest, FrameName) {
    EXPECT_STREQ(dragonfly_parietal_frame_name(COORD_FRAME_EYE), "Eye");
    EXPECT_STREQ(dragonfly_parietal_frame_name(COORD_FRAME_HEAD), "Head");
    EXPECT_STREQ(dragonfly_parietal_frame_name(COORD_FRAME_BODY), "Body");
    EXPECT_STREQ(dragonfly_parietal_frame_name(COORD_FRAME_WORLD), "World");
    EXPECT_STREQ(dragonfly_parietal_frame_name(COORD_FRAME_CAMERA), "Camera");
}

TEST_F(ParietalBridgeTest, MotorCmdName) {
    EXPECT_STREQ(dragonfly_parietal_motor_cmd_name(MOTOR_CMD_SACCADE), "Saccade");
    EXPECT_STREQ(dragonfly_parietal_motor_cmd_name(MOTOR_CMD_SMOOTH_PURSUIT), "SmoothPursuit");
    EXPECT_STREQ(dragonfly_parietal_motor_cmd_name(MOTOR_CMD_HEAD_TURN), "HeadTurn");
    EXPECT_STREQ(dragonfly_parietal_motor_cmd_name(MOTOR_CMD_BODY_ORIENT), "BodyOrient");
    EXPECT_STREQ(dragonfly_parietal_motor_cmd_name(MOTOR_CMD_INTERCEPT_PATH), "InterceptPath");
}

//=============================================================================
// Null Pointer Tests
//=============================================================================

TEST_F(ParietalBridgeTest, NullPointerHandling) {
    observer_state_t obs;
    parietal_target_t target;
    parietal_bridge_stats_t stats;
    parietal_bridge_config_t config;
    motor_command_t cmd;
    gain_field_t gain;

    EXPECT_EQ(dragonfly_parietal_bridge_reset(nullptr), -1);
    EXPECT_EQ(dragonfly_parietal_bridge_set_observer(nullptr, &obs), -1);
    EXPECT_EQ(dragonfly_parietal_bridge_set_observer(bridge, nullptr), -1);
    EXPECT_EQ(dragonfly_parietal_bridge_get_observer(nullptr, &obs), -1);
    EXPECT_EQ(dragonfly_parietal_bridge_get_observer(bridge, nullptr), -1);

    parietal_vec3_t pos;
    EXPECT_EQ(dragonfly_parietal_bridge_transform_position(nullptr, &pos, COORD_FRAME_WORLD, COORD_FRAME_BODY), -1);
    EXPECT_EQ(dragonfly_parietal_bridge_transform_position(bridge, nullptr, COORD_FRAME_WORLD, COORD_FRAME_BODY), -1);

    EXPECT_EQ(dragonfly_parietal_bridge_compute_angles(nullptr, &pos, nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(dragonfly_parietal_bridge_compute_angles(bridge, nullptr, nullptr, nullptr, nullptr), -1);

    EXPECT_EQ(dragonfly_parietal_bridge_sync_targets(nullptr), -1);
    EXPECT_EQ(dragonfly_parietal_bridge_get_targets(nullptr, &target, COORD_FRAME_WORLD), -1);
    EXPECT_EQ(dragonfly_parietal_bridge_get_targets(bridge, nullptr, COORD_FRAME_WORLD), -1);

    EXPECT_EQ(dragonfly_parietal_bridge_update_attention(nullptr, nullptr), -1);

    EXPECT_EQ(dragonfly_parietal_bridge_generate_saccade(nullptr, 0, 0, &cmd), -1);
    EXPECT_EQ(dragonfly_parietal_bridge_generate_saccade(bridge, 0, 0, nullptr), -1);

    EXPECT_EQ(dragonfly_parietal_bridge_compute_gain_field(nullptr, &target, &gain), -1);
    EXPECT_EQ(dragonfly_parietal_bridge_compute_gain_field(bridge, nullptr, &gain), -1);
    EXPECT_EQ(dragonfly_parietal_bridge_compute_gain_field(bridge, &target, nullptr), -1);

    EXPECT_EQ(dragonfly_parietal_bridge_apply_gain_field(nullptr, &gain), -1);
    EXPECT_EQ(dragonfly_parietal_bridge_apply_gain_field(&cmd, nullptr), -1);

    EXPECT_EQ(dragonfly_parietal_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(dragonfly_parietal_bridge_get_stats(bridge, nullptr), -1);
    EXPECT_EQ(dragonfly_parietal_bridge_reset_stats(nullptr), -1);

    EXPECT_EQ(dragonfly_parietal_bridge_set_config(nullptr, &config), -1);
    EXPECT_EQ(dragonfly_parietal_bridge_set_config(bridge, nullptr), -1);
    EXPECT_EQ(dragonfly_parietal_bridge_get_config(nullptr, &config), -1);
    EXPECT_EQ(dragonfly_parietal_bridge_get_config(bridge, nullptr), -1);
}
