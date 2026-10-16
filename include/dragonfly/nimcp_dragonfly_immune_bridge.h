/**
 * @file nimcp_dragonfly_immune_bridge.h
 * @brief Immune-Dragonfly Integration Bridge
 *
 * BIOLOGICAL REFERENCE:
 * The immune system modulates behavior including hunting:
 * - Sickness behavior reduces activity to conserve energy for immune response
 * - Stress responses can trigger immune changes
 * - Repeated failure/frustration triggers stress-immune interactions
 *
 * WHAT: Integrates immune system state with hunting behavior
 * WHY:  Enables realistic energy conservation and stress responses
 * HOW:  Bidirectional communication with BBB and immune system
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#ifndef NIMCP_DRAGONFLY_IMMUNE_BRIDGE_H
#define NIMCP_DRAGONFLY_IMMUNE_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "dragonfly/nimcp_dragonfly.h"
#include "security/nimcp_blood_brain_barrier.h"

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct dragonfly_immune_bridge_s* dragonfly_immune_bridge_t;
/* Note: bbb_system_t is already defined in nimcp_blood_brain_barrier.h */

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Health status affecting hunting
 */
typedef enum {
    HEALTH_OPTIMAL,           /**< Full hunting capability */
    HEALTH_MILD_IMPAIRMENT,   /**< Slightly reduced performance */
    HEALTH_MODERATE_IMPAIRMENT, /**< Significantly reduced */
    HEALTH_SEVERE_IMPAIRMENT, /**< Hunting not recommended */
    HEALTH_CRITICAL           /**< Must rest/recover */
} health_status_t;

/**
 * @brief Stress level
 */
typedef enum {
    STRESS_NONE,              /**< No stress */
    STRESS_LOW,               /**< Mild stress (energizing) */
    STRESS_MODERATE,          /**< Moderate stress */
    STRESS_HIGH,              /**< High stress (impairing) */
    STRESS_CHRONIC            /**< Chronic stress (damaging) */
} stress_level_t;

/**
 * @brief Immune modulation of hunting
 */
typedef struct {
    /* Performance modifiers */
    float speed_modifier;         /**< Speed capability [0,1] */
    float accuracy_modifier;      /**< Accuracy capability [0,1] */
    float endurance_modifier;     /**< Endurance capability [0,1] */
    float reaction_modifier;      /**< Reaction time modifier [0,1] */

    /* Behavioral modifiers */
    bool hunting_recommended;     /**< Should hunting be attempted? */
    float max_pursuit_duration_s; /**< Maximum pursuit time */
    float energy_conservation;    /**< Energy conservation factor [0,1] */

    /* Recovery */
    float recovery_rate;          /**< Rate of recovery [0,1] */
    float rest_urgency;           /**< Urgency to rest [0,1] */
} immune_modulation_t;

/**
 * @brief Hunting stress report
 */
typedef struct {
    /* Hunt statistics */
    uint32_t hunts_attempted;     /**< Total hunts attempted */
    uint32_t hunts_successful;    /**< Successful hunts */
    uint32_t consecutive_failures;/**< Consecutive failures */

    /* Stress indicators */
    float frustration_level;      /**< Frustration from failures [0,1] */
    float fatigue_level;          /**< Physical fatigue [0,1] */
    float injury_risk;            /**< Risk of injury [0,1] */

    /* Physiological */
    float cortisol_proxy;         /**< Simulated cortisol level */
    float adrenaline_proxy;       /**< Simulated adrenaline level */
    float energy_reserves;        /**< Remaining energy [0,1] */
} hunting_stress_t;

/**
 * @brief Immune bridge configuration
 */
typedef struct {
    /* Health thresholds */
    float mild_impairment_threshold;    /**< Health for mild impairment */
    float moderate_impairment_threshold;/**< Health for moderate impairment */
    float severe_impairment_threshold;  /**< Health for severe impairment */
    float critical_threshold;           /**< Health for critical status */

    /* Stress accumulation */
    float failure_stress_increment;     /**< Stress per failure */
    float success_stress_decrement;     /**< Stress relief per success */
    float stress_decay_rate;            /**< Natural stress decay */
    uint32_t failure_frustration_threshold; /**< Failures before frustration */

    /* Energy management */
    float energy_per_pursuit_j;         /**< Energy cost per pursuit */
    float energy_recovery_rate;         /**< Energy recovery rate */
    float min_energy_for_hunt;          /**< Minimum energy to hunt */

    /* Injury modeling */
    float injury_probability_base;      /**< Base injury probability */
    float injury_fatigue_factor;        /**< Fatigue increases injury risk */
    float injury_recovery_time_s;       /**< Time to recover from injury */

    /* Feedback to immune */
    bool enable_immune_feedback;        /**< Send status to immune system */
    float immune_stress_weight;         /**< Weight of stress on immune */
} dragonfly_immune_config_t;

/**
 * @brief Immune bridge state
 */
typedef struct {
    health_status_t health_status;      /**< Current health status */
    stress_level_t stress_level;        /**< Current stress level */
    immune_modulation_t modulation;     /**< Current modulation */
    hunting_stress_t stress_report;     /**< Current stress report */

    /* Injury state */
    bool is_injured;                    /**< Currently injured */
    float injury_severity;              /**< Injury severity [0,1] */
    float time_to_recovery_s;           /**< Time until recovered */
} dragonfly_immune_state_t;

/**
 * @brief Immune bridge statistics
 */
typedef struct {
    uint64_t modulations_applied;       /**< Total modulations applied */
    uint64_t hunts_blocked;             /**< Hunts blocked by health */
    uint64_t stress_events;             /**< Stress accumulation events */
    uint64_t recovery_events;           /**< Recovery events */
    float total_energy_expended_j;      /**< Total energy used */
    float avg_health_modifier;          /**< Average health modifier */
    uint32_t injuries_sustained;        /**< Total injuries */
} dragonfly_immune_stats_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default immune bridge configuration
 */
dragonfly_immune_config_t dragonfly_immune_default_config(void);

/**
 * @brief Validate immune bridge configuration
 */
bool dragonfly_immune_validate_config(const dragonfly_immune_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create immune bridge
 */
dragonfly_immune_bridge_t dragonfly_immune_bridge_create(
    const dragonfly_immune_config_t* config
);

/**
 * @brief Destroy immune bridge
 */
void dragonfly_immune_bridge_destroy(dragonfly_immune_bridge_t bridge);

/**
 * @brief Connect to systems
 */
int dragonfly_immune_bridge_connect(
    dragonfly_immune_bridge_t bridge,
    dragonfly_system_t* dragonfly,
    bbb_system_t bbb
);

/**
 * @brief Disconnect from systems
 */
int dragonfly_immune_bridge_disconnect(dragonfly_immune_bridge_t bridge);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Update immune bridge
 */
int dragonfly_immune_bridge_update(
    dragonfly_immune_bridge_t bridge,
    float dt_s
);

/**
 * @brief Report hunt attempt
 */
int dragonfly_immune_report_hunt(
    dragonfly_immune_bridge_t bridge,
    bool success,
    float duration_s,
    float energy_used
);

/**
 * @brief Report pursuit stress
 */
int dragonfly_immune_report_stress(
    dragonfly_immune_bridge_t bridge,
    float pursuit_intensity,
    float duration_s
);

/**
 * @brief Report rest/recovery
 */
int dragonfly_immune_report_rest(
    dragonfly_immune_bridge_t bridge,
    float duration_s
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get current modulation
 */
int dragonfly_immune_get_modulation(
    const dragonfly_immune_bridge_t bridge,
    immune_modulation_t* modulation
);

/**
 * @brief Check if hunting is safe
 */
bool dragonfly_immune_hunting_safe(const dragonfly_immune_bridge_t bridge);

/**
 * @brief Get health status
 */
health_status_t dragonfly_immune_get_health(const dragonfly_immune_bridge_t bridge);

/**
 * @brief Get stress level
 */
stress_level_t dragonfly_immune_get_stress(const dragonfly_immune_bridge_t bridge);

/**
 * @brief Get immune bridge state
 */
int dragonfly_immune_get_state(
    const dragonfly_immune_bridge_t bridge,
    dragonfly_immune_state_t* state
);

/**
 * @brief Get immune bridge statistics
 */
int dragonfly_immune_get_stats(
    const dragonfly_immune_bridge_t bridge,
    dragonfly_immune_stats_t* stats
);

/**
 * @brief Get health status name
 */
const char* dragonfly_health_status_name(health_status_t status);

/**
 * @brief Get stress level name
 */
const char* dragonfly_stress_level_name(stress_level_t level);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_IMMUNE_BRIDGE_H */
