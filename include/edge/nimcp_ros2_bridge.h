/**
 * @file nimcp_ros2_bridge.h
 * @brief ROS 2 Bridge — Connect NIMCP brains to Robot Operating System 2
 *
 * Enables NIMCP brains to receive sensor data (LiDAR, IMU, camera, odometry)
 * and output motor commands (Twist, JointTrajectory, PWM) to any ROS 2
 * compatible robot platform.
 *
 * Compiles in TWO modes:
 *   - Stub mode (default): No ROS 2 dependency. Supports manual sensor
 *     injection and brain inference for headless testing.
 *   - Full mode (NIMCP_HAS_ROS2): Links against rclc. Real ROS 2 node with
 *     subscriptions, publishers, and executor.
 *
 * Enable at cmake time: -DNIMCP_ENABLE_ROS2=ON
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#ifndef NIMCP_ROS2_BRIDGE_H
#define NIMCP_ROS2_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

/* Use the public brain handle type from edge types */
#include "edge/nimcp_edge_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Types
 * ============================================================================ */

typedef struct nimcp_ros2_bridge nimcp_ros2_bridge_t;

/** Motor command output format */
typedef enum {
    NIMCP_ROS2_CMD_TWIST,          /**< geometry_msgs/Twist (linear + angular velocity) */
    NIMCP_ROS2_CMD_JOINT,          /**< trajectory_msgs/JointTrajectory */
    NIMCP_ROS2_CMD_PWM             /**< Custom PWM array */
} nimcp_ros2_cmd_type_t;

/** Bridge configuration */
typedef struct {
    const char* node_name;          /**< ROS 2 node name (default "nimcp_brain") */
    const char* namespace_prefix;   /**< Topic namespace (default "/nimcp") */

    /* Sensor subscriptions */
    bool subscribe_lidar;           /**< /scan or /points */
    bool subscribe_imu;             /**< /imu/data */
    bool subscribe_camera;          /**< /camera/image_raw */
    bool subscribe_depth;           /**< /camera/depth */
    bool subscribe_odom;            /**< /odom */
    bool subscribe_joint_states;    /**< /joint_states */
    bool subscribe_battery;         /**< /battery_state */

    /* Motor command output */
    nimcp_ros2_cmd_type_t cmd_type; /**< Output command format */
    const char* cmd_topic;          /**< Output topic (default "/cmd_vel") */
    float cmd_rate_hz;              /**< Command publish rate (default 20.0) */

    /* Brain integration */
    uint32_t brain_input_dim;       /**< Feature vector size for brain (default 1024) */
    float inference_rate_hz;        /**< How often to run brain inference (default 30.0) */

    /* Safety */
    uint32_t watchdog_timeout_ms;   /**< Watchdog timeout (default 500) */
    float max_linear_vel;           /**< Max linear velocity m/s (default 1.0) */
    float max_angular_vel;          /**< Max angular velocity rad/s (default 1.0) */
} nimcp_ros2_config_t;

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

/**
 * @brief Return a default ROS 2 bridge configuration.
 *
 * Subscribes to IMU and odometry. Publishes Twist on /cmd_vel at 20 Hz.
 * Brain input dim 1024, inference at 30 Hz, watchdog 500 ms.
 */
nimcp_ros2_config_t nimcp_ros2_config_default(void);

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

/**
 * @brief Create a ROS 2 bridge for a brain.
 *
 * Allocates the bridge struct, sensor/command buffers, and mutexes.
 * In stub mode (no ROS 2), the bridge is fully functional for manual
 * sensor injection and brain inference testing.
 *
 * @param brain    Public brain handle (nimcp_brain_t)
 * @param config   Bridge configuration (NULL for defaults)
 * @return Bridge instance, or NULL on failure
 */
nimcp_ros2_bridge_t* nimcp_ros2_bridge_create(nimcp_brain_t brain,
                                               const nimcp_ros2_config_t* config);

/**
 * @brief Destroy a ROS 2 bridge and free all resources.
 *
 * Stops the control loop if running, tears down ROS 2 node (if active),
 * and frees all buffers and mutexes.
 */
void nimcp_ros2_bridge_destroy(nimcp_ros2_bridge_t* bridge);

/* ============================================================================
 * Control
 * ============================================================================ */

/**
 * @brief Start the bridge control loop.
 *
 * With ROS 2: creates the node, subscriptions, publisher, and starts
 * the executor + control loop thread.
 *
 * Without ROS 2: starts the control loop thread that runs brain inference
 * at inference_rate_hz using the sensor_features buffer as input, and
 * stores output in the command buffer. Useful for testing the brain-to-motor
 * pipeline without a real robot.
 *
 * @return 0 on success, -1 on failure
 */
int nimcp_ros2_bridge_start(nimcp_ros2_bridge_t* bridge);

/**
 * @brief Stop the bridge control loop.
 *
 * Signals the control thread to exit and joins it. With ROS 2, also
 * shuts down subscriptions, publisher, and the node.
 *
 * @return 0 on success, -1 on failure
 */
int nimcp_ros2_bridge_stop(nimcp_ros2_bridge_t* bridge);

/**
 * @brief Check if the bridge control loop is running.
 */
bool nimcp_ros2_bridge_is_running(const nimcp_ros2_bridge_t* bridge);

/* ============================================================================
 * Sensor Injection (works without ROS 2)
 * ============================================================================ */

/**
 * @brief Inject sensor data into the bridge manually.
 *
 * Writes float data into the sensor_features buffer. This works both
 * with and without ROS 2, enabling headless testing of the brain-motor
 * pipeline.
 *
 * @param bridge  Bridge instance
 * @param topic   Topic name (used for offset mapping; NULL to write at offset 0)
 * @param data    Sensor data array
 * @param count   Number of floats
 * @return 0 on success, -1 on failure
 */
int nimcp_ros2_bridge_inject_sensor(nimcp_ros2_bridge_t* bridge,
                                     const char* topic, const float* data,
                                     uint32_t count);

/* ============================================================================
 * Motor Command Output
 * ============================================================================ */

/**
 * @brief Get the last motor command output from the bridge.
 *
 * Copies the most recent command buffer contents into the caller's buffer.
 *
 * @param bridge     Bridge instance
 * @param data       Output buffer
 * @param max_count  Maximum floats to copy
 * @return Number of floats copied, or -1 on failure
 */
int nimcp_ros2_bridge_get_last_cmd(const nimcp_ros2_bridge_t* bridge,
                                    float* data, uint32_t max_count);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ROS2_BRIDGE_H */
