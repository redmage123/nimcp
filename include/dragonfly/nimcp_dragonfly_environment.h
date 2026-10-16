/**
 * @file nimcp_dragonfly_environment.h
 * @brief Environmental Context and Compensation
 *
 * BIOLOGICAL REFERENCE:
 * Dragonflies hunt in various environmental conditions and must
 * compensate for wind, lighting, and terrain. They often hunt
 * near water where conditions change rapidly.
 *
 * WHAT: Models environmental factors affecting hunting
 * WHY:  Enables realistic hunting under varying conditions
 * HOW:  Environmental sensing with adaptive compensation
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#ifndef NIMCP_DRAGONFLY_ENVIRONMENT_H
#define NIMCP_DRAGONFLY_ENVIRONMENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct dragonfly_environment_s* dragonfly_environment_t;

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Light condition
 */
typedef enum {
    LIGHT_BRIGHT_SUN,         /**< Direct sunlight */
    LIGHT_OVERCAST,           /**< Cloudy/diffuse */
    LIGHT_DUSK,               /**< Low light dusk */
    LIGHT_DAWN,               /**< Low light dawn */
    LIGHT_SHADE,              /**< In shadow */
    LIGHT_DARK                /**< Too dark for hunting */
} light_condition_t;

/**
 * @brief Wind condition
 */
typedef enum {
    WIND_CALM,                /**< No wind */
    WIND_LIGHT,               /**< Light breeze */
    WIND_MODERATE,            /**< Moderate wind */
    WIND_STRONG,              /**< Strong wind */
    WIND_GUSTY,               /**< Variable gusts */
    WIND_EXTREME              /**< Too windy for hunting */
} wind_condition_t;

/**
 * @brief Terrain type
 */
typedef enum {
    TERRAIN_OPEN_WATER,       /**< Over water */
    TERRAIN_WATER_EDGE,       /**< Near water edge */
    TERRAIN_MEADOW,           /**< Open meadow */
    TERRAIN_FOREST_EDGE,      /**< Forest clearing edge */
    TERRAIN_FOREST,           /**< Dense vegetation */
    TERRAIN_URBAN             /**< Urban environment */
} terrain_type_t;

/**
 * @brief Environmental state
 */
typedef struct {
    /* Atmospheric */
    float wind_velocity[3];       /**< Wind velocity (m/s) */
    float wind_variability;       /**< Wind gust factor [0,1] */
    wind_condition_t wind_condition; /**< Wind classification */

    /* Lighting */
    float light_level;            /**< Ambient light [0,1] */
    float sun_elevation_rad;      /**< Sun elevation angle */
    float sun_azimuth_rad;        /**< Sun azimuth angle */
    light_condition_t light_condition; /**< Light classification */

    /* Terrain */
    terrain_type_t terrain;       /**< Current terrain type */
    float terrain_complexity;     /**< Obstacle density [0,1] */
    float water_surface_level;    /**< Water surface height */

    /* Temperature */
    float temperature_c;          /**< Ambient temperature */
    bool is_optimal_temp;         /**< In optimal hunting temp range */

    /* Weather */
    bool is_raining;              /**< Rain detected */
    float visibility_m;           /**< Visibility distance */
} environment_state_t;

/**
 * @brief Environmental compensation
 */
typedef struct {
    /* Wind compensation */
    float wind_correction[3];     /**< Velocity correction for wind */
    float heading_correction_rad; /**< Heading correction */
    float speed_adjustment;       /**< Speed adjustment factor */

    /* Visual compensation */
    float contrast_boost;         /**< Contrast enhancement needed */
    float attention_threshold_adjust; /**< Adjusted attention threshold */
    bool backlight_warning;       /**< Target may be backlit */

    /* Terrain compensation */
    float altitude_floor_m;       /**< Minimum safe altitude */
    float altitude_ceiling_m;     /**< Maximum useful altitude */
    bool water_reflection_risk;   /**< Potential glare from water */

    /* Overall assessment */
    float hunting_suitability;    /**< Overall suitability [0,1] */
    bool hunting_recommended;     /**< Should hunting proceed? */
    const char* limiting_factor;  /**< Main limiting factor */
} environment_compensation_t;

/**
 * @brief Environment configuration
 */
typedef struct {
    /* Wind tolerance */
    float max_hunting_wind_ms;    /**< Maximum huntable wind speed */
    float wind_compensation_gain; /**< Wind compensation aggressiveness */
    float gust_safety_margin;     /**< Safety margin for gusts */

    /* Light tolerance */
    float min_hunting_light;      /**< Minimum light for hunting */
    float glare_sensitivity;      /**< Sensitivity to glare/backlight */

    /* Temperature */
    float min_temp_c;             /**< Minimum active temperature */
    float max_temp_c;             /**< Maximum active temperature */
    float optimal_temp_min_c;     /**< Optimal range minimum */
    float optimal_temp_max_c;     /**< Optimal range maximum */

    /* Terrain */
    float min_altitude_margin_m;  /**< Margin above obstacles */
    float water_surface_margin_m; /**< Margin above water */

    /* Adaptation */
    float adaptation_rate;        /**< Rate of condition adaptation */
    bool enable_compensation;     /**< Enable active compensation */
} environment_config_t;

/**
 * @brief Environment statistics
 */
typedef struct {
    uint64_t updates;             /**< Total updates */
    float avg_wind_speed_ms;      /**< Average wind speed */
    float avg_light_level;        /**< Average light level */
    float time_in_optimal_s;      /**< Time in optimal conditions */
    uint64_t hunts_blocked_wind;  /**< Blocked by wind */
    uint64_t hunts_blocked_light; /**< Blocked by light */
    uint64_t hunts_blocked_temp;  /**< Blocked by temperature */
} environment_stats_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default environment configuration
 */
environment_config_t environment_default_config(void);

/**
 * @brief Validate environment configuration
 */
bool environment_validate_config(const environment_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create environment system
 */
dragonfly_environment_t dragonfly_environment_create(
    const environment_config_t* config
);

/**
 * @brief Destroy environment system
 */
void dragonfly_environment_destroy(dragonfly_environment_t env);

/**
 * @brief Reset environment system
 */
int dragonfly_environment_reset(dragonfly_environment_t env);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Update wind state
 */
int dragonfly_environment_set_wind(
    dragonfly_environment_t env,
    const float wind_velocity[3],
    float variability
);

/**
 * @brief Update light conditions
 */
int dragonfly_environment_set_light(
    dragonfly_environment_t env,
    float light_level,
    float sun_elevation_rad,
    float sun_azimuth_rad
);

/**
 * @brief Update terrain
 */
int dragonfly_environment_set_terrain(
    dragonfly_environment_t env,
    terrain_type_t terrain,
    float complexity,
    float water_level
);

/**
 * @brief Update temperature
 */
int dragonfly_environment_set_temperature(
    dragonfly_environment_t env,
    float temperature_c
);

/**
 * @brief Full environment update
 */
int dragonfly_environment_update(
    dragonfly_environment_t env,
    const environment_state_t* state
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get current environment state
 */
int dragonfly_environment_get_state(
    const dragonfly_environment_t env,
    environment_state_t* state
);

/**
 * @brief Get environment compensation
 */
int dragonfly_environment_get_compensation(
    const dragonfly_environment_t env,
    const float target_direction[3],
    environment_compensation_t* compensation
);

/**
 * @brief Check if hunting is suitable
 */
bool dragonfly_environment_hunting_ok(const dragonfly_environment_t env);

/**
 * @brief Get wind-corrected intercept velocity
 */
int dragonfly_environment_correct_velocity(
    const dragonfly_environment_t env,
    const float desired_velocity[3],
    float corrected_velocity[3]
);

/**
 * @brief Get environment statistics
 */
int dragonfly_environment_get_stats(
    const dragonfly_environment_t env,
    environment_stats_t* stats
);

/**
 * @brief Get light condition name
 */
const char* dragonfly_light_name(light_condition_t condition);

/**
 * @brief Get wind condition name
 */
const char* dragonfly_wind_name(wind_condition_t condition);

/**
 * @brief Get terrain name
 */
const char* dragonfly_terrain_name(terrain_type_t terrain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_ENVIRONMENT_H */
