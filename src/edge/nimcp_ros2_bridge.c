/**
 * @file nimcp_ros2_bridge.c
 * @brief ROS 2 Bridge — NIMCP brain ↔ ROS 2 robot integration
 *
 * Two compilation modes:
 *   - NIMCP_HAS_ROS2 defined: full rclc integration (node, subs, pubs, executor)
 *   - NIMCP_HAS_ROS2 not defined: stub mode with manual sensor injection,
 *     brain inference control loop, and command output capture
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#define LOG_MODULE "ROS2_BRIDGE"

#include "edge/nimcp_ros2_bridge.h"
#include "nimcp.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"

#include <string.h>
#include <math.h>

#ifdef NIMCP_HAS_ROS2
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <rcl/rcl.h>
#include <geometry_msgs/msg/twist.h>
#include <sensor_msgs/msg/laser_scan.h>
#include <sensor_msgs/msg/imu.h>
#include <nav_msgs/msg/odometry.h>
#include <sensor_msgs/msg/joint_state.h>
#include <sensor_msgs/msg/battery_state.h>
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define ROS2_BRIDGE_DEFAULT_NODE_NAME     "nimcp_brain"
#define ROS2_BRIDGE_DEFAULT_NAMESPACE     "/nimcp"
#define ROS2_BRIDGE_DEFAULT_CMD_TOPIC     "/cmd_vel"
#define ROS2_BRIDGE_DEFAULT_CMD_RATE_HZ   20.0f
#define ROS2_BRIDGE_DEFAULT_INPUT_DIM     1024
#define ROS2_BRIDGE_DEFAULT_INFER_HZ      30.0f
#define ROS2_BRIDGE_DEFAULT_WATCHDOG_MS   500
#define ROS2_BRIDGE_DEFAULT_MAX_LIN_VEL   1.0f
#define ROS2_BRIDGE_DEFAULT_MAX_ANG_VEL   1.0f

/** Twist command dimension: linear.x/y/z + angular.x/y/z */
#define ROS2_TWIST_DIM   6

/** Maximum output dimension from brain for motor mapping */
#define ROS2_MAX_CMD_DIM 64

/* Sensor feature offsets within the flattened sensor buffer.
 * Each sensor type gets a fixed-size slot so subscriptions can be toggled
 * independently without shifting offsets.
 *
 * All sensor callbacks validate that OFFSET + SIZE <= sensor_feature_count
 * before writing, preventing buffer overflows even if the sensor provides
 * more data than the slot allocation. Local arrays are stack-allocated
 * at exactly SLOT_SIZE to match the memcpy length. */
#define SENSOR_SLOT_LIDAR_OFFSET        0
#define SENSOR_SLOT_LIDAR_SIZE          360   /* 1-degree resolution */
#define SENSOR_SLOT_IMU_OFFSET          360
#define SENSOR_SLOT_IMU_SIZE            10    /* orientation(4) + angular_vel(3) + linear_accel(3) */
#define SENSOR_SLOT_CAMERA_OFFSET       370
#define SENSOR_SLOT_CAMERA_SIZE         256   /* Downsampled feature vector */
#define SENSOR_SLOT_DEPTH_OFFSET        626
#define SENSOR_SLOT_DEPTH_SIZE          128   /* Downsampled depth features */
#define SENSOR_SLOT_ODOM_OFFSET         754
#define SENSOR_SLOT_ODOM_SIZE           13    /* pose(7) + twist(6) */
#define SENSOR_SLOT_JOINT_OFFSET        767
#define SENSOR_SLOT_JOINT_SIZE          32    /* Up to 32 joints */
#define SENSOR_SLOT_BATTERY_OFFSET      799
#define SENSOR_SLOT_BATTERY_SIZE        4     /* voltage, current, charge, percentage */

/** Total sensor buffer size (all slots) */
#define SENSOR_BUFFER_TOTAL_SIZE        803

/* ============================================================================
 * Internal Bridge Struct
 * ============================================================================ */

struct nimcp_ros2_bridge {
    nimcp_brain_t brain;
    nimcp_ros2_config_t config;

    /* Sensor feature buffer (composed from all subscriptions) */
    float* sensor_features;
    uint32_t sensor_feature_count;
    nimcp_mutex_t* sensor_lock;

    /* Motor command output buffer */
    float* cmd_output;
    uint32_t cmd_dim;
    nimcp_mutex_t* cmd_lock;

    /* Control loop thread */
    nimcp_thread_t control_thread;
    volatile bool running;
    volatile bool started;

    /* Watchdog */
    volatile uint64_t last_inference_us;

#ifdef NIMCP_HAS_ROS2
    /* ROS 2 handles */
    rcl_node_t node;
    rcl_allocator_t allocator;
    rclc_support_t support;
    rclc_executor_t executor;

    /* Subscriptions */
    rcl_subscription_t sub_lidar;
    rcl_subscription_t sub_imu;
    rcl_subscription_t sub_odom;
    rcl_subscription_t sub_joint_states;
    rcl_subscription_t sub_battery;

    /* Publisher */
    rcl_publisher_t pub_cmd;
    rcl_timer_t cmd_timer;

    /* ROS 2 message buffers */
    geometry_msgs__msg__Twist twist_msg;
    sensor_msgs__msg__LaserScan lidar_msg;
    sensor_msgs__msg__Imu imu_msg;
    nav_msgs__msg__Odometry odom_msg;
    sensor_msgs__msg__JointState joint_msg;
    sensor_msgs__msg__BatteryState battery_msg;

    bool ros2_initialized;
#endif
};

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

nimcp_ros2_config_t nimcp_ros2_config_default(void)
{
    nimcp_ros2_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.node_name        = ROS2_BRIDGE_DEFAULT_NODE_NAME;
    cfg.namespace_prefix = ROS2_BRIDGE_DEFAULT_NAMESPACE;

    /* Subscribe to common robot sensors */
    cfg.subscribe_lidar        = false;
    cfg.subscribe_imu          = true;
    cfg.subscribe_camera       = false;
    cfg.subscribe_depth        = false;
    cfg.subscribe_odom         = true;
    cfg.subscribe_joint_states = false;
    cfg.subscribe_battery      = false;

    /* Motor output */
    cfg.cmd_type     = NIMCP_ROS2_CMD_TWIST;
    cfg.cmd_topic    = ROS2_BRIDGE_DEFAULT_CMD_TOPIC;
    cfg.cmd_rate_hz  = ROS2_BRIDGE_DEFAULT_CMD_RATE_HZ;

    /* Brain */
    cfg.brain_input_dim  = ROS2_BRIDGE_DEFAULT_INPUT_DIM;
    cfg.inference_rate_hz = ROS2_BRIDGE_DEFAULT_INFER_HZ;

    /* Safety */
    cfg.watchdog_timeout_ms = ROS2_BRIDGE_DEFAULT_WATCHDOG_MS;
    cfg.max_linear_vel      = ROS2_BRIDGE_DEFAULT_MAX_LIN_VEL;
    cfg.max_angular_vel     = ROS2_BRIDGE_DEFAULT_MAX_ANG_VEL;

    return cfg;
}

/* ============================================================================
 * Helpers
 * ============================================================================ */

/** Clamp a float to [-limit, +limit] */
static float clampf(float val, float limit)
{
    if (val > limit) return limit;
    if (val < -limit) return -limit;
    return val;
}

/**
 * Translate brain output vector to motor command.
 *
 * For TWIST: map first 6 outputs to linear.xyz + angular.xyz, then clamp.
 * For JOINT: pass through raw values (clamped per-joint in downstream).
 * For PWM:   pass through raw values.
 */
static void translate_brain_output_to_cmd(
    const float* brain_output, uint32_t output_size,
    float* cmd, uint32_t cmd_dim,
    nimcp_ros2_cmd_type_t cmd_type,
    float max_lin, float max_ang)
{
    memset(cmd, 0, cmd_dim * sizeof(float));

    switch (cmd_type) {
    case NIMCP_ROS2_CMD_TWIST: {
        /* Map brain output[0..5] → linear.x/y/z + angular.x/y/z */
        uint32_t n = output_size < ROS2_TWIST_DIM ? output_size : ROS2_TWIST_DIM;
        for (uint32_t i = 0; i < n; i++) {
            cmd[i] = brain_output[i];
        }
        /* Clamp linear velocities (indices 0-2) */
        for (uint32_t i = 0; i < 3 && i < cmd_dim; i++) {
            cmd[i] = clampf(cmd[i], max_lin);
        }
        /* Clamp angular velocities (indices 3-5) */
        for (uint32_t i = 3; i < 6 && i < cmd_dim; i++) {
            cmd[i] = clampf(cmd[i], max_ang);
        }
        break;
    }
    case NIMCP_ROS2_CMD_JOINT:
    case NIMCP_ROS2_CMD_PWM: {
        /* Pass through, clamped to max_lin as generic safety bound */
        uint32_t n = output_size < cmd_dim ? output_size : cmd_dim;
        for (uint32_t i = 0; i < n; i++) {
            cmd[i] = clampf(brain_output[i], max_lin);
        }
        break;
    }
    }
}

/* ============================================================================
 * Control Loop Thread
 * ============================================================================ */

static void* ros2_bridge_control_loop(void* arg)
{
    nimcp_ros2_bridge_t* bridge = (nimcp_ros2_bridge_t*)arg;
    if (!bridge) {
        return NULL;
    }

    const uint32_t input_dim = bridge->config.brain_input_dim;
    const float rate_hz = bridge->config.inference_rate_hz > 0.0f
                        ? bridge->config.inference_rate_hz
                        : ROS2_BRIDGE_DEFAULT_INFER_HZ;
    const uint64_t sleep_us = (uint64_t)(1000000.0f / rate_hz);

    /* Allocate thread-local copy of sensor features */
    float* local_features = (float*)nimcp_calloc(input_dim, sizeof(float));
    if (!local_features) {
        LOG_ERROR("[ROS2_BRIDGE] Failed to allocate local feature buffer (%u floats)",
                  input_dim);
        return NULL;
    }

    /* Brain inference output buffer */
    float* brain_output = (float*)nimcp_calloc(ROS2_MAX_CMD_DIM, sizeof(float));
    if (!brain_output) {
        LOG_ERROR("[ROS2_BRIDGE] Failed to allocate brain output buffer");
        nimcp_free(local_features);
        return NULL;
    }

    LOG_INFO("[ROS2_BRIDGE] Control loop started (%.1f Hz, input_dim=%u)",
             rate_hz, input_dim);

    while (bridge->running) {
        /* 1. Copy sensor features under lock */
        nimcp_mutex_lock(bridge->sensor_lock);
        uint32_t copy_count = bridge->sensor_feature_count < input_dim
                            ? bridge->sensor_feature_count
                            : input_dim;
        memcpy(local_features, bridge->sensor_features, copy_count * sizeof(float));
        nimcp_mutex_unlock(bridge->sensor_lock);

        /* 2. Run brain inference */
        uint32_t out_size = ROS2_MAX_CMD_DIM;
        nimcp_status_t status = nimcp_brain_infer(
            bridge->brain,
            local_features, input_dim,
            brain_output, out_size);

        if (status == NIMCP_OK) {
            /* 3. Translate brain output to motor command */
            float cmd_buf[ROS2_MAX_CMD_DIM];
            translate_brain_output_to_cmd(
                brain_output, out_size,
                cmd_buf, bridge->cmd_dim,
                bridge->config.cmd_type,
                bridge->config.max_linear_vel,
                bridge->config.max_angular_vel);

            /* 4. Store command under lock */
            nimcp_mutex_lock(bridge->cmd_lock);
            memcpy(bridge->cmd_output, cmd_buf,
                   bridge->cmd_dim * sizeof(float));
            nimcp_mutex_unlock(bridge->cmd_lock);

            bridge->last_inference_us = nimcp_time_now_us();

#ifdef NIMCP_HAS_ROS2
            /* 5. Publish to ROS 2 */
            if (bridge->ros2_initialized &&
                bridge->config.cmd_type == NIMCP_ROS2_CMD_TWIST) {
                bridge->twist_msg.linear.x = (double)cmd_buf[0];
                bridge->twist_msg.linear.y = (double)cmd_buf[1];
                bridge->twist_msg.linear.z = (double)cmd_buf[2];
                bridge->twist_msg.angular.x = (double)cmd_buf[3];
                bridge->twist_msg.angular.y = (double)cmd_buf[4];
                bridge->twist_msg.angular.z = (double)cmd_buf[5];

                rcl_ret_t ret = rcl_publish(&bridge->pub_cmd,
                                            &bridge->twist_msg, NULL);
                if (ret != RCL_RET_OK) {
                    LOG_WARN("[ROS2_BRIDGE] Failed to publish Twist: %d",
                             (int)ret);
                }
            }
#endif
        } else {
            LOG_WARN("[ROS2_BRIDGE] Brain inference failed: %d", (int)status);
        }

        /* 6. Sleep until next cycle */
        nimcp_time_sleep_us(sleep_us);
    }

    nimcp_free(brain_output);
    nimcp_free(local_features);

    LOG_INFO("[ROS2_BRIDGE] Control loop stopped");
    return NULL;
}

/* ============================================================================
 * ROS 2 Subscription Callbacks (only when ROS 2 is available)
 * ============================================================================ */

#ifdef NIMCP_HAS_ROS2

static void ros2_imu_callback(const void* msg_in, void* context)
{
    nimcp_ros2_bridge_t* bridge = (nimcp_ros2_bridge_t*)context;
    const sensor_msgs__msg__Imu* msg = (const sensor_msgs__msg__Imu*)msg_in;
    if (!bridge || !msg) return;

    float imu_data[SENSOR_SLOT_IMU_SIZE];
    /* Quaternion orientation */
    imu_data[0] = (float)msg->orientation.x;
    imu_data[1] = (float)msg->orientation.y;
    imu_data[2] = (float)msg->orientation.z;
    imu_data[3] = (float)msg->orientation.w;
    /* Angular velocity */
    imu_data[4] = (float)msg->angular_velocity.x;
    imu_data[5] = (float)msg->angular_velocity.y;
    imu_data[6] = (float)msg->angular_velocity.z;
    /* Linear acceleration */
    imu_data[7] = (float)msg->linear_acceleration.x;
    imu_data[8] = (float)msg->linear_acceleration.y;
    imu_data[9] = (float)msg->linear_acceleration.z;

    nimcp_mutex_lock(bridge->sensor_lock);
    if (SENSOR_SLOT_IMU_OFFSET + SENSOR_SLOT_IMU_SIZE <= bridge->sensor_feature_count) {
        memcpy(&bridge->sensor_features[SENSOR_SLOT_IMU_OFFSET],
               imu_data, SENSOR_SLOT_IMU_SIZE * sizeof(float));
    }
    nimcp_mutex_unlock(bridge->sensor_lock);
}

static void ros2_odom_callback(const void* msg_in, void* context)
{
    nimcp_ros2_bridge_t* bridge = (nimcp_ros2_bridge_t*)context;
    const nav_msgs__msg__Odometry* msg = (const nav_msgs__msg__Odometry*)msg_in;
    if (!bridge || !msg) return;

    float odom_data[SENSOR_SLOT_ODOM_SIZE];
    /* Pose: position(3) + orientation quaternion(4) */
    odom_data[0] = (float)msg->pose.pose.position.x;
    odom_data[1] = (float)msg->pose.pose.position.y;
    odom_data[2] = (float)msg->pose.pose.position.z;
    odom_data[3] = (float)msg->pose.pose.orientation.x;
    odom_data[4] = (float)msg->pose.pose.orientation.y;
    odom_data[5] = (float)msg->pose.pose.orientation.z;
    odom_data[6] = (float)msg->pose.pose.orientation.w;
    /* Twist: linear(3) + angular(3) */
    odom_data[7]  = (float)msg->twist.twist.linear.x;
    odom_data[8]  = (float)msg->twist.twist.linear.y;
    odom_data[9]  = (float)msg->twist.twist.linear.z;
    odom_data[10] = (float)msg->twist.twist.angular.x;
    odom_data[11] = (float)msg->twist.twist.angular.y;
    odom_data[12] = (float)msg->twist.twist.angular.z;

    nimcp_mutex_lock(bridge->sensor_lock);
    if (SENSOR_SLOT_ODOM_OFFSET + SENSOR_SLOT_ODOM_SIZE <= bridge->sensor_feature_count) {
        memcpy(&bridge->sensor_features[SENSOR_SLOT_ODOM_OFFSET],
               odom_data, SENSOR_SLOT_ODOM_SIZE * sizeof(float));
    }
    nimcp_mutex_unlock(bridge->sensor_lock);
}

static void ros2_battery_callback(const void* msg_in, void* context)
{
    nimcp_ros2_bridge_t* bridge = (nimcp_ros2_bridge_t*)context;
    const sensor_msgs__msg__BatteryState* msg =
        (const sensor_msgs__msg__BatteryState*)msg_in;
    if (!bridge || !msg) return;

    float bat_data[SENSOR_SLOT_BATTERY_SIZE];
    bat_data[0] = msg->voltage;
    bat_data[1] = msg->current;
    bat_data[2] = msg->charge;
    bat_data[3] = msg->percentage;

    nimcp_mutex_lock(bridge->sensor_lock);
    if (SENSOR_SLOT_BATTERY_OFFSET + SENSOR_SLOT_BATTERY_SIZE <= bridge->sensor_feature_count) {
        memcpy(&bridge->sensor_features[SENSOR_SLOT_BATTERY_OFFSET],
               bat_data, SENSOR_SLOT_BATTERY_SIZE * sizeof(float));
    }
    nimcp_mutex_unlock(bridge->sensor_lock);
}

/**
 * Initialize ROS 2 node, subscriptions, and publisher.
 */
static int ros2_init_node(nimcp_ros2_bridge_t* bridge)
{
    if (!bridge) return -1;

    bridge->allocator = rcl_get_default_allocator();

    /* Init rclc support */
    rcl_ret_t ret = rclc_support_init(&bridge->support, 0, NULL,
                                       &bridge->allocator);
    if (ret != RCL_RET_OK) {
        LOG_ERROR("[ROS2_BRIDGE] rclc_support_init failed: %d", (int)ret);
        return -1;
    }

    /* Create node */
    const char* name = bridge->config.node_name
                     ? bridge->config.node_name
                     : ROS2_BRIDGE_DEFAULT_NODE_NAME;
    const char* ns = bridge->config.namespace_prefix
                   ? bridge->config.namespace_prefix
                   : ROS2_BRIDGE_DEFAULT_NAMESPACE;

    ret = rclc_node_init_default(&bridge->node, name, ns, &bridge->support);
    if (ret != RCL_RET_OK) {
        LOG_ERROR("[ROS2_BRIDGE] rclc_node_init_default failed: %d", (int)ret);
        rclc_support_fini(&bridge->support);
        return -1;
    }

    /* Count number of subscription handles needed for executor */
    uint32_t num_handles = 0;

    /* Create IMU subscription if configured */
    if (bridge->config.subscribe_imu) {
        ret = rclc_subscription_init_default(
            &bridge->sub_imu, &bridge->node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Imu),
            "/imu/data");
        if (ret == RCL_RET_OK) {
            num_handles++;
        } else {
            LOG_WARN("[ROS2_BRIDGE] Failed to create IMU subscription: %d",
                     (int)ret);
        }
    }

    /* Create odometry subscription if configured */
    if (bridge->config.subscribe_odom) {
        ret = rclc_subscription_init_default(
            &bridge->sub_odom, &bridge->node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(nav_msgs, msg, Odometry),
            "/odom");
        if (ret == RCL_RET_OK) {
            num_handles++;
        } else {
            LOG_WARN("[ROS2_BRIDGE] Failed to create odom subscription: %d",
                     (int)ret);
        }
    }

    /* Create battery subscription if configured */
    if (bridge->config.subscribe_battery) {
        ret = rclc_subscription_init_default(
            &bridge->sub_battery, &bridge->node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, BatteryState),
            "/battery_state");
        if (ret == RCL_RET_OK) {
            num_handles++;
        } else {
            LOG_WARN("[ROS2_BRIDGE] Failed to create battery subscription: %d",
                     (int)ret);
        }
    }

    /* Create Twist publisher for cmd_vel */
    const char* cmd_topic = bridge->config.cmd_topic
                          ? bridge->config.cmd_topic
                          : ROS2_BRIDGE_DEFAULT_CMD_TOPIC;
    ret = rclc_publisher_init_default(
        &bridge->pub_cmd, &bridge->node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist),
        cmd_topic);
    if (ret != RCL_RET_OK) {
        LOG_ERROR("[ROS2_BRIDGE] Failed to create cmd publisher: %d", (int)ret);
        /* Non-fatal — continue without publishing */
    }

    /* Create executor with all handles */
    if (num_handles > 0) {
        ret = rclc_executor_init(&bridge->executor, &bridge->support.context,
                                  num_handles, &bridge->allocator);
        if (ret != RCL_RET_OK) {
            LOG_ERROR("[ROS2_BRIDGE] rclc_executor_init failed: %d", (int)ret);
            return -1;
        }

        /* Add subscriptions to executor */
        if (bridge->config.subscribe_imu) {
            rclc_executor_add_subscription(
                &bridge->executor, &bridge->sub_imu,
                &bridge->imu_msg, ros2_imu_callback, ON_NEW_DATA);
        }
        if (bridge->config.subscribe_odom) {
            rclc_executor_add_subscription(
                &bridge->executor, &bridge->sub_odom,
                &bridge->odom_msg, ros2_odom_callback, ON_NEW_DATA);
        }
        if (bridge->config.subscribe_battery) {
            rclc_executor_add_subscription(
                &bridge->executor, &bridge->sub_battery,
                &bridge->battery_msg, ros2_battery_callback, ON_NEW_DATA);
        }
    }

    bridge->ros2_initialized = true;
    LOG_INFO("[ROS2_BRIDGE] ROS 2 node '%s' initialized (ns='%s', %u subs)",
             name, ns, num_handles);
    return 0;
}

/**
 * Tear down ROS 2 node and all handles.
 */
static void ros2_fini_node(nimcp_ros2_bridge_t* bridge)
{
    if (!bridge || !bridge->ros2_initialized) return;

    if (bridge->config.subscribe_imu) {
        rcl_subscription_fini(&bridge->sub_imu, &bridge->node);
    }
    if (bridge->config.subscribe_odom) {
        rcl_subscription_fini(&bridge->sub_odom, &bridge->node);
    }
    if (bridge->config.subscribe_battery) {
        rcl_subscription_fini(&bridge->sub_battery, &bridge->node);
    }

    rcl_publisher_fini(&bridge->pub_cmd, &bridge->node);
    rclc_executor_fini(&bridge->executor);
    rcl_node_fini(&bridge->node);
    rclc_support_fini(&bridge->support);

    bridge->ros2_initialized = false;
    LOG_INFO("[ROS2_BRIDGE] ROS 2 node finalized");
}

#endif /* NIMCP_HAS_ROS2 */

/* ============================================================================
 * Create / Destroy
 * ============================================================================ */

nimcp_ros2_bridge_t* nimcp_ros2_bridge_create(nimcp_brain_t brain,
                                               const nimcp_ros2_config_t* config)
{
    if (!brain) {
        LOG_ERROR("[ROS2_BRIDGE] NULL brain handle");
        return NULL;
    }

    nimcp_ros2_bridge_t* bridge =
        (nimcp_ros2_bridge_t*)nimcp_calloc(1, sizeof(nimcp_ros2_bridge_t));
    if (!bridge) {
        LOG_ERROR("[ROS2_BRIDGE] Failed to allocate bridge struct");
        return NULL;
    }

    bridge->brain = brain;
    bridge->running = false;
    bridge->started = false;
    bridge->last_inference_us = 0;

    /* Apply config (or defaults) */
    if (config) {
        memcpy(&bridge->config, config, sizeof(nimcp_ros2_config_t));
    } else {
        bridge->config = nimcp_ros2_config_default();
    }

    /* Validate / fixup config */
    if (bridge->config.brain_input_dim == 0) {
        bridge->config.brain_input_dim = ROS2_BRIDGE_DEFAULT_INPUT_DIM;
    }
    if (bridge->config.inference_rate_hz <= 0.0f) {
        bridge->config.inference_rate_hz = ROS2_BRIDGE_DEFAULT_INFER_HZ;
    }
    if (bridge->config.max_linear_vel <= 0.0f) {
        bridge->config.max_linear_vel = ROS2_BRIDGE_DEFAULT_MAX_LIN_VEL;
    }
    if (bridge->config.max_angular_vel <= 0.0f) {
        bridge->config.max_angular_vel = ROS2_BRIDGE_DEFAULT_MAX_ANG_VEL;
    }

    /* Allocate sensor feature buffer.
     * Use the larger of SENSOR_BUFFER_TOTAL_SIZE and brain_input_dim
     * to ensure the brain always has a full input vector. */
    uint32_t sensor_buf_size = bridge->config.brain_input_dim;
    if (sensor_buf_size < SENSOR_BUFFER_TOTAL_SIZE) {
        sensor_buf_size = SENSOR_BUFFER_TOTAL_SIZE;
    }
    bridge->sensor_features = (float*)nimcp_calloc(sensor_buf_size, sizeof(float));
    if (!bridge->sensor_features) {
        LOG_ERROR("[ROS2_BRIDGE] Failed to allocate sensor buffer (%u floats)",
                  sensor_buf_size);
        nimcp_free(bridge);
        return NULL;
    }
    bridge->sensor_feature_count = sensor_buf_size;

    /* Allocate command output buffer */
    bridge->cmd_dim = (bridge->config.cmd_type == NIMCP_ROS2_CMD_TWIST)
                    ? ROS2_TWIST_DIM
                    : ROS2_MAX_CMD_DIM;
    bridge->cmd_output = (float*)nimcp_calloc(bridge->cmd_dim, sizeof(float));
    if (!bridge->cmd_output) {
        LOG_ERROR("[ROS2_BRIDGE] Failed to allocate command buffer");
        nimcp_free(bridge->sensor_features);
        nimcp_free(bridge);
        return NULL;
    }

    /* Create mutexes */
    bridge->sensor_lock = nimcp_mutex_create(NULL);
    if (!bridge->sensor_lock) {
        LOG_ERROR("[ROS2_BRIDGE] Failed to create sensor mutex");
        nimcp_free(bridge->cmd_output);
        nimcp_free(bridge->sensor_features);
        nimcp_free(bridge);
        return NULL;
    }

    bridge->cmd_lock = nimcp_mutex_create(NULL);
    if (!bridge->cmd_lock) {
        LOG_ERROR("[ROS2_BRIDGE] Failed to create command mutex");
        nimcp_mutex_free(bridge->sensor_lock);
        nimcp_free(bridge->cmd_output);
        nimcp_free(bridge->sensor_features);
        nimcp_free(bridge);
        return NULL;
    }

#ifdef NIMCP_HAS_ROS2
    bridge->ros2_initialized = false;
#endif

#ifndef NIMCP_HAS_ROS2
    LOG_INFO("[ROS2_BRIDGE] Created in STUB mode (ROS 2 not available). "
             "Use nimcp_ros2_bridge_inject_sensor() for testing.");
#else
    LOG_INFO("[ROS2_BRIDGE] Created with ROS 2 support");
#endif

    return bridge;
}

void nimcp_ros2_bridge_destroy(nimcp_ros2_bridge_t* bridge)
{
    if (!bridge) return;

    /* Stop if running */
    if (bridge->running) {
        nimcp_ros2_bridge_stop(bridge);
    }

#ifdef NIMCP_HAS_ROS2
    ros2_fini_node(bridge);
#endif

    if (bridge->cmd_lock) {
        nimcp_mutex_free(bridge->cmd_lock);
    }
    if (bridge->sensor_lock) {
        nimcp_mutex_free(bridge->sensor_lock);
    }
    if (bridge->cmd_output) {
        nimcp_free(bridge->cmd_output);
    }
    if (bridge->sensor_features) {
        nimcp_free(bridge->sensor_features);
    }

    nimcp_free(bridge);
    LOG_INFO("[ROS2_BRIDGE] Destroyed");
}

/* ============================================================================
 * Start / Stop
 * ============================================================================ */

int nimcp_ros2_bridge_start(nimcp_ros2_bridge_t* bridge)
{
    if (!bridge) {
        LOG_ERROR("[ROS2_BRIDGE] NULL bridge in start()");
        return -1;
    }

    if (bridge->running) {
        LOG_WARN("[ROS2_BRIDGE] Already running");
        return 0;
    }

#ifdef NIMCP_HAS_ROS2
    /* Initialize ROS 2 node */
    if (ros2_init_node(bridge) != 0) {
        LOG_ERROR("[ROS2_BRIDGE] Failed to initialize ROS 2 node");
        return -1;
    }
#else
    LOG_INFO("[ROS2_BRIDGE] Starting in stub mode (no ROS 2). "
             "Control loop will run brain inference on injected sensor data.");
#endif

    /* Start control loop thread */
    bridge->running = true;
    nimcp_result_t result = nimcp_thread_create(
        &bridge->control_thread,
        ros2_bridge_control_loop,
        bridge,
        NULL);

    if (result != NIMCP_OK) {
        LOG_ERROR("[ROS2_BRIDGE] Failed to create control thread: %d",
                  (int)result);
        bridge->running = false;
#ifdef NIMCP_HAS_ROS2
        ros2_fini_node(bridge);
#endif
        return -1;
    }

    bridge->started = true;
    LOG_INFO("[ROS2_BRIDGE] Started (inference_rate=%.1f Hz, input_dim=%u, "
             "max_lin=%.2f m/s, max_ang=%.2f rad/s)",
             bridge->config.inference_rate_hz,
             bridge->config.brain_input_dim,
             bridge->config.max_linear_vel,
             bridge->config.max_angular_vel);
    return 0;
}

int nimcp_ros2_bridge_stop(nimcp_ros2_bridge_t* bridge)
{
    if (!bridge) {
        LOG_ERROR("[ROS2_BRIDGE] NULL bridge in stop()");
        return -1;
    }

    if (!bridge->running) {
        LOG_WARN("[ROS2_BRIDGE] Not running");
        return 0;
    }

    /* Signal thread to stop */
    bridge->running = false;

    /* Join control thread */
    if (bridge->started) {
        nimcp_result_t result = nimcp_thread_join(bridge->control_thread, NULL);
        if (result != NIMCP_OK) {
            LOG_WARN("[ROS2_BRIDGE] Thread join returned %d", (int)result);
        }
        bridge->started = false;
    }

    /* Send zero command (safety: stop the robot) */
    nimcp_mutex_lock(bridge->cmd_lock);
    memset(bridge->cmd_output, 0, bridge->cmd_dim * sizeof(float));
    nimcp_mutex_unlock(bridge->cmd_lock);

#ifdef NIMCP_HAS_ROS2
    /* Publish zero twist to stop the robot */
    if (bridge->ros2_initialized &&
        bridge->config.cmd_type == NIMCP_ROS2_CMD_TWIST) {
        memset(&bridge->twist_msg, 0, sizeof(bridge->twist_msg));
        rcl_publish(&bridge->pub_cmd, &bridge->twist_msg, NULL);
    }
    ros2_fini_node(bridge);
#endif

    LOG_INFO("[ROS2_BRIDGE] Stopped");
    return 0;
}

bool nimcp_ros2_bridge_is_running(const nimcp_ros2_bridge_t* bridge)
{
    if (!bridge) return false;
    return bridge->running;
}

/* ============================================================================
 * Sensor Injection
 * ============================================================================ */

int nimcp_ros2_bridge_inject_sensor(nimcp_ros2_bridge_t* bridge,
                                     const char* topic, const float* data,
                                     uint32_t count)
{
    if (!bridge || !data || count == 0) {
        LOG_ERROR("[ROS2_BRIDGE] Invalid args to inject_sensor()");
        return -1;
    }

    /* Determine write offset based on topic name */
    uint32_t offset = 0;
    uint32_t max_count = bridge->sensor_feature_count;

    if (topic) {
        if (strstr(topic, "lidar") || strstr(topic, "scan")) {
            offset = SENSOR_SLOT_LIDAR_OFFSET;
            max_count = SENSOR_SLOT_LIDAR_SIZE;
        } else if (strstr(topic, "imu")) {
            offset = SENSOR_SLOT_IMU_OFFSET;
            max_count = SENSOR_SLOT_IMU_SIZE;
        } else if (strstr(topic, "depth")) {
            offset = SENSOR_SLOT_DEPTH_OFFSET;
            max_count = SENSOR_SLOT_DEPTH_SIZE;
        } else if (strstr(topic, "camera") || strstr(topic, "image")) {
            offset = SENSOR_SLOT_CAMERA_OFFSET;
            max_count = SENSOR_SLOT_CAMERA_SIZE;
        } else if (strstr(topic, "odom")) {
            offset = SENSOR_SLOT_ODOM_OFFSET;
            max_count = SENSOR_SLOT_ODOM_SIZE;
        } else if (strstr(topic, "joint")) {
            offset = SENSOR_SLOT_JOINT_OFFSET;
            max_count = SENSOR_SLOT_JOINT_SIZE;
        } else if (strstr(topic, "battery")) {
            offset = SENSOR_SLOT_BATTERY_OFFSET;
            max_count = SENSOR_SLOT_BATTERY_SIZE;
        }
        /* Unknown topic: write at offset 0, full buffer size */
    }

    /* Clamp count to available slot */
    if (count > max_count) {
        count = max_count;
    }

    /* Bounds check */
    if (offset + count > bridge->sensor_feature_count) {
        LOG_ERROR("[ROS2_BRIDGE] Sensor injection out of bounds "
                  "(offset=%u, count=%u, buf=%u)",
                  offset, count, bridge->sensor_feature_count);
        return -1;
    }

    nimcp_mutex_lock(bridge->sensor_lock);
    memcpy(&bridge->sensor_features[offset], data, count * sizeof(float));
    nimcp_mutex_unlock(bridge->sensor_lock);

    return 0;
}

/* ============================================================================
 * Motor Command Output
 * ============================================================================ */

int nimcp_ros2_bridge_get_last_cmd(const nimcp_ros2_bridge_t* bridge,
                                    float* data, uint32_t max_count)
{
    if (!bridge || !data || max_count == 0) {
        return -1;
    }

    uint32_t copy_count = bridge->cmd_dim < max_count
                        ? bridge->cmd_dim
                        : max_count;

    /* Cast away const for lock — safe because we only read cmd_output */
    nimcp_mutex_lock(((nimcp_ros2_bridge_t*)bridge)->cmd_lock);
    memcpy(data, bridge->cmd_output, copy_count * sizeof(float));
    nimcp_mutex_unlock(((nimcp_ros2_bridge_t*)bridge)->cmd_lock);

    return (int)copy_count;
}
