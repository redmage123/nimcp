/**
 * @file nimcp_dragonfly_environment.c
 * @brief Environmental Context and Compensation Implementation
 *
 * WHAT: Models environmental factors affecting hunting
 * WHY:  Enables realistic hunting under varying conditions
 * HOW:  Environmental sensing with adaptive compensation
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include "dragonfly/nimcp_dragonfly_environment.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include <math.h>
#include <string.h>
#include <time.h>
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_environment)

//=============================================================================
// Local Helpers
//=============================================================================

static inline uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static inline float vec3_length(const float v[3]) {
    return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

//=============================================================================
// Internal Structure
//=============================================================================

struct dragonfly_environment_s {
    /* Configuration */
    environment_config_t config;

    /* Current state */
    environment_state_t state;

    /* Statistics */
    environment_stats_t stats;

    /* Adaptation */
    float adapted_light_level;
    float adapted_wind_factor;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Timing */
    uint64_t creation_time_us;
    uint64_t last_update_us;
};

//=============================================================================
// Name Functions
//=============================================================================

const char* dragonfly_light_name(light_condition_t condition) {
    switch (condition) {
        case LIGHT_BRIGHT_SUN: return "bright_sun";
        case LIGHT_OVERCAST:   return "overcast";
        case LIGHT_DUSK:       return "dusk";
        case LIGHT_DAWN:       return "dawn";
        case LIGHT_SHADE:      return "shade";
        case LIGHT_DARK:       return "dark";
        default:               return "unknown";
    }
}

const char* dragonfly_wind_name(wind_condition_t condition) {
    switch (condition) {
        case WIND_CALM:     return "calm";
        case WIND_LIGHT:    return "light";
        case WIND_MODERATE: return "moderate";
        case WIND_STRONG:   return "strong";
        case WIND_GUSTY:    return "gusty";
        case WIND_EXTREME:  return "extreme";
        default:            return "unknown";
    }
}

const char* dragonfly_terrain_name(terrain_type_t terrain) {
    switch (terrain) {
        case TERRAIN_OPEN_WATER:  return "open_water";
        case TERRAIN_WATER_EDGE:  return "water_edge";
        case TERRAIN_MEADOW:      return "meadow";
        case TERRAIN_FOREST_EDGE: return "forest_edge";
        case TERRAIN_FOREST:      return "forest";
        case TERRAIN_URBAN:       return "urban";
        default:                  return "unknown";
    }
}

//=============================================================================
// Configuration Functions
//=============================================================================

environment_config_t environment_default_config(void) {
    environment_config_t config = {
        /* Wind tolerance */
        .max_hunting_wind_ms = 8.0f,
        .wind_compensation_gain = 0.8f,
        .gust_safety_margin = 1.5f,

        /* Light tolerance */
        .min_hunting_light = 0.1f,
        .glare_sensitivity = 0.5f,

        /* Temperature */
        .min_temp_c = 15.0f,
        .max_temp_c = 40.0f,
        .optimal_temp_min_c = 20.0f,
        .optimal_temp_max_c = 30.0f,

        /* Terrain */
        .min_altitude_margin_m = 0.5f,
        .water_surface_margin_m = 0.3f,

        /* Adaptation */
        .adaptation_rate = 0.1f,
        .enable_compensation = true
    };
    return config;
}

bool environment_validate_config(const environment_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "environment_validate_config: config is NULL");
        return false;
    }

    if (config->max_hunting_wind_ms < 0.0f) {
        return false;
    }
    if (config->wind_compensation_gain < 0.0f || config->wind_compensation_gain > 1.0f) {
        return false;
    }
    if (config->gust_safety_margin < 1.0f) {
        return false;
    }

    if (config->min_hunting_light < 0.0f || config->min_hunting_light > 1.0f) {
        return false;
    }
    if (config->glare_sensitivity < 0.0f || config->glare_sensitivity > 1.0f) {
        return false;
    }

    if (config->min_temp_c >= config->max_temp_c) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "environment_validate_config: min_temp >= max_temp");
        return false;
    }
    if (config->optimal_temp_min_c < config->min_temp_c) {
        return false;
    }
    if (config->optimal_temp_max_c > config->max_temp_c) {
        return false;
    }

    if (config->min_altitude_margin_m < 0.0f) {
        return false;
    }
    if (config->water_surface_margin_m < 0.0f) {
        return false;
    }

    if (config->adaptation_rate < 0.0f || config->adaptation_rate > 1.0f) {
        return false;
    }

    return true;
}

//=============================================================================
// Internal Helpers
//=============================================================================

static light_condition_t classify_light(float level, float sun_elevation) {
    if (level < 0.05f) return LIGHT_DARK;
    if (level < 0.2f) {
        if (sun_elevation < 0.0f) return LIGHT_DARK;
        if (sun_elevation < 0.1f) return LIGHT_DUSK;  /* Or DAWN based on time */
        return LIGHT_SHADE;
    }
    if (level < 0.5f) return LIGHT_SHADE;
    if (level < 0.8f) return LIGHT_OVERCAST;
    return LIGHT_BRIGHT_SUN;
}

static wind_condition_t classify_wind(float speed, float variability) {
    if (speed < 1.0f) return WIND_CALM;
    if (speed < 3.0f) {
        if (variability > 0.5f) return WIND_GUSTY;
        return WIND_LIGHT;
    }
    if (speed < 6.0f) {
        if (variability > 0.5f) return WIND_GUSTY;
        return WIND_MODERATE;
    }
    if (speed < 10.0f) return WIND_STRONG;
    return WIND_EXTREME;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

dragonfly_environment_t dragonfly_environment_create(
    const environment_config_t* config
) {
    environment_config_t cfg = config ? *config : environment_default_config();

    if (!environment_validate_config(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_environment_create: invalid config");
        return NULL;
    }

    dragonfly_environment_t env = nimcp_calloc(1, sizeof(struct dragonfly_environment_s));
    if (!env) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dragonfly_environment_create: failed to allocate environment");
        return NULL;
    }

    env->config = cfg;
    env->creation_time_us = get_time_us();

    /* Initialize with optimal conditions */
    env->state.wind_condition = WIND_CALM;
    env->state.light_level = 0.8f;
    env->state.light_condition = LIGHT_OVERCAST;
    env->state.terrain = TERRAIN_MEADOW;
    env->state.temperature_c = 25.0f;
    env->state.is_optimal_temp = true;
    env->state.visibility_m = 1000.0f;

    env->adapted_light_level = env->state.light_level;
    env->adapted_wind_factor = 1.0f;

    env->mutex = nimcp_mutex_create(NULL);
    if (!env->mutex) {
        nimcp_free(env);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dragonfly_environment_create: failed to create mutex");
        return NULL;
    }

    return env;
}

void dragonfly_environment_destroy(dragonfly_environment_t env) {
    if (!env) return;

    if (env->mutex) {
        nimcp_mutex_destroy(env->mutex);
    }

    nimcp_free(env);
}

int dragonfly_environment_reset(dragonfly_environment_t env) {
    if (!env) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_environment_reset: env is NULL");
        return -1;
    }

    nimcp_mutex_lock(env->mutex);

    memset(&env->state, 0, sizeof(env->state));
    env->state.wind_condition = WIND_CALM;
    env->state.light_level = 0.8f;
    env->state.light_condition = LIGHT_OVERCAST;
    env->state.terrain = TERRAIN_MEADOW;
    env->state.temperature_c = 25.0f;
    env->state.is_optimal_temp = true;
    env->state.visibility_m = 1000.0f;

    env->adapted_light_level = env->state.light_level;
    env->adapted_wind_factor = 1.0f;

    nimcp_mutex_unlock(env->mutex);

    return 0;
}

//=============================================================================
// Update Functions
//=============================================================================

int dragonfly_environment_set_wind(
    dragonfly_environment_t env,
    const float wind_velocity[3],
    float variability
) {
    if (!env || !wind_velocity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_environment_set_wind: env or wind_velocity is NULL");
        return -1;
    }

    nimcp_mutex_lock(env->mutex);

    memcpy(env->state.wind_velocity, wind_velocity, sizeof(env->state.wind_velocity));
    env->state.wind_variability = nimcp_clampf(variability, 0.0f, 1.0f);

    float speed = vec3_length(wind_velocity);
    env->state.wind_condition = classify_wind(speed, variability);

    {
        float new_avg = (env->stats.avg_wind_speed_ms * env->stats.updates + speed) /
                        (env->stats.updates + 1);
        if (isfinite(new_avg)) env->stats.avg_wind_speed_ms = new_avg;
    }

    nimcp_mutex_unlock(env->mutex);

    return 0;
}

int dragonfly_environment_set_light(
    dragonfly_environment_t env,
    float light_level,
    float sun_elevation_rad,
    float sun_azimuth_rad
) {
    if (!env) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_environment_set_light: env is NULL");
        return -1;
    }

    nimcp_mutex_lock(env->mutex);

    env->state.light_level = nimcp_clampf(light_level, 0.0f, 1.0f);
    env->state.sun_elevation_rad = sun_elevation_rad;
    env->state.sun_azimuth_rad = sun_azimuth_rad;
    env->state.light_condition = classify_light(light_level, sun_elevation_rad);

    /* Adapt to light level */
    env->adapted_light_level += (light_level - env->adapted_light_level) * env->config.adaptation_rate;

    {
        float new_avg = (env->stats.avg_light_level * env->stats.updates + light_level) /
                        (env->stats.updates + 1);
        if (isfinite(new_avg)) env->stats.avg_light_level = new_avg;
    }

    nimcp_mutex_unlock(env->mutex);

    return 0;
}

int dragonfly_environment_set_terrain(
    dragonfly_environment_t env,
    terrain_type_t terrain,
    float complexity,
    float water_level
) {
    if (!env) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_environment_set_terrain: env is NULL");
        return -1;
    }

    nimcp_mutex_lock(env->mutex);

    env->state.terrain = terrain;
    env->state.terrain_complexity = nimcp_clampf(complexity, 0.0f, 1.0f);
    env->state.water_surface_level = water_level;

    nimcp_mutex_unlock(env->mutex);

    return 0;
}

int dragonfly_environment_set_temperature(
    dragonfly_environment_t env,
    float temperature_c
) {
    if (!env) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_environment_set_temperature: env is NULL");
        return -1;
    }

    nimcp_mutex_lock(env->mutex);

    env->state.temperature_c = temperature_c;
    env->state.is_optimal_temp = (temperature_c >= env->config.optimal_temp_min_c &&
                                  temperature_c <= env->config.optimal_temp_max_c);

    nimcp_mutex_unlock(env->mutex);

    return 0;
}

int dragonfly_environment_update(
    dragonfly_environment_t env,
    const environment_state_t* state
) {
    if (!env || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_environment_update: env or state is NULL");
        return -1;
    }

    nimcp_mutex_lock(env->mutex);

    env->state = *state;

    /* Classify conditions */
    float wind_speed = vec3_length(state->wind_velocity);
    env->state.wind_condition = classify_wind(wind_speed, state->wind_variability);
    env->state.light_condition = classify_light(state->light_level, state->sun_elevation_rad);
    env->state.is_optimal_temp = (state->temperature_c >= env->config.optimal_temp_min_c &&
                                  state->temperature_c <= env->config.optimal_temp_max_c);

    /* Update statistics */
    env->stats.updates++;
    {
        float new_avg_wind = (env->stats.avg_wind_speed_ms * (env->stats.updates - 1) + wind_speed) /
                              env->stats.updates;
        if (isfinite(new_avg_wind)) env->stats.avg_wind_speed_ms = new_avg_wind;
        float new_avg_light = (env->stats.avg_light_level * (env->stats.updates - 1) + state->light_level) /
                               env->stats.updates;
        if (isfinite(new_avg_light)) env->stats.avg_light_level = new_avg_light;
    }

    env->last_update_us = get_time_us();

    nimcp_mutex_unlock(env->mutex);

    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

int dragonfly_environment_get_state(
    const dragonfly_environment_t env,
    environment_state_t* state
) {
    if (!env || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_environment_get_state: env or state is NULL");
        return -1;
    }

    nimcp_mutex_lock(env->mutex);
    *state = env->state;
    nimcp_mutex_unlock(env->mutex);

    return 0;
}

int dragonfly_environment_get_compensation(
    const dragonfly_environment_t env,
    const float target_direction[3],
    environment_compensation_t* compensation
) {
    if (!env || !compensation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_environment_get_compensation: env or compensation is NULL");
        return -1;
    }

    nimcp_mutex_lock(env->mutex);

    /* Wind compensation */
    float wind_speed = vec3_length(env->state.wind_velocity);
    for (int i = 0; i < 3; i++) {
        compensation->wind_correction[i] = -env->state.wind_velocity[i] * env->config.wind_compensation_gain;
    }

    /* Heading correction (simple approximation) */
    if (target_direction && wind_speed > 0.5f) {
        float cross = target_direction[0] * env->state.wind_velocity[1] -
                      target_direction[1] * env->state.wind_velocity[0];
        compensation->heading_correction_rad = atanf(cross * 0.1f);
    } else {
        compensation->heading_correction_rad = 0.0f;
    }

    compensation->speed_adjustment = 1.0f + wind_speed * 0.05f;

    /* Visual compensation */
    float light_ratio = env->state.light_level / (env->adapted_light_level + 0.01f);
    compensation->contrast_boost = (light_ratio < 0.5f) ? 1.5f : 1.0f;
    compensation->attention_threshold_adjust = (env->state.light_level < 0.3f) ? 0.2f : 0.0f;

    /* Check for backlight */
    compensation->backlight_warning = (env->state.sun_elevation_rad > 0.0f &&
                                       env->state.sun_elevation_rad < 0.5f &&
                                       env->state.light_level > 0.7f);

    /* Terrain compensation */
    switch (env->state.terrain) {
        case TERRAIN_OPEN_WATER:
            compensation->altitude_floor_m = env->state.water_surface_level + env->config.water_surface_margin_m;
            compensation->altitude_ceiling_m = 10.0f;
            compensation->water_reflection_risk = (env->state.light_level > 0.7f);
            break;
        case TERRAIN_WATER_EDGE:
            compensation->altitude_floor_m = env->state.water_surface_level + env->config.water_surface_margin_m;
            compensation->altitude_ceiling_m = 5.0f;
            compensation->water_reflection_risk = (env->state.light_level > 0.6f);
            break;
        case TERRAIN_FOREST:
            compensation->altitude_floor_m = 2.0f + env->config.min_altitude_margin_m;
            compensation->altitude_ceiling_m = 3.0f;
            compensation->water_reflection_risk = false;
            break;
        default:
            compensation->altitude_floor_m = env->config.min_altitude_margin_m;
            compensation->altitude_ceiling_m = 10.0f;
            compensation->water_reflection_risk = false;
            break;
    }

    /* Overall suitability */
    float suitability = 1.0f;

    /* Wind factor */
    if (wind_speed > env->config.max_hunting_wind_ms) {
        suitability = 0.0f;
        compensation->limiting_factor = "wind_too_strong";
    } else {
        suitability *= 1.0f - (wind_speed / env->config.max_hunting_wind_ms) * 0.5f;
    }

    /* Light factor */
    if (env->state.light_level < env->config.min_hunting_light) {
        suitability = 0.0f;
        compensation->limiting_factor = "too_dark";
    } else if (env->state.light_condition == LIGHT_DARK) {
        suitability = 0.0f;
        compensation->limiting_factor = "too_dark";
    }

    /* Temperature factor */
    if (env->state.temperature_c < env->config.min_temp_c ||
        env->state.temperature_c > env->config.max_temp_c) {
        suitability *= 0.3f;
        if (suitability < 0.5f) {
            compensation->limiting_factor = "temperature";
        }
    } else if (!env->state.is_optimal_temp) {
        suitability *= 0.8f;
    }

    /* Rain */
    if (env->state.is_raining) {
        suitability *= 0.2f;
        compensation->limiting_factor = "rain";
    }

    compensation->hunting_suitability = nimcp_clampf(suitability, 0.0f, 1.0f);
    compensation->hunting_recommended = (suitability > 0.3f);

    if (suitability > 0.3f && !compensation->limiting_factor) {
        compensation->limiting_factor = "none";
    }

    nimcp_mutex_unlock(env->mutex);

    return 0;
}

bool dragonfly_environment_hunting_ok(const dragonfly_environment_t env) {
    if (!env) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_environment_hunting_ok: env is NULL");
        return false;
    }

    environment_compensation_t comp;
    dragonfly_environment_get_compensation(env, NULL, &comp);
    return comp.hunting_recommended;
}

int dragonfly_environment_correct_velocity(
    const dragonfly_environment_t env,
    const float desired_velocity[3],
    float corrected_velocity[3]
) {
    if (!env || !desired_velocity || !corrected_velocity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_environment_correct_velocity: env, desired_velocity, or corrected_velocity is NULL");
        return -1;
    }

    nimcp_mutex_lock(env->mutex);

    for (int i = 0; i < 3; i++) {
        corrected_velocity[i] = desired_velocity[i] -
                                env->state.wind_velocity[i] * env->config.wind_compensation_gain;
    }

    nimcp_mutex_unlock(env->mutex);

    return 0;
}

int dragonfly_environment_get_stats(
    const dragonfly_environment_t env,
    environment_stats_t* stats
) {
    if (!env || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_environment_get_stats: env or stats is NULL");
        return -1;
    }

    nimcp_mutex_lock(env->mutex);
    *stats = env->stats;
    nimcp_mutex_unlock(env->mutex);

    return 0;
}
