/**
 * @file nimcp_motor_output.c
 * @brief Motor command translation from brain output to actuator commands.
 *
 * Translates raw brain neural outputs into typed motor commands with
 * per-channel deadzone, scaling, clamping, and exponential smoothing.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#include "edge/nimcp_motor_output.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <math.h>

#define LOG_MODULE "MOTOR_OUTPUT"

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct nimcp_motor_output {
    nimcp_motor_config_t config;
    float* smoothed_output;        /* Exponentially smoothed values per channel */
    bool first_translate;          /* True until first translate call */
};

/* ============================================================================
 * Type Name
 * ============================================================================ */

const char* nimcp_motor_type_name(nimcp_motor_type_t type) {
    switch (type) {
        case NIMCP_MOTOR_TWIST:          return "TWIST";
        case NIMCP_MOTOR_JOINT_POSITION: return "JOINT_POSITION";
        case NIMCP_MOTOR_JOINT_VELOCITY: return "JOINT_VELOCITY";
        case NIMCP_MOTOR_PWM:            return "PWM";
        case NIMCP_MOTOR_DIFFERENTIAL:   return "DIFFERENTIAL";
        case NIMCP_MOTOR_QUADROTOR:      return "QUADROTOR";
        default:                         return "UNKNOWN";
    }
}

/* ============================================================================
 * Create / Destroy
 * ============================================================================ */

nimcp_motor_output_t* nimcp_motor_create(const nimcp_motor_config_t* config) {
    nimcp_motor_output_t* mt = (nimcp_motor_output_t*)nimcp_calloc(
        1, sizeof(nimcp_motor_output_t));
    if (!mt) {
        LOG_ERROR("[%s] Failed to allocate motor output translator", LOG_MODULE);
        return NULL;
    }

    /* Apply config or defaults */
    if (config) {
        memcpy(&mt->config, config, sizeof(nimcp_motor_config_t));
    } else {
        /* Default: TWIST with 6 identity-mapped channels */
        mt->config = nimcp_motor_preset_twist(1.0f, 1.0f);
    }

    /* Clamp num_channels */
    if (mt->config.num_channels == 0) {
        LOG_WARN("[%s] num_channels is 0, defaulting to 1", LOG_MODULE);
        mt->config.num_channels = 1;
    }
    if (mt->config.num_channels > NIMCP_MOTOR_MAX_CHANNELS) {
        LOG_WARN("[%s] num_channels %u clamped to %d",
                 LOG_MODULE, mt->config.num_channels, NIMCP_MOTOR_MAX_CHANNELS);
        mt->config.num_channels = NIMCP_MOTOR_MAX_CHANNELS;
    }

    /* Default global_scale */
    if (mt->config.global_scale == 0.0f) {
        mt->config.global_scale = 1.0f;
    }

    /* Clamp smoothing_alpha to [0, 1] */
    if (mt->config.smoothing_alpha < 0.0f) {
        mt->config.smoothing_alpha = 0.0f;
    }
    if (mt->config.smoothing_alpha > 1.0f) {
        mt->config.smoothing_alpha = 1.0f;
    }

    /* Allocate smoothing buffer */
    mt->smoothed_output = (float*)nimcp_calloc(
        mt->config.num_channels, sizeof(float));
    if (!mt->smoothed_output) {
        LOG_ERROR("[%s] Failed to allocate smoothing buffer", LOG_MODULE);
        nimcp_free(mt);
        return NULL;
    }

    mt->first_translate = true;

    LOG_INFO("[%s] Created motor output: type=%s, channels=%u, global_scale=%.3f",
             LOG_MODULE, nimcp_motor_type_name(mt->config.type),
             mt->config.num_channels, mt->config.global_scale);

    return mt;
}

void nimcp_motor_destroy(nimcp_motor_output_t* motor) {
    if (!motor) {
        return;
    }
    nimcp_free(motor->smoothed_output);
    nimcp_free(motor);
    LOG_DEBUG("[%s] Destroyed motor output translator", LOG_MODULE);
}

/* ============================================================================
 * Translate
 * ============================================================================ */

int nimcp_motor_translate(nimcp_motor_output_t* motor,
                          const float* brain_output, uint32_t num_outputs,
                          float* motor_commands, uint32_t num_commands) {
    if (!motor || !brain_output || !motor_commands) {
        return -1;
    }

    uint32_t n = motor->config.num_channels;
    if (num_commands < n) {
        LOG_ERROR("[%s] motor_commands buffer too small: %u < %u",
                  LOG_MODULE, num_commands, n);
        return -1;
    }

    for (uint32_t i = 0; i < n; i++) {
        nimcp_motor_channel_t* ch = &motor->config.channels[i];

        /* Read brain output (bounds check) */
        float raw = 0.0f;
        if (ch->brain_output_idx < num_outputs) {
            raw = brain_output[ch->brain_output_idx];
        }

        /* NaN/Inf safety */
        if (!isfinite(raw)) {
            raw = 0.0f;
        }

        /* Deadzone — clamp to < 1.0 to prevent division by zero */
        if (motor->config.enable_deadzone && ch->deadzone > 0.0f) {
            if (ch->deadzone >= 1.0f) ch->deadzone = 0.999f;
            if (fabsf(raw) < ch->deadzone) {
                raw = 0.0f;
            } else {
                /* Rescale so output is continuous after deadzone */
                float sign = (raw > 0.0f) ? 1.0f : -1.0f;
                raw = sign * (fabsf(raw) - ch->deadzone) / (1.0f - ch->deadzone);
            }
        }

        /* Scale + global scale + offset */
        float value = raw * ch->scale * motor->config.global_scale + ch->offset;

        /* Clamp */
        if (ch->min_value < ch->max_value) {
            if (value < ch->min_value) value = ch->min_value;
            if (value > ch->max_value) value = ch->max_value;
        }

        /* Exponential smoothing */
        if (motor->config.enable_smoothing && motor->config.smoothing_alpha > 0.0f) {
            if (motor->first_translate) {
                motor->smoothed_output[i] = value;
            } else {
                float alpha = motor->config.smoothing_alpha;
                motor->smoothed_output[i] = alpha * value +
                    (1.0f - alpha) * motor->smoothed_output[i];
            }
            value = motor->smoothed_output[i];
        } else {
            motor->smoothed_output[i] = value;
        }

        motor_commands[i] = value;
    }

    motor->first_translate = false;
    return 0;
}

/* ============================================================================
 * Accessors
 * ============================================================================ */

uint32_t nimcp_motor_get_num_channels(const nimcp_motor_output_t* motor) {
    if (!motor) {
        return 0;
    }
    return motor->config.num_channels;
}

int nimcp_motor_set_channel(nimcp_motor_output_t* motor, uint32_t idx,
                            const nimcp_motor_channel_t* channel) {
    if (!motor || !channel) {
        return -1;
    }
    if (idx >= motor->config.num_channels) {
        LOG_ERROR("[%s] Channel index %u out of range (max %u)",
                  LOG_MODULE, idx, motor->config.num_channels);
        return -1;
    }
    memcpy(&motor->config.channels[idx], channel, sizeof(nimcp_motor_channel_t));
    return 0;
}

/* ============================================================================
 * Presets
 * ============================================================================ */

nimcp_motor_config_t nimcp_motor_preset_twist(float max_linear_vel,
                                              float max_angular_vel) {
    nimcp_motor_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.type = NIMCP_MOTOR_TWIST;
    cfg.num_channels = 6;
    cfg.global_scale = 1.0f;
    cfg.enable_deadzone = false;
    cfg.enable_smoothing = false;
    cfg.smoothing_alpha = 0.0f;

    /* Linear xyz (channels 0-2) */
    for (uint32_t i = 0; i < 3; i++) {
        cfg.channels[i].brain_output_idx = i;
        cfg.channels[i].scale = max_linear_vel;
        cfg.channels[i].offset = 0.0f;
        cfg.channels[i].min_value = -max_linear_vel;
        cfg.channels[i].max_value = max_linear_vel;
        cfg.channels[i].deadzone = 0.0f;
    }
    /* Angular xyz (channels 3-5) */
    for (uint32_t i = 3; i < 6; i++) {
        cfg.channels[i].brain_output_idx = i;
        cfg.channels[i].scale = max_angular_vel;
        cfg.channels[i].offset = 0.0f;
        cfg.channels[i].min_value = -max_angular_vel;
        cfg.channels[i].max_value = max_angular_vel;
        cfg.channels[i].deadzone = 0.0f;
    }
    return cfg;
}

nimcp_motor_config_t nimcp_motor_preset_quadrotor(float min_throttle,
                                                  float max_throttle) {
    nimcp_motor_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.type = NIMCP_MOTOR_QUADROTOR;
    cfg.num_channels = 4;
    cfg.global_scale = 1.0f;
    cfg.enable_deadzone = false;
    cfg.enable_smoothing = false;
    cfg.smoothing_alpha = 0.0f;

    for (uint32_t i = 0; i < 4; i++) {
        cfg.channels[i].brain_output_idx = i;
        cfg.channels[i].scale = max_throttle - min_throttle;
        cfg.channels[i].offset = min_throttle;
        cfg.channels[i].min_value = min_throttle;
        cfg.channels[i].max_value = max_throttle;
        cfg.channels[i].deadzone = 0.0f;
    }
    return cfg;
}

nimcp_motor_config_t nimcp_motor_preset_differential(float max_speed) {
    nimcp_motor_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.type = NIMCP_MOTOR_DIFFERENTIAL;
    cfg.num_channels = 2;
    cfg.global_scale = 1.0f;
    cfg.enable_deadzone = false;
    cfg.enable_smoothing = false;
    cfg.smoothing_alpha = 0.0f;

    for (uint32_t i = 0; i < 2; i++) {
        cfg.channels[i].brain_output_idx = i;
        cfg.channels[i].scale = max_speed;
        cfg.channels[i].offset = 0.0f;
        cfg.channels[i].min_value = -max_speed;
        cfg.channels[i].max_value = max_speed;
        cfg.channels[i].deadzone = 0.0f;
    }
    return cfg;
}

nimcp_motor_config_t nimcp_motor_preset_arm(uint32_t num_joints,
                                            const float* joint_limits) {
    nimcp_motor_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.type = NIMCP_MOTOR_JOINT_POSITION;
    cfg.global_scale = 1.0f;
    cfg.enable_deadzone = false;
    cfg.enable_smoothing = false;
    cfg.smoothing_alpha = 0.0f;

    if (num_joints > NIMCP_MOTOR_MAX_CHANNELS) {
        num_joints = NIMCP_MOTOR_MAX_CHANNELS;
    }
    cfg.num_channels = num_joints;

    for (uint32_t i = 0; i < num_joints; i++) {
        cfg.channels[i].brain_output_idx = i;

        float lo = -3.14159f;
        float hi = 3.14159f;
        if (joint_limits) {
            lo = joint_limits[2 * i];
            hi = joint_limits[2 * i + 1];
        }

        /* Scale from [-1, 1] brain output to [lo, hi] */
        cfg.channels[i].scale = (hi - lo) * 0.5f;
        cfg.channels[i].offset = (hi + lo) * 0.5f;
        cfg.channels[i].min_value = lo;
        cfg.channels[i].max_value = hi;
        cfg.channels[i].deadzone = 0.0f;
    }
    return cfg;
}
