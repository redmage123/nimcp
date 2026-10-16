/**
 * @file nimcp_dragonfly_sleep_bridge.h
 * @brief Sleep-Dragonfly Integration Bridge
 *
 * BIOLOGICAL REFERENCE:
 * Dragonflies have circadian activity patterns, being most active
 * during daylight hours. Sleep/rest periods allow for memory
 * consolidation of successful hunting strategies.
 *
 * WHAT: Integrates dragonfly hunting with sleep/wake cycles
 * WHY:  Enables realistic circadian behavior and strategy learning
 * HOW:  Bidirectional sleep system communication
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#ifndef NIMCP_DRAGONFLY_SLEEP_BRIDGE_H
#define NIMCP_DRAGONFLY_SLEEP_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "dragonfly/nimcp_dragonfly.h"

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct dragonfly_sleep_bridge_s* dragonfly_sleep_bridge_t;
typedef struct sleep_orchestrator_s* sleep_orchestrator_t;

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Hunting experience for consolidation
 */
typedef struct {
    /* Target information */
    uint32_t target_id;
    float target_initial_pos[3];
    float target_velocity[3];
    float target_size;

    /* Hunt parameters */
    float intercept_point[3];
    float lead_angle_rad;
    float pursuit_duration_s;
    intercept_strategy_t strategy_used;

    /* Outcome */
    bool success;
    float miss_distance;
    const char* failure_reason;

    /* Timing */
    uint64_t timestamp_us;
} hunting_experience_t;

/**
 * @brief Consolidated hunting memory
 */
typedef struct {
    /* Strategy effectiveness */
    float strategy_scores[5];      /**< Score per intercept strategy */
    uint32_t strategy_counts[5];   /**< Use count per strategy */

    /* Prey-type learning */
    float prey_success_rates[10];  /**< Success rate per prey type */
    float prey_optimal_leads[10];  /**< Optimal lead angle per prey type */

    /* Environmental associations */
    float time_of_day_success[24]; /**< Success rate by hour */
    float location_scores[16];     /**< Spatial success distribution */

    /* Consolidation metrics */
    uint32_t experiences_processed;
    float consolidation_quality;
    uint64_t last_consolidation_us;
} consolidated_memory_t;

/**
 * @brief Sleep bridge configuration
 */
typedef struct {
    /* Consolidation settings */
    bool enable_memory_consolidation;  /**< Enable experience consolidation */
    uint32_t min_experiences_to_consolidate; /**< Minimum experiences needed */
    float consolidation_threshold;     /**< Sleep depth for consolidation */

    /* Circadian modulation */
    bool enable_circadian_modulation;  /**< Modulate hunting by time */
    float dawn_activity_boost;         /**< Activity boost at dawn */
    float dusk_activity_boost;         /**< Activity boost at dusk */
    float night_activity_floor;        /**< Minimum night activity */

    /* Wake triggers */
    float prey_wake_threshold;         /**< Prey salience to wake */
    float predator_wake_threshold;     /**< Predator threat to wake */

    /* Rest triggers */
    float fatigue_rest_threshold;      /**< Fatigue level to rest */
    float hunt_failure_rest_boost;     /**< Rest boost after failures */
} dragonfly_sleep_config_t;

/**
 * @brief Sleep bridge state
 */
typedef struct {
    /* Current state */
    bool is_hunting_allowed;           /**< Hunting currently allowed */
    float activity_level;              /**< Current activity level [0,1] */
    float fatigue_level;               /**< Current fatigue [0,1] */

    /* Circadian */
    float circadian_phase;             /**< Current circadian phase [0,1] */
    bool is_peak_hunting_time;         /**< Peak hunting window */

    /* Memory */
    uint32_t pending_experiences;      /**< Experiences awaiting consolidation */
    bool consolidation_in_progress;    /**< Currently consolidating */
} dragonfly_sleep_state_t;

/**
 * @brief Sleep bridge statistics
 */
typedef struct {
    uint64_t experiences_recorded;     /**< Total experiences recorded */
    uint64_t consolidations_completed; /**< Consolidation cycles completed */
    uint64_t wake_events;              /**< Times woken for hunting */
    uint64_t rest_events;              /**< Times rested */
    float avg_consolidation_quality;   /**< Average consolidation quality */
    float total_rest_time_s;           /**< Total rest time */
} dragonfly_sleep_stats_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default sleep bridge configuration
 */
dragonfly_sleep_config_t dragonfly_sleep_default_config(void);

/**
 * @brief Validate sleep bridge configuration
 */
bool dragonfly_sleep_validate_config(const dragonfly_sleep_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create sleep bridge
 */
dragonfly_sleep_bridge_t dragonfly_sleep_bridge_create(
    const dragonfly_sleep_config_t* config
);

/**
 * @brief Destroy sleep bridge
 */
void dragonfly_sleep_bridge_destroy(dragonfly_sleep_bridge_t bridge);

/**
 * @brief Connect to systems
 */
int dragonfly_sleep_bridge_connect(
    dragonfly_sleep_bridge_t bridge,
    dragonfly_system_t* dragonfly,
    sleep_orchestrator_t sleep
);

/**
 * @brief Disconnect from systems
 */
int dragonfly_sleep_bridge_disconnect(dragonfly_sleep_bridge_t bridge);

//=============================================================================
// Experience Recording
//=============================================================================

/**
 * @brief Record hunting experience for later consolidation
 */
int dragonfly_sleep_record_experience(
    dragonfly_sleep_bridge_t bridge,
    const hunting_experience_t* experience
);

/**
 * @brief Record hunt success
 */
int dragonfly_sleep_record_success(
    dragonfly_sleep_bridge_t bridge,
    uint32_t target_id,
    float miss_distance,
    intercept_strategy_t strategy
);

/**
 * @brief Record hunt failure
 */
int dragonfly_sleep_record_failure(
    dragonfly_sleep_bridge_t bridge,
    uint32_t target_id,
    const char* reason,
    intercept_strategy_t strategy
);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Update sleep bridge
 */
int dragonfly_sleep_bridge_update(
    dragonfly_sleep_bridge_t bridge,
    float dt_s
);

/**
 * @brief Trigger memory consolidation (called during sleep)
 */
int dragonfly_sleep_consolidate(dragonfly_sleep_bridge_t bridge);

/**
 * @brief Get consolidated hunting memory
 */
int dragonfly_sleep_get_memory(
    const dragonfly_sleep_bridge_t bridge,
    consolidated_memory_t* memory
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Check if hunting is currently appropriate
 */
bool dragonfly_sleep_hunting_allowed(const dragonfly_sleep_bridge_t bridge);

/**
 * @brief Get current activity level
 */
float dragonfly_sleep_get_activity(const dragonfly_sleep_bridge_t bridge);

/**
 * @brief Get recommended strategy based on consolidated memory
 */
int dragonfly_sleep_recommend_strategy(
    const dragonfly_sleep_bridge_t bridge,
    uint32_t prey_type,
    float time_of_day,
    intercept_strategy_t* strategy,
    float* confidence
);

/**
 * @brief Get bridge state
 */
int dragonfly_sleep_get_state(
    const dragonfly_sleep_bridge_t bridge,
    dragonfly_sleep_state_t* state
);

/**
 * @brief Get bridge statistics
 */
int dragonfly_sleep_get_stats(
    const dragonfly_sleep_bridge_t bridge,
    dragonfly_sleep_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_SLEEP_BRIDGE_H */
