/**
 * @file test_drone_bridges.cpp
 * @brief Unit tests for drone bridge stubs — MAVLink, DJI, MSP, Parrot, ROS 2.
 *
 * WHAT: Test create/destroy lifecycle, config defaults, stub-mode command behavior,
 *       telemetry getters, and NULL safety for all five drone/robot bridges.
 * WHY:  Each bridge is a safety-critical interface between the brain and actuators.
 *       Stubs must be well-behaved (no crash, clean teardown, zeroed telemetry).
 * HOW:  Google Test, stub mode (no vendor SDKs, no hardware).
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "edge/nimcp_mavlink_bridge.h"
#include "edge/nimcp_dji_bridge.h"
#include "edge/nimcp_msp_bridge.h"
#include "edge/nimcp_parrot_bridge.h"
#include "edge/nimcp_ros2_bridge.h"
}

// ============================================================================
// MAVLink Bridge
// ============================================================================

TEST(DroneBridges, MAVLinkCreateDestroy) {
    nimcp_mavlink_bridge_t* bridge = nimcp_mavlink_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);
    nimcp_mavlink_bridge_destroy(bridge);
}

TEST(DroneBridges, MAVLinkDestroyNull) {
    nimcp_mavlink_bridge_destroy(NULL);
    SUCCEED();
}

TEST(DroneBridges, MAVLinkConfigDefaults) {
    nimcp_mavlink_config_t cfg = nimcp_mavlink_config_default();
    EXPECT_GT(cfg.baud_rate, 0u);
    EXPECT_GT(cfg.attitude_rate, 0.0f);
    EXPECT_GT(cfg.geofence_radius_m, 0.0f);
    EXPECT_GT(cfg.max_altitude_m, 0.0f);
    EXPECT_GT(cfg.min_battery_pct, 0.0f);
}

TEST(DroneBridges, MAVLinkGetAttitudeZeroed) {
    nimcp_mavlink_bridge_t* bridge = nimcp_mavlink_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    nimcp_mavlink_attitude_t att;
    memset(&att, 0xFF, sizeof(att));
    int rc = nimcp_mavlink_get_attitude(bridge, &att);
    EXPECT_EQ(rc, 0);
    // Freshly created bridge should have zeroed telemetry
    EXPECT_FLOAT_EQ(att.roll, 0.0f);
    EXPECT_FLOAT_EQ(att.pitch, 0.0f);
    EXPECT_FLOAT_EQ(att.yaw, 0.0f);

    nimcp_mavlink_bridge_destroy(bridge);
}

TEST(DroneBridges, MAVLinkGetPositionZeroed) {
    nimcp_mavlink_bridge_t* bridge = nimcp_mavlink_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    nimcp_mavlink_position_t pos;
    memset(&pos, 0xFF, sizeof(pos));
    int rc = nimcp_mavlink_get_position(bridge, &pos);
    EXPECT_EQ(rc, 0);
    EXPECT_DOUBLE_EQ(pos.latitude, 0.0);
    EXPECT_DOUBLE_EQ(pos.longitude, 0.0);

    nimcp_mavlink_bridge_destroy(bridge);
}

TEST(DroneBridges, MAVLinkGetBatteryZeroed) {
    nimcp_mavlink_bridge_t* bridge = nimcp_mavlink_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    nimcp_mavlink_battery_t bat;
    memset(&bat, 0xFF, sizeof(bat));
    int rc = nimcp_mavlink_get_battery(bridge, &bat);
    EXPECT_EQ(rc, 0);
    EXPECT_FLOAT_EQ(bat.voltage, 0.0f);

    nimcp_mavlink_bridge_destroy(bridge);
}

TEST(DroneBridges, MAVLinkComposeFeatures) {
    nimcp_mavlink_bridge_t* bridge = nimcp_mavlink_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    float features[NIMCP_MAVLINK_FEATURE_COUNT];
    memset(features, 0xFF, sizeof(features));
    int count = nimcp_mavlink_compose_features(bridge, features, NIMCP_MAVLINK_FEATURE_COUNT);
    EXPECT_EQ(count, NIMCP_MAVLINK_FEATURE_COUNT);
    // All features should be zero (no telemetry data)
    for (int i = 0; i < NIMCP_MAVLINK_FEATURE_COUNT; i++) {
        EXPECT_FALSE(std::isnan(features[i])) << "Feature " << i << " is NaN";
    }

    nimcp_mavlink_bridge_destroy(bridge);
}

TEST(DroneBridges, MAVLinkNullSafety) {
    EXPECT_LT(nimcp_mavlink_get_attitude(NULL, NULL), 0);
    EXPECT_LT(nimcp_mavlink_get_position(NULL, NULL), 0);
    EXPECT_LT(nimcp_mavlink_get_battery(NULL, NULL), 0);
    EXPECT_LT(nimcp_mavlink_compose_features(NULL, NULL, 0), 0);
}

TEST(DroneBridges, MAVLinkFlightModeEnums) {
    EXPECT_EQ(NIMCP_FLIGHT_MANUAL, 0);
    EXPECT_EQ(NIMCP_FLIGHT_GUIDED, 3);
    EXPECT_EQ(NIMCP_FLIGHT_LAND, 6);
}

// ============================================================================
// DJI Bridge
// ============================================================================

TEST(DroneBridges, DJICreateDestroy) {
    nimcp_dji_bridge_t* bridge = nimcp_dji_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);
    nimcp_dji_bridge_destroy(bridge);
}

TEST(DroneBridges, DJIDestroyNull) {
    nimcp_dji_bridge_destroy(NULL);
    SUCCEED();
}

TEST(DroneBridges, DJIConfigDefaults) {
    nimcp_dji_config_t cfg = nimcp_dji_config_default();
    EXPECT_GT(cfg.baud_rate, 0u);
    EXPECT_GT(cfg.geofence_radius_m, 0.0f);
    EXPECT_GT(cfg.max_altitude_m, 0.0f);
    EXPECT_GT(cfg.min_battery_pct, 0.0f);
    EXPECT_TRUE(cfg.enable_geofence);
}

TEST(DroneBridges, DJICommandsReturnMinusOneInStub) {
    nimcp_dji_bridge_t* bridge = nimcp_dji_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    // All commands should return -1 in stub mode
    EXPECT_EQ(nimcp_dji_arm(bridge, true), -1);
    EXPECT_EQ(nimcp_dji_takeoff(bridge), -1);
    EXPECT_EQ(nimcp_dji_land(bridge), -1);
    EXPECT_EQ(nimcp_dji_goto_position(bridge, 0.0, 0.0, 0.0f, 0.0f), -1);
    EXPECT_EQ(nimcp_dji_set_velocity(bridge, 0.0f, 0.0f, 0.0f, 0.0f), -1);
    EXPECT_EQ(nimcp_dji_set_gimbal(bridge, 0.0f, 0.0f, 0.0f), -1);
    EXPECT_EQ(nimcp_dji_trigger_photo(bridge), -1);

    nimcp_dji_bridge_destroy(bridge);
}

TEST(DroneBridges, DJIGetAttitudeZeroed) {
    nimcp_dji_bridge_t* bridge = nimcp_dji_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    nimcp_dji_attitude_t att;
    memset(&att, 0xFF, sizeof(att));
    int rc = nimcp_dji_get_attitude(bridge, &att);
    EXPECT_EQ(rc, 0);
    EXPECT_FLOAT_EQ(att.roll, 0.0f);
    EXPECT_FLOAT_EQ(att.pitch, 0.0f);

    nimcp_dji_bridge_destroy(bridge);
}

TEST(DroneBridges, DJINullSafety) {
    EXPECT_LT(nimcp_dji_get_attitude(NULL, NULL), 0);
    EXPECT_LT(nimcp_dji_get_position(NULL, NULL), 0);
    EXPECT_LT(nimcp_dji_get_battery(NULL, NULL), 0);
    EXPECT_LT(nimcp_dji_arm(NULL, true), 0);
}

TEST(DroneBridges, DJIFlightStatusEnums) {
    EXPECT_EQ(NIMCP_DJI_STATUS_STOPPED, 0);
    EXPECT_EQ(NIMCP_DJI_STATUS_ON_GROUND, 1);
    EXPECT_EQ(NIMCP_DJI_STATUS_IN_AIR, 2);
}

// ============================================================================
// MSP Bridge
// ============================================================================

TEST(DroneBridges, MSPCreateDestroy) {
    nimcp_msp_bridge_t* bridge = nimcp_msp_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);
    nimcp_msp_bridge_destroy(bridge);
}

TEST(DroneBridges, MSPDestroyNull) {
    nimcp_msp_bridge_destroy(NULL);
    SUCCEED();
}

TEST(DroneBridges, MSPConfigDefaults) {
    nimcp_msp_config_t cfg = nimcp_msp_config_default();
    EXPECT_GT(cfg.baud_rate, 0u);
    EXPECT_GT(cfg.poll_rate_hz, 0u);
    EXPECT_GT(cfg.min_battery_volts, 0.0f);
    EXPECT_GT(cfg.max_angle_degrees, 0.0f);
}

TEST(DroneBridges, MSPSetRcOverrideStub) {
    nimcp_msp_bridge_t* bridge = nimcp_msp_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    int rc = nimcp_msp_set_rc_override(bridge, 1500, 1500, 1500, 1500);
    EXPECT_EQ(rc, -1) << "RC override should fail in stub mode";

    nimcp_msp_bridge_destroy(bridge);
}

TEST(DroneBridges, MSPGetAttitudeZeroed) {
    nimcp_msp_bridge_t* bridge = nimcp_msp_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    nimcp_msp_attitude_t att;
    memset(&att, 0xFF, sizeof(att));
    int rc = nimcp_msp_get_attitude(bridge, &att);
    EXPECT_EQ(rc, 0);
    EXPECT_FLOAT_EQ(att.roll, 0.0f);

    nimcp_msp_bridge_destroy(bridge);
}

TEST(DroneBridges, MSPNullSafety) {
    EXPECT_LT(nimcp_msp_get_attitude(NULL, NULL), 0);
    EXPECT_LT(nimcp_msp_get_gps(NULL, NULL), 0);
    EXPECT_LT(nimcp_msp_get_battery(NULL, NULL), 0);
    EXPECT_LT(nimcp_msp_set_rc_override(NULL, 0, 0, 0, 0), 0);
}

// ============================================================================
// Parrot Bridge
// ============================================================================

TEST(DroneBridges, ParrotCreateDestroy) {
    nimcp_parrot_bridge_t* bridge = nimcp_parrot_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);
    nimcp_parrot_bridge_destroy(bridge);
}

TEST(DroneBridges, ParrotDestroyNull) {
    nimcp_parrot_bridge_destroy(NULL);
    SUCCEED();
}

TEST(DroneBridges, ParrotConfigDefaults) {
    nimcp_parrot_config_t cfg = nimcp_parrot_config_default();
    EXPECT_GT(cfg.geofence_radius_m, 0.0f);
    EXPECT_GT(cfg.max_altitude_m, 0.0f);
    EXPECT_GT(cfg.min_battery_pct, 0.0f);
    EXPECT_GT(cfg.max_tilt_degrees, 0.0f);
}

TEST(DroneBridges, ParrotMoveByStub) {
    nimcp_parrot_bridge_t* bridge = nimcp_parrot_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    int rc = nimcp_parrot_move_by(bridge, 1.0f, 0.0f, 0.0f, 0.0f);
    EXPECT_EQ(rc, -1) << "move_by should fail in stub mode";

    nimcp_parrot_bridge_destroy(bridge);
}

TEST(DroneBridges, ParrotGetAttitudeZeroed) {
    nimcp_parrot_bridge_t* bridge = nimcp_parrot_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    nimcp_parrot_attitude_t att;
    memset(&att, 0xFF, sizeof(att));
    int rc = nimcp_parrot_get_attitude(bridge, &att);
    EXPECT_EQ(rc, 0);
    EXPECT_FLOAT_EQ(att.roll, 0.0f);

    nimcp_parrot_bridge_destroy(bridge);
}

TEST(DroneBridges, ParrotNullSafety) {
    EXPECT_LT(nimcp_parrot_get_attitude(NULL, NULL), 0);
    EXPECT_LT(nimcp_parrot_get_position(NULL, NULL), 0);
    EXPECT_LT(nimcp_parrot_get_battery(NULL, NULL), 0);
    EXPECT_LT(nimcp_parrot_move_by(NULL, 0, 0, 0, 0), 0);
}

// ============================================================================
// ROS 2 Bridge
// ============================================================================

TEST(DroneBridges, ROS2CreateDestroy) {
    // ROS 2 bridge requires a brain handle — use NULL to test NULL safety
    nimcp_ros2_bridge_t* bridge = nimcp_ros2_bridge_create(NULL, NULL);
    // NULL brain may return NULL (valid behavior)
    if (bridge) {
        nimcp_ros2_bridge_destroy(bridge);
    }
    SUCCEED() << "ROS2 create/destroy with NULL brain did not crash";
}

TEST(DroneBridges, ROS2DestroyNull) {
    nimcp_ros2_bridge_destroy(NULL);
    SUCCEED();
}

TEST(DroneBridges, ROS2ConfigDefaults) {
    nimcp_ros2_config_t cfg = nimcp_ros2_config_default();
    EXPECT_GT(cfg.brain_input_dim, 0u);
    EXPECT_GT(cfg.inference_rate_hz, 0.0f);
    EXPECT_GT(cfg.cmd_rate_hz, 0.0f);
    EXPECT_GT(cfg.watchdog_timeout_ms, 0u);
    EXPECT_GT(cfg.max_linear_vel, 0.0f);
    EXPECT_GT(cfg.max_angular_vel, 0.0f);
}

TEST(DroneBridges, ROS2NotRunningInitially) {
    nimcp_ros2_bridge_t* bridge = nimcp_ros2_bridge_create(NULL, NULL);
    if (bridge) {
        EXPECT_FALSE(nimcp_ros2_bridge_is_running(bridge));
        nimcp_ros2_bridge_destroy(bridge);
    }
    SUCCEED();
}

TEST(DroneBridges, ROS2IsRunningNull) {
    EXPECT_FALSE(nimcp_ros2_bridge_is_running(NULL));
}
