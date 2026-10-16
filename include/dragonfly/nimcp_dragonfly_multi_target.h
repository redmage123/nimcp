/**
 * @file nimcp_dragonfly_multi_target.h
 * @brief Multi-Target Priority Queue and Rapid Switching
 *
 * BIOLOGICAL REFERENCE:
 * While dragonflies exhibit strict winner-take-all attention (CSTMD1),
 * they can rapidly switch targets when the current one escapes. This
 * module maintains a priority queue of potential targets for fast switching.
 *
 * WHAT: Manages priority queue of target candidates
 * WHY:  Enables rapid target switching on miss or escape
 * HOW:  Continuous parallel evaluation of alternative targets
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#ifndef NIMCP_DRAGONFLY_MULTI_TARGET_H
#define NIMCP_DRAGONFLY_MULTI_TARGET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "dragonfly/nimcp_dragonfly.h"

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct dragonfly_multi_target_s* dragonfly_multi_target_t;

//=============================================================================
// Constants
//=============================================================================

#define MULTI_TARGET_MAX_QUEUE 8      /**< Maximum targets in queue */
#define MULTI_TARGET_HISTORY_SIZE 16  /**< Recent target history */

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Target priority factors
 */
typedef enum {
    PRIORITY_DISTANCE,        /**< Closer is higher priority */
    PRIORITY_SIZE,            /**< Larger is higher priority */
    PRIORITY_VELOCITY,        /**< Slower is higher priority */
    PRIORITY_FEASIBILITY,     /**< More feasible is higher */
    PRIORITY_SALIENCE,        /**< More salient is higher */
    PRIORITY_ISOLATION,       /**< More isolated is higher */
    PRIORITY_COUNT
} priority_factor_t;

/**
 * @brief Target status in queue
 */
typedef enum {
    TARGET_STATUS_CANDIDATE,  /**< Potential target */
    TARGET_STATUS_PRIMARY,    /**< Currently tracked */
    TARGET_STATUS_BACKUP,     /**< Ready backup */
    TARGET_STATUS_EVALUATING, /**< Being evaluated */
    TARGET_STATUS_REJECTED,   /**< Rejected (too difficult) */
    TARGET_STATUS_LOST        /**< Lost from view */
} target_status_t;

/**
 * @brief Switch reason
 */
typedef enum {
    SWITCH_TARGET_ESCAPED,    /**< Primary target escaped */
    SWITCH_TARGET_LOST,       /**< Primary target lost */
    SWITCH_BETTER_AVAILABLE,  /**< Better target appeared */
    SWITCH_OBSTRUCTION,       /**< Path obstructed */
    SWITCH_ENERGY_SAVE,       /**< Energy conservation */
    SWITCH_MANUAL             /**< Manual switch command */
} switch_reason_t;

/**
 * @brief Queued target information
 */
typedef struct {
    /* Identification */
    uint32_t id;                      /**< Target ID */
    target_status_t status;           /**< Current status */

    /* State estimate */
    float position[3];                /**< Estimated position */
    float velocity[3];                /**< Estimated velocity */
    float size;                       /**< Angular size */
    float confidence;                 /**< Tracking confidence [0,1] */

    /* Priority scoring */
    float priority_score;             /**< Overall priority [0,1] */
    float factor_scores[PRIORITY_COUNT]; /**< Per-factor scores */

    /* Feasibility */
    float intercept_time_s;           /**< Estimated intercept time */
    float success_probability;        /**< Estimated success prob [0,1] */
    float energy_cost;                /**< Estimated energy cost */

    /* History */
    uint64_t first_seen_us;           /**< First detection time */
    uint64_t last_seen_us;            /**< Last detection time */
    uint32_t observations;            /**< Number of observations */
} queued_target_t;

/**
 * @brief Target switch event
 */
typedef struct {
    uint32_t from_target_id;          /**< Previous target ID */
    uint32_t to_target_id;            /**< New target ID */
    switch_reason_t reason;           /**< Reason for switch */
    float switch_time_ms;             /**< Time to complete switch */
    uint64_t timestamp_us;            /**< Switch timestamp */
} switch_event_t;

/**
 * @brief Multi-target configuration
 */
typedef struct {
    /* Queue settings */
    uint32_t max_queue_size;          /**< Maximum queue size */
    float min_confidence_threshold;   /**< Minimum confidence to queue */
    float rejection_threshold;        /**< Success prob to reject */

    /* Priority weights */
    float priority_weights[PRIORITY_COUNT]; /**< Factor weights */

    /* Switching parameters */
    float switch_hysteresis;          /**< Hysteresis for switching */
    float min_lock_time_s;            /**< Minimum lock before switch */
    float better_target_margin;       /**< Margin for "better" target */

    /* Precomputation */
    bool enable_parallel_evaluation;  /**< Evaluate backup targets */
    uint32_t evaluation_budget;       /**< Max evaluations per frame */

    /* History */
    bool track_switch_history;        /**< Track switch events */
    float recent_target_penalty;      /**< Penalty for recently failed */
} multi_target_config_t;

/**
 * @brief Multi-target state
 */
typedef struct {
    /* Queue state */
    uint32_t num_targets;             /**< Targets in queue */
    uint32_t primary_target_id;       /**< Current primary ID */
    bool has_backup;                  /**< Backup available */
    uint32_t backup_target_id;        /**< Best backup ID */

    /* Performance */
    float avg_switch_time_ms;         /**< Average switch time */
    uint32_t switches_this_session;   /**< Switches in current session */
} multi_target_state_t;

/**
 * @brief Multi-target statistics
 */
typedef struct {
    uint64_t targets_queued;          /**< Total targets queued */
    uint64_t targets_rejected;        /**< Targets rejected */
    uint64_t switches_performed;      /**< Total switches */
    uint64_t successful_switches;     /**< Switches leading to success */
    float avg_queue_size;             /**< Average queue size */
    float avg_switch_latency_ms;      /**< Average switch latency */
    float switch_success_rate;        /**< Success after switch */
    uint32_t switch_reasons[6];       /**< Count by switch reason */
} multi_target_stats_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default multi-target configuration
 */
multi_target_config_t multi_target_default_config(void);

/**
 * @brief Validate multi-target configuration
 */
bool multi_target_validate_config(const multi_target_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create multi-target system
 */
dragonfly_multi_target_t dragonfly_multi_target_create(
    const multi_target_config_t* config
);

/**
 * @brief Destroy multi-target system
 */
void dragonfly_multi_target_destroy(dragonfly_multi_target_t mt);

/**
 * @brief Reset multi-target system
 */
int dragonfly_multi_target_reset(dragonfly_multi_target_t mt);

//=============================================================================
// Queue Management Functions
//=============================================================================

/**
 * @brief Add or update target in queue
 */
int dragonfly_multi_target_update(
    dragonfly_multi_target_t mt,
    const dragonfly_detection_t* detection,
    const dragonfly_self_state_t* self_state
);

/**
 * @brief Remove target from queue
 */
int dragonfly_multi_target_remove(
    dragonfly_multi_target_t mt,
    uint32_t target_id
);

/**
 * @brief Set primary target
 */
int dragonfly_multi_target_set_primary(
    dragonfly_multi_target_t mt,
    uint32_t target_id
);

/**
 * @brief Evaluate and sort queue
 */
int dragonfly_multi_target_evaluate(
    dragonfly_multi_target_t mt,
    const dragonfly_self_state_t* self_state
);

//=============================================================================
// Switching Functions
//=============================================================================

/**
 * @brief Check if target switch is recommended
 */
bool dragonfly_multi_target_should_switch(
    const dragonfly_multi_target_t mt,
    switch_reason_t* reason
);

/**
 * @brief Execute target switch
 */
int dragonfly_multi_target_switch(
    dragonfly_multi_target_t mt,
    switch_reason_t reason,
    switch_event_t* event
);

/**
 * @brief Switch to specific target
 */
int dragonfly_multi_target_switch_to(
    dragonfly_multi_target_t mt,
    uint32_t target_id,
    switch_reason_t reason
);

/**
 * @brief Get next best target after current
 */
int dragonfly_multi_target_get_backup(
    const dragonfly_multi_target_t mt,
    queued_target_t* backup
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get primary target info
 */
int dragonfly_multi_target_get_primary(
    const dragonfly_multi_target_t mt,
    queued_target_t* primary
);

/**
 * @brief Get all queued targets
 */
int dragonfly_multi_target_get_queue(
    const dragonfly_multi_target_t mt,
    queued_target_t* targets,
    uint32_t max_targets,
    uint32_t* num_targets
);

/**
 * @brief Get target by ID
 */
int dragonfly_multi_target_get_by_id(
    const dragonfly_multi_target_t mt,
    uint32_t target_id,
    queued_target_t* target
);

/**
 * @brief Get multi-target state
 */
int dragonfly_multi_target_get_state(
    const dragonfly_multi_target_t mt,
    multi_target_state_t* state
);

/**
 * @brief Get multi-target statistics
 */
int dragonfly_multi_target_get_stats(
    const dragonfly_multi_target_t mt,
    multi_target_stats_t* stats
);

/**
 * @brief Get switch history
 */
int dragonfly_multi_target_get_history(
    const dragonfly_multi_target_t mt,
    switch_event_t* history,
    uint32_t max_events,
    uint32_t* num_events
);

/**
 * @brief Get target status name
 */
const char* dragonfly_target_status_name(target_status_t status);

/**
 * @brief Get switch reason name
 */
const char* dragonfly_switch_reason_name(switch_reason_t reason);

/**
 * @brief Get priority factor name
 */
const char* dragonfly_priority_factor_name(priority_factor_t factor);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_MULTI_TARGET_H */
