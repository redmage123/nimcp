/**
 * @file nimcp_motor_output.h
 * @brief Motor command translation from brain output to actuator commands.
 *
 * Translates raw brain neural outputs into typed motor commands with
 * per-channel deadzone, scaling, clamping, and exponential smoothing.
 *
 * Usage:
 *   1. Create translator with nimcp_motor_create(&config) or a preset
 *   2. Each inference cycle: nimcp_motor_translate(mt, brain_out, n_out, motor_cmd, n_cmd)
 *   3. Destroy with nimcp_motor_destroy(mt)
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#ifndef NIMCP_MOTOR_OUTPUT_H
#define NIMCP_MOTOR_OUTPUT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Motor Types
 * ============================================================================ */

typedef enum {
    NIMCP_MOTOR_TWIST = 0,         /* 6-DOF twist (linear xyz + angular xyz) */
    NIMCP_MOTOR_JOINT_POSITION,    /* Joint position control */
    NIMCP_MOTOR_JOINT_VELOCITY,    /* Joint velocity control */
    NIMCP_MOTOR_PWM,               /* Raw PWM signals */
    NIMCP_MOTOR_DIFFERENTIAL,      /* 2-wheel differential drive */
    NIMCP_MOTOR_QUADROTOR,         /* 4-channel quadrotor (throttle per motor) */
} nimcp_motor_type_t;

/* ============================================================================
 * Channel Mapping
 * ============================================================================ */

#define NIMCP_MOTOR_MAX_CHANNELS 64

typedef struct {
    uint32_t brain_output_idx;     /* Index into brain output array */
    float scale;                   /* Multiplier (default 1.0) */
    float offset;                  /* Added after scaling (default 0.0) */
    float min_value;               /* Output clamp lower bound */
    float max_value;               /* Output clamp upper bound */
    float deadzone;                /* Deadzone threshold (default 0.0) */
} nimcp_motor_channel_t;

/* ============================================================================
 * Motor Output Config
 * ============================================================================ */

typedef struct {
    nimcp_motor_type_t type;
    nimcp_motor_channel_t channels[NIMCP_MOTOR_MAX_CHANNELS];
    uint32_t num_channels;
    float smoothing_alpha;         /* Exponential smoothing [0,1], 0=no smooth (default 0.0) */
    float global_scale;            /* Global multiplier applied to all channels (default 1.0) */
    bool enable_deadzone;          /* Enable deadzone processing (default false) */
    bool enable_smoothing;         /* Enable exponential smoothing (default false) */
} nimcp_motor_config_t;

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

typedef struct nimcp_motor_output nimcp_motor_output_t;

/* ============================================================================
 * API
 * ============================================================================ */

/**
 * @brief Create a motor output translator with the given configuration.
 * @param config Pointer to configuration. NULL uses defaults (TWIST, 6 channels, identity).
 * @return Handle, or NULL on allocation failure.
 */
nimcp_motor_output_t* nimcp_motor_create(const nimcp_motor_config_t* config);

/**
 * @brief Destroy a motor output translator and free all resources.
 * @param motor Handle. NULL-safe.
 */
void nimcp_motor_destroy(nimcp_motor_output_t* motor);

/**
 * @brief Translate brain output to motor commands.
 *
 * For each channel: read brain_output[idx] -> deadzone -> scale*global_scale+offset
 * -> clamp -> exponential smooth -> write motor_commands[channel].
 *
 * @param motor        Handle.
 * @param brain_output Array of brain outputs.
 * @param num_outputs  Number of brain output elements.
 * @param motor_commands Output array to fill with motor commands.
 * @param num_commands Size of motor_commands array (must be >= num_channels).
 * @return 0 on success, -1 on failure.
 */
int nimcp_motor_translate(nimcp_motor_output_t* motor,
                          const float* brain_output, uint32_t num_outputs,
                          float* motor_commands, uint32_t num_commands);

/**
 * @brief Get the number of channels in this translator.
 */
uint32_t nimcp_motor_get_num_channels(const nimcp_motor_output_t* motor);

/**
 * @brief Update a single channel mapping at runtime.
 * @return 0 on success, -1 if idx out of range.
 */
int nimcp_motor_set_channel(nimcp_motor_output_t* motor, uint32_t idx,
                            const nimcp_motor_channel_t* channel);

/**
 * @brief Get motor type name as string.
 */
const char* nimcp_motor_type_name(nimcp_motor_type_t type);

/* ============================================================================
 * Preset Configs
 * ============================================================================ */

/**
 * @brief Preset config for 6-DOF twist (linear+angular velocity).
 * @param max_linear_vel  Max linear velocity magnitude.
 * @param max_angular_vel Max angular velocity magnitude.
 */
nimcp_motor_config_t nimcp_motor_preset_twist(float max_linear_vel,
                                              float max_angular_vel);

/**
 * @brief Preset config for quadrotor (4 motors).
 * @param min_throttle Minimum throttle value.
 * @param max_throttle Maximum throttle value.
 */
nimcp_motor_config_t nimcp_motor_preset_quadrotor(float min_throttle,
                                                  float max_throttle);

/**
 * @brief Preset config for differential drive (2 wheels).
 * @param max_speed Maximum wheel speed.
 */
nimcp_motor_config_t nimcp_motor_preset_differential(float max_speed);

/**
 * @brief Preset config for robotic arm with per-joint limits.
 * @param num_joints   Number of joints.
 * @param joint_limits Array of [min, max] pairs (2 * num_joints floats).
 * @return Config. num_joints clamped to NIMCP_MOTOR_MAX_CHANNELS.
 */
nimcp_motor_config_t nimcp_motor_preset_arm(uint32_t num_joints,
                                            const float* joint_limits);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MOTOR_OUTPUT_H */
