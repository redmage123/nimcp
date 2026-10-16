/**
 * @file nimcp_power.c
 * @brief Battery-aware power management for edge brain inference and learning.
 *
 * Automatically transitions between power modes based on battery level
 * and device temperature, adjusting inference rate, learning rate,
 * and GPU usage accordingly.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#include "edge/nimcp_edge.h"
#include <string.h>

/* ============================================================================
 * nimcp_power_init
 * ============================================================================ */

int nimcp_power_init(nimcp_power_config_t* config) {
    if (!config) {
        return -1;
    }

    *config = nimcp_power_config_default();
    return 0;
}

/* ============================================================================
 * nimcp_power_update
 *
 * Check battery and temperature, transition power modes, update config.
 * Returns the current power mode after update.
 * ============================================================================ */

nimcp_power_mode_t nimcp_power_update(
    nimcp_power_config_t* config,
    float battery_pct, float temperature_c)
{
    if (!config) {
        return NIMCP_POWER_FULL;
    }

    /* If not in auto mode, return current mode unchanged */
    if (!config->auto_manage) {
        return config->mode;
    }

    /* Determine mode from battery level */
    nimcp_power_mode_t new_mode;
    if (battery_pct < config->critical_battery_pct) {
        new_mode = NIMCP_POWER_CRITICAL;
    } else if (battery_pct < config->saving_battery_pct) {
        new_mode = NIMCP_POWER_SAVING;
    } else if (battery_pct < config->balanced_battery_pct) {
        new_mode = NIMCP_POWER_BALANCED;
    } else {
        new_mode = NIMCP_POWER_FULL;
    }

    /* Thermal throttle: if temperature exceeds threshold, go to at least SAVING */
    if (temperature_c > config->thermal_throttle_c && new_mode < NIMCP_POWER_SAVING) {
        new_mode = NIMCP_POWER_SAVING;
    }

    config->mode = new_mode;

    /* Update derived parameters based on mode */
    switch (new_mode) {
        case NIMCP_POWER_FULL:
            config->inference_hz = 30.0f;
            config->learning_rate_scale = 1.0f;
            config->gpu_enabled = true;
            config->early_exit_forced = false;
            config->early_exit_threshold = 0.9f;
            break;

        case NIMCP_POWER_BALANCED:
            config->inference_hz = 15.0f;
            config->learning_rate_scale = 0.5f;
            config->gpu_enabled = true;
            config->early_exit_forced = false;
            config->early_exit_threshold = 0.85f;
            break;

        case NIMCP_POWER_SAVING:
            config->inference_hz = 5.0f;
            config->learning_rate_scale = 0.1f;
            config->gpu_enabled = false;
            config->early_exit_forced = true;
            config->early_exit_threshold = 0.7f;
            break;

        case NIMCP_POWER_CRITICAL:
            config->inference_hz = 1.0f;
            config->learning_rate_scale = 0.0f; /* No learning */
            config->gpu_enabled = false;
            config->early_exit_forced = true;
            config->early_exit_threshold = 0.5f;
            break;
    }

    return new_mode;
}

/* ============================================================================
 * Accessors
 * ============================================================================ */

float nimcp_power_get_inference_hz(const nimcp_power_config_t* config) {
    if (!config) {
        return 0.0f;
    }
    return config->inference_hz;
}

float nimcp_power_get_lr_scale(const nimcp_power_config_t* config) {
    if (!config) {
        return 0.0f;
    }
    return config->learning_rate_scale;
}

bool nimcp_power_is_gpu_enabled(const nimcp_power_config_t* config) {
    if (!config) {
        return false;
    }
    return config->gpu_enabled;
}
