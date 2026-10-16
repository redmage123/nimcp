/**
 * @file nimcp_dragonfly_substrate_bridge.h
 * @brief Dragonfly-to-Neural Substrate Bridge
 *
 * WHAT: Bridges dragonfly target tracking to neural substrate layer
 * WHY:  Model metabolic costs and substrate-level constraints on interception
 * HOW:  Track energy consumption, modulate performance based on substrate health
 *
 * BIOLOGICAL BASIS:
 * - Dragonfly hunting is metabolically expensive (high flight speed, neural processing)
 * - Prey interception success depends on energy reserves
 * - Repeated pursuits deplete ATP, oxygen, glucose
 * - Substrate fatigue affects tracking accuracy and reaction time
 * - This bridge models these substrate-level constraints
 *
 * @author NIMCP Development Team
 * @date 2025-12-28
 */

#ifndef NIMCP_DRAGONFLY_SUBSTRATE_BRIDGE_H
#define NIMCP_DRAGONFLY_SUBSTRATE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct dragonfly_system_s;
typedef struct dragonfly_system_s dragonfly_system_t;

//=============================================================================
// Constants
//=============================================================================

#define SUBSTRATE_COST_TSDN_UPDATE 0.0002f      /**< ATP cost per TSDN update */
#define SUBSTRATE_COST_TRACKING 0.0003f         /**< ATP cost per tracking step */
#define SUBSTRATE_COST_PREDICTION 0.0004f       /**< ATP cost per prediction */
#define SUBSTRATE_COST_INTERCEPT_CALC 0.0005f   /**< ATP cost per intercept calculation */
#define SUBSTRATE_COST_MODE_SWITCH 0.001f       /**< ATP cost per mode switch */
#define SUBSTRATE_COST_PURSUIT_FLIGHT 0.002f    /**< ATP cost for pursuit (per step) */

//=============================================================================
// Enums
//=============================================================================

/**
 * @brief Metabolic activity level
 */
typedef enum {
    SUBSTRATE_ACTIVITY_IDLE = 0,    /**< Minimal activity (resting) */
    SUBSTRATE_ACTIVITY_SCANNING,    /**< Scanning for targets */
    SUBSTRATE_ACTIVITY_TRACKING,    /**< Active target tracking */
    SUBSTRATE_ACTIVITY_PURSUIT,     /**< High-energy pursuit */
    SUBSTRATE_ACTIVITY_INTERCEPT    /**< Maximum effort interception */
} substrate_activity_level_t;

/**
 * @brief Performance impact from substrate state
 */
typedef enum {
    PERF_IMPACT_NONE = 0,       /**< Normal performance */
    PERF_IMPACT_MILD,           /**< Slight degradation */
    PERF_IMPACT_MODERATE,       /**< Noticeable degradation */
    PERF_IMPACT_SEVERE,         /**< Significant degradation */
    PERF_IMPACT_CRITICAL        /**< Near-failure */
} substrate_perf_impact_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Energy costs per dragonfly operation
 */
typedef struct {
    float tsdn_update;          /**< Per TSDN population update */
    float tracking_step;        /**< Per tracking iteration */
    float prediction_step;      /**< Per prediction calculation */
    float intercept_calc;       /**< Per interception calculation */
    float mode_switch;          /**< Per mode transition */
    float pursuit_flight;       /**< Per pursuit flight step */
    float idle_baseline;        /**< Baseline metabolic cost */
} dragonfly_energy_costs_t;

/**
 * @brief Modulation factors from substrate state
 */
typedef struct {
    float tracking_accuracy;     /**< Tracking accuracy multiplier [0-1] */
    float reaction_time_factor;  /**< Reaction time multiplier [0.5-2.0] */
    float pursuit_speed;         /**< Pursuit speed multiplier [0-1] */
    float prediction_accuracy;   /**< Prediction accuracy multiplier [0-1] */
    float decision_quality;      /**< Decision quality multiplier [0-1] */
    float overall_performance;   /**< Combined performance [0-1] */
} dragonfly_substrate_modulation_t;

/**
 * @brief Substrate bridge configuration
 */
typedef struct {
    dragonfly_energy_costs_t costs;     /**< Energy cost parameters */

    /* Fatigue thresholds */
    float mild_fatigue_threshold;       /**< ATP level for mild fatigue */
    float moderate_fatigue_threshold;   /**< ATP level for moderate fatigue */
    float severe_fatigue_threshold;     /**< ATP level for severe fatigue */

    /* Recovery parameters */
    float rest_recovery_rate;           /**< Recovery during idle */
    float active_recovery_rate;         /**< Recovery during activity */

    /* Performance impact scales */
    float fatigue_accuracy_impact;      /**< How much fatigue affects accuracy */
    float fatigue_speed_impact;         /**< How much fatigue affects speed */

    /* Feature enables */
    bool enable_fatigue_modeling;       /**< Enable fatigue effects */
    bool enable_recovery;               /**< Enable energy recovery */
    bool enable_substrate_feedback;     /**< Feedback substrate state to dragonfly */
} dragonfly_substrate_config_t;

/**
 * @brief Substrate bridge statistics
 */
typedef struct {
    float total_energy_consumed;
    float peak_consumption_rate;
    uint64_t tsdn_updates;
    uint64_t tracking_steps;
    uint64_t prediction_steps;
    uint64_t intercept_calcs;
    uint64_t mode_switches;
    uint64_t pursuit_steps;
    float avg_performance_level;
    float min_performance_level;
    uint32_t fatigue_events;
    float time_in_fatigue_ms;
} substrate_bridge_stats_t;

/**
 * @brief Substrate bridge handle
 */
typedef struct dragonfly_substrate_bridge_s dragonfly_substrate_bridge_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Initialize default substrate bridge configuration
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int dragonfly_substrate_bridge_default_config(dragonfly_substrate_config_t* config);

/**
 * @brief Validate substrate bridge configuration
 * @param config Configuration to validate
 * @return 0 if valid, -1 if invalid
 */
int dragonfly_substrate_bridge_validate_config(const dragonfly_substrate_config_t* config);

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Create substrate bridge
 * @param dragonfly Dragonfly system to connect (may be NULL)
 * @param substrate Neural substrate to connect (may be NULL)
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
dragonfly_substrate_bridge_t* dragonfly_substrate_bridge_create(
    dragonfly_system_t* dragonfly,
    void* substrate,
    const dragonfly_substrate_config_t* config
);

/**
 * @brief Destroy substrate bridge
 * @param bridge Bridge to destroy
 */
void dragonfly_substrate_bridge_destroy(dragonfly_substrate_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int dragonfly_substrate_bridge_reset(dragonfly_substrate_bridge_t* bridge);

//=============================================================================
// Energy Consumption
//=============================================================================

/**
 * @brief Record TSDN update (consumes energy)
 * @param bridge Substrate bridge
 * @param population_size Number of TSDN neurons updated
 * @return 0 on success, -1 on error
 */
int dragonfly_substrate_record_tsdn_update(
    dragonfly_substrate_bridge_t* bridge,
    uint32_t population_size
);

/**
 * @brief Record tracking step (consumes energy)
 * @param bridge Substrate bridge
 * @param num_targets Number of targets being tracked
 * @return 0 on success, -1 on error
 */
int dragonfly_substrate_record_tracking(
    dragonfly_substrate_bridge_t* bridge,
    uint32_t num_targets
);

/**
 * @brief Record prediction calculation (consumes energy)
 * @param bridge Substrate bridge
 * @param complexity Prediction complexity factor [0-1]
 * @return 0 on success, -1 on error
 */
int dragonfly_substrate_record_prediction(
    dragonfly_substrate_bridge_t* bridge,
    float complexity
);

/**
 * @brief Record interception calculation (consumes energy)
 * @param bridge Substrate bridge
 * @param nav_complexity Navigation complexity factor [0-1]
 * @return 0 on success, -1 on error
 */
int dragonfly_substrate_record_intercept_calc(
    dragonfly_substrate_bridge_t* bridge,
    float nav_complexity
);

/**
 * @brief Record mode switch (consumes energy)
 * @param bridge Substrate bridge
 * @return 0 on success, -1 on error
 */
int dragonfly_substrate_record_mode_switch(dragonfly_substrate_bridge_t* bridge);

/**
 * @brief Record pursuit flight step (consumes energy)
 * @param bridge Substrate bridge
 * @param intensity Pursuit intensity [0-1]
 * @return 0 on success, -1 on error
 */
int dragonfly_substrate_record_pursuit(
    dragonfly_substrate_bridge_t* bridge,
    float intensity
);

//=============================================================================
// Performance Modulation
//=============================================================================

/**
 * @brief Get current performance modulation factors
 * @param bridge Substrate bridge
 * @param mod Output modulation factors
 * @return 0 on success, -1 on error
 */
int dragonfly_substrate_get_modulation(
    const dragonfly_substrate_bridge_t* bridge,
    dragonfly_substrate_modulation_t* mod
);

/**
 * @brief Get tracking accuracy factor
 * @param bridge Substrate bridge
 * @return Tracking accuracy multiplier [0-1]
 */
float dragonfly_substrate_get_tracking_accuracy(
    const dragonfly_substrate_bridge_t* bridge
);

/**
 * @brief Get pursuit speed factor
 * @param bridge Substrate bridge
 * @return Pursuit speed multiplier [0-1]
 */
float dragonfly_substrate_get_pursuit_speed(
    const dragonfly_substrate_bridge_t* bridge
);

/**
 * @brief Get reaction time factor
 * @param bridge Substrate bridge
 * @return Reaction time multiplier (1.0 = normal, >1 = slower)
 */
float dragonfly_substrate_get_reaction_factor(
    const dragonfly_substrate_bridge_t* bridge
);

/**
 * @brief Get current performance impact level
 * @param bridge Substrate bridge
 * @return Current performance impact
 */
substrate_perf_impact_t dragonfly_substrate_get_impact(
    const dragonfly_substrate_bridge_t* bridge
);

/**
 * @brief Check if substrate is fatigued
 * @param bridge Substrate bridge
 * @return true if in fatigue state
 */
bool dragonfly_substrate_is_fatigued(const dragonfly_substrate_bridge_t* bridge);

//=============================================================================
// Activity Tracking
//=============================================================================

/**
 * @brief Set current activity level
 * @param bridge Substrate bridge
 * @param level Activity level
 * @return 0 on success, -1 on error
 */
int dragonfly_substrate_set_activity(
    dragonfly_substrate_bridge_t* bridge,
    substrate_activity_level_t level
);

/**
 * @brief Get current activity level
 * @param bridge Substrate bridge
 * @return Current activity level
 */
substrate_activity_level_t dragonfly_substrate_get_activity(
    const dragonfly_substrate_bridge_t* bridge
);

/**
 * @brief Get current energy level (ATP)
 * @param bridge Substrate bridge
 * @return Current ATP level [0-1]
 */
float dragonfly_substrate_get_energy(const dragonfly_substrate_bridge_t* bridge);

//=============================================================================
// Integration
//=============================================================================

/**
 * @brief Connect to dragonfly system
 * @param bridge Substrate bridge
 * @param dragonfly Dragonfly system
 * @return 0 on success, -1 on error
 */
int dragonfly_substrate_connect_dragonfly(
    dragonfly_substrate_bridge_t* bridge,
    dragonfly_system_t* dragonfly
);

/**
 * @brief Connect to neural substrate
 * @param bridge Substrate bridge
 * @param substrate Neural substrate
 * @return 0 on success, -1 on error
 */
int dragonfly_substrate_connect_substrate(
    dragonfly_substrate_bridge_t* bridge,
    void* substrate
);

/**
 * @brief Check if dragonfly is connected
 * @param bridge Substrate bridge
 * @return true if connected
 */
bool dragonfly_substrate_has_dragonfly(const dragonfly_substrate_bridge_t* bridge);

/**
 * @brief Check if substrate is connected
 * @param bridge Substrate bridge
 * @return true if connected
 */
bool dragonfly_substrate_has_substrate(const dragonfly_substrate_bridge_t* bridge);

//=============================================================================
// Update
//=============================================================================

/**
 * @brief Update bridge state
 * @param bridge Substrate bridge
 * @return 0 on success, -1 on error
 */
int dragonfly_substrate_update(dragonfly_substrate_bridge_t* bridge);

/**
 * @brief Step bridge simulation
 * @param bridge Substrate bridge
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
int dragonfly_substrate_step(
    dragonfly_substrate_bridge_t* bridge,
    float dt_ms
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get bridge statistics
 * @param bridge Substrate bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int dragonfly_substrate_bridge_get_stats(
    const dragonfly_substrate_bridge_t* bridge,
    substrate_bridge_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 * @param bridge Substrate bridge
 * @return 0 on success, -1 on error
 */
int dragonfly_substrate_bridge_reset_stats(dragonfly_substrate_bridge_t* bridge);

//=============================================================================
// Utility
//=============================================================================

/**
 * @brief Get activity level name
 * @param level Activity level
 * @return Activity name string
 */
const char* dragonfly_substrate_activity_name(substrate_activity_level_t level);

/**
 * @brief Get performance impact name
 * @param impact Performance impact
 * @return Impact name string
 */
const char* dragonfly_substrate_impact_name(substrate_perf_impact_t impact);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_SUBSTRATE_BRIDGE_H */
