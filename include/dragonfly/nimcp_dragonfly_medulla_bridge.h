//=============================================================================
// nimcp_dragonfly_medulla_bridge.h - Dragonfly-Medulla Integration Bridge
//=============================================================================
/**
 * @file nimcp_dragonfly_medulla_bridge.h
 * @brief Bridge connecting dragonfly target tracking to medulla oblongata states
 *
 * BIOLOGICAL BASIS:
 * In real dragonflies, hunting behavior is modulated by arousal state:
 * - Drowsy dragonflies don't hunt (low arousal suppresses prey capture)
 * - Alert dragonflies have faster reaction times and higher success rates
 * - Circadian rhythms affect hunting activity (diurnal hunters)
 * - Stress/threat responses can abort or suppress hunting behavior
 *
 * INTEGRATION ARCHITECTURE:
 *
 *   MEDULLA OBLONGATA                    DRAGONFLY
 *   ==================                   ==========
 *
 *   Arousal Level ───────────────────▶  Pursuit Intensity
 *   (COMA...HYPERAROUSAL)               nav_gain, urgency_scale
 *
 *   Protection Level ────────────────▶  Hunting Permission
 *   (NORMAL...SHUTDOWN)                 enable/disable, abort
 *
 *   Circadian Phase ─────────────────▶  Performance Modulation
 *   (EARLY_MORNING...PRE_DAWN)          efficiency, reaction_time
 *
 *   Medulla State ───────────────────▶  System Enable
 *   (STOPPED...STOPPING)                operational state
 *
 *   ◀───────────────────────────────────  Hunt Feedback
 *   arousal_boost, stress_signal        pursuit_active, intercept_result
 *
 * MODULATION EFFECTS:
 *
 * | Arousal Level    | Nav Gain | Urgency | Reaction | Accuracy |
 * |------------------|----------|---------|----------|----------|
 * | COMA             | 0.0      | 0.0     | 0.0      | 0.0      |
 * | DEEP_SLEEP       | 0.0      | 0.0     | 0.0      | 0.0      |
 * | LIGHT_SLEEP      | 0.2      | 0.1     | 0.3      | 0.5      |
 * | DROWSY           | 0.5      | 0.3     | 0.6      | 0.7      |
 * | AWAKE            | 1.0      | 1.0     | 1.0      | 1.0      |
 * | ALERT            | 1.2      | 1.2     | 1.3      | 1.1      |
 * | HYPERAROUSAL     | 1.5      | 1.5     | 1.5      | 0.8      |
 *
 * | Protection Level | Hunting Allowed | Max Duration | Abort Threshold |
 * |------------------|-----------------|--------------|-----------------|
 * | NORMAL           | Yes             | Unlimited    | Normal          |
 * | CAUTIOUS         | Yes             | 90%          | Lower           |
 * | GUARDED          | Yes             | 70%          | Much Lower      |
 * | DEFENSIVE        | Limited         | 50%          | Very Low        |
 * | CRITICAL         | No (abort)      | 0%           | Immediate       |
 * | SHUTDOWN         | No (abort)      | 0%           | Immediate       |
 *
 * | Circadian Phase  | Performance | Notes                    |
 * |------------------|-------------|--------------------------|
 * | EARLY_MORNING    | 0.7         | Warming up               |
 * | MORNING          | 1.0         | Peak hunting time        |
 * | AFTERNOON        | 0.85        | Post-peak dip            |
 * | EVENING          | 0.95        | Second peak              |
 * | LATE_EVENING     | 0.7         | Declining                |
 * | NIGHT            | 0.3         | Mostly inactive          |
 * | DEEP_NIGHT       | 0.1         | Minimal activity         |
 * | PRE_DAWN         | 0.4         | Starting to wake         |
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-12-29
 */

#ifndef NIMCP_DRAGONFLY_MEDULLA_BRIDGE_H
#define NIMCP_DRAGONFLY_MEDULLA_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct dragonfly_system_s dragonfly_system_t;
typedef struct medulla_struct* medulla_t;

//=============================================================================
// Bridge Configuration
//=============================================================================

/**
 * @brief Dragonfly-Medulla bridge configuration
 */
typedef struct {
    /* Arousal modulation settings */
    float arousal_nav_gain_min;         /**< Nav gain at min arousal (default: 0.0) */
    float arousal_nav_gain_max;         /**< Nav gain at max arousal (default: 1.5) */
    float arousal_urgency_scale_min;    /**< Urgency scale at min arousal (default: 0.0) */
    float arousal_urgency_scale_max;    /**< Urgency scale at max arousal (default: 1.5) */
    float arousal_reaction_min;         /**< Reaction multiplier at min (default: 0.0) */
    float arousal_reaction_max;         /**< Reaction multiplier at max (default: 1.5) */

    /* Hyperarousal accuracy penalty */
    float hyperarousal_accuracy_penalty; /**< Accuracy reduction at hyperarousal (default: 0.2) */

    /* Protection level thresholds */
    bool abort_on_critical;             /**< Abort pursuit on CRITICAL (default: true) */
    bool abort_on_shutdown;             /**< Abort pursuit on SHUTDOWN (default: true) */
    float defensive_duration_scale;     /**< Duration scale at DEFENSIVE (default: 0.5) */
    float guarded_duration_scale;       /**< Duration scale at GUARDED (default: 0.7) */
    float cautious_duration_scale;      /**< Duration scale at CAUTIOUS (default: 0.9) */

    /* Circadian modulation */
    bool enable_circadian_modulation;   /**< Enable circadian effects (default: true) */
    float night_performance_floor;      /**< Minimum performance at night (default: 0.1) */

    /* Feedback to medulla */
    bool enable_arousal_feedback;       /**< Pursuit affects arousal (default: true) */
    float pursuit_arousal_boost;        /**< Arousal boost during pursuit (default: 0.1) */
    float intercept_arousal_boost;      /**< Arousal boost on success (default: 0.2) */
    float failure_arousal_penalty;      /**< Arousal decrease on failure (default: 0.05) */

    /* Update settings */
    uint32_t update_interval_ms;        /**< Bridge update interval (default: 50ms) */
    bool enable_logging;                /**< Enable debug logging (default: false) */
} dragonfly_medulla_config_t;

/**
 * @brief Dragonfly-Medulla bridge statistics
 */
typedef struct {
    /* Connection state */
    bool is_connected;                  /**< Bridge is active */
    uint64_t connection_time_us;        /**< Time when connected */

    /* Modulation state */
    float current_arousal_modifier;     /**< Current arousal-based modifier */
    float current_circadian_modifier;   /**< Current circadian-based modifier */
    float current_protection_modifier;  /**< Current protection-based modifier */
    float effective_nav_gain_scale;     /**< Combined nav gain scale */
    float effective_urgency_scale;      /**< Combined urgency scale */

    /* Protection events */
    uint32_t pursuits_aborted_protection; /**< Pursuits aborted due to protection */
    uint32_t pursuits_blocked;          /**< Pursuits blocked from starting */

    /* Feedback statistics */
    uint32_t arousal_boosts_sent;       /**< Arousal boost signals sent */
    float total_arousal_contribution;   /**< Net arousal contribution */

    /* Update statistics */
    uint64_t total_updates;             /**< Total bridge updates */
    uint64_t last_update_us;            /**< Timestamp of last update */
    float avg_update_time_us;           /**< Average update duration */
} dragonfly_medulla_stats_t;

/**
 * @brief Current modulation state from bridge
 */
typedef struct dragonfly_medulla_modulation_s {
    /* Combined modifiers */
    float nav_gain_scale;               /**< Scale factor for nav gain */
    float urgency_scale;                /**< Scale factor for urgency */
    float reaction_scale;               /**< Scale factor for reaction time */
    float accuracy_scale;               /**< Scale factor for accuracy */
    float max_duration_scale;           /**< Scale factor for max pursuit duration */

    /* Hunting permission */
    bool hunting_allowed;               /**< Whether hunting is currently allowed */
    bool should_abort;                  /**< Whether to abort current pursuit */

    /* Source states */
    int arousal_level;                  /**< Current arousal level (enum value) */
    int protection_level;               /**< Current protection level (enum value) */
    int circadian_phase;                /**< Current circadian phase (enum value) */
} dragonfly_medulla_modulation_t;

//=============================================================================
// Bridge Handle
//=============================================================================

/**
 * @brief Opaque bridge handle
 */
typedef struct dragonfly_medulla_bridge_s* dragonfly_medulla_bridge_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @return Default configuration with biologically-plausible values
 */
dragonfly_medulla_config_t dragonfly_medulla_default_config(void);

/**
 * @brief Create dragonfly-medulla bridge
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
dragonfly_medulla_bridge_t dragonfly_medulla_bridge_create(
    const dragonfly_medulla_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge handle
 */
void dragonfly_medulla_bridge_destroy(dragonfly_medulla_bridge_t bridge);

//=============================================================================
// Connection Functions
//=============================================================================

/**
 * @brief Connect bridge to dragonfly and medulla systems
 *
 * @param bridge Bridge handle
 * @param dragonfly Dragonfly system handle
 * @param medulla Medulla handle
 * @return 0 on success, -1 on error
 */
int dragonfly_medulla_bridge_connect(
    dragonfly_medulla_bridge_t bridge,
    dragonfly_system_t* dragonfly,
    medulla_t medulla
);

/**
 * @brief Disconnect bridge from systems
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int dragonfly_medulla_bridge_disconnect(dragonfly_medulla_bridge_t bridge);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Bridge handle
 * @return true if connected to both systems
 */
bool dragonfly_medulla_bridge_is_connected(const dragonfly_medulla_bridge_t bridge);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Update bridge state and apply modulations
 *
 * This should be called regularly (e.g., every frame) to:
 * 1. Read current medulla states (arousal, protection, circadian)
 * 2. Compute modulation factors
 * 3. Apply modulations to dragonfly system
 * 4. Send feedback to medulla based on dragonfly activity
 *
 * @param bridge Bridge handle
 * @param dt Time step in seconds
 * @return 0 on success, -1 on error
 */
int dragonfly_medulla_bridge_update(dragonfly_medulla_bridge_t bridge, float dt);

/**
 * @brief Get current modulation state
 *
 * @param bridge Bridge handle
 * @param modulation Output modulation state
 * @return 0 on success, -1 on error
 */
int dragonfly_medulla_bridge_get_modulation(
    const dragonfly_medulla_bridge_t bridge,
    dragonfly_medulla_modulation_t* modulation
);

//=============================================================================
// Feedback Functions
//=============================================================================

/**
 * @brief Notify bridge of pursuit start
 *
 * Called when dragonfly begins tracking a target.
 * May trigger arousal boost in medulla.
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int dragonfly_medulla_bridge_notify_pursuit_start(dragonfly_medulla_bridge_t bridge);

/**
 * @brief Notify bridge of successful intercept
 *
 * Called when dragonfly successfully intercepts target.
 * Triggers arousal boost and positive feedback.
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int dragonfly_medulla_bridge_notify_intercept_success(dragonfly_medulla_bridge_t bridge);

/**
 * @brief Notify bridge of pursuit failure
 *
 * Called when target escapes or pursuit is aborted.
 * May trigger slight arousal decrease.
 *
 * @param bridge Bridge handle
 * @param reason Failure reason (escaped, aborted, timeout)
 * @return 0 on success, -1 on error
 */
int dragonfly_medulla_bridge_notify_pursuit_failure(
    dragonfly_medulla_bridge_t bridge,
    const char* reason
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Check if hunting is currently allowed
 *
 * Quick check based on current medulla states.
 * Returns false if protection level is too high or arousal too low.
 *
 * @param bridge Bridge handle
 * @return true if hunting is allowed
 */
bool dragonfly_medulla_bridge_hunting_allowed(const dragonfly_medulla_bridge_t bridge);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int dragonfly_medulla_bridge_get_stats(
    const dragonfly_medulla_bridge_t bridge,
    dragonfly_medulla_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int dragonfly_medulla_bridge_reset_stats(dragonfly_medulla_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_DRAGONFLY_MEDULLA_BRIDGE_H
