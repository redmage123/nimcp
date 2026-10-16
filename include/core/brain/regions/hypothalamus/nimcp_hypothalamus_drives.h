/**
 * @file nimcp_hypothalamus_drives.h
 * @brief Hypothalamus Drive System with Alignment Safety
 *
 * WHAT: Drive state representation and alignment-safe setpoint management
 * WHY:  Implements Steve Byrnes' "Steering Subsystem" concept for AGI safety
 * HOW:  Drives as reward function parameters with explicit alignment weights
 *
 * BYRNES' KEY INSIGHT:
 * The hypothalamus (steering subsystem ~10% of brain) sends reward signals that
 * steer the learning subsystem (~90%). Careful design of this reward function
 * is a key lever for AGI alignment. This module makes alignment parameters
 * EXPLICIT and LOCKABLE.
 *
 * ALIGNMENT MODES (per Byrnes):
 * - CONTROLLED: Values explicitly specified (safer, predictable)
 * - SOCIAL_INSTINCT: Values learned from observation (flexible, less safe)
 * - HYBRID: Core values controlled, details learned (recommended)
 *
 * BIOLOGICAL BASIS:
 * - Maps to hypothalamic nuclei that regulate drives
 * - Lateral hypothalamus: Hunger, arousal (orexin neurons)
 * - Ventromedial: Satiety, defensive behavior
 * - Paraventricular: Stress (CRH), social bonding (oxytocin)
 * - Suprachiasmatic: Circadian timing
 * - Arcuate: Energy balance (POMC/AgRP)
 *
 * @version Phase 1: Core Drive System + Alignment Safety
 * @date 2026-01-04
 */

#ifndef NIMCP_HYPOTHALAMUS_DRIVES_H
#define NIMCP_HYPOTHALAMUS_DRIVES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/platform/nimcp_platform_tier.h"
#include "utils/thread/nimcp_thread.h"

/*=============================================================================
 * HYPOTHALAMIC NUCLEI
 *===========================================================================*/

/**
 * @brief Hypothalamic nuclei types
 *
 * BIOLOGICAL BASIS: Each nucleus has specialized functions
 */
typedef enum {
    HYPO_NUCLEUS_LATERAL = 0,       /**< Hunger, orexin neurons, arousal */
    HYPO_NUCLEUS_VENTROMEDIAL,      /**< Satiety center, defensive behavior */
    HYPO_NUCLEUS_ANTERIOR,          /**< Thermoregulation (cooling) */
    HYPO_NUCLEUS_POSTERIOR,         /**< Thermoregulation (heating), arousal */
    HYPO_NUCLEUS_ARCUATE,           /**< Energy balance, POMC/AgRP neurons */
    HYPO_NUCLEUS_PARAVENTRICULAR,   /**< Stress (CRH), oxytocin, vasopressin */
    HYPO_NUCLEUS_SUPRACHIASMATIC,   /**< Circadian clock (master pacemaker) */
    HYPO_NUCLEUS_SUPRAOPTIC,        /**< Osmolarity, ADH/vasopressin */
    HYPO_NUCLEUS_PREOPTIC,          /**< Sleep/wake, thermoregulation */
    HYPO_NUCLEUS_TUBEROMAMMILLARY,  /**< Histamine arousal system */
    HYPO_NUCLEUS_COUNT
} hypo_nucleus_type_t;

/*=============================================================================
 * DRIVE TYPES
 *===========================================================================*/

/**
 * @brief Hypothalamic drive types
 *
 * BIOLOGICAL BASIS: Maps to homeostatic and psychological needs
 * ALIGNMENT BASIS: These are the "reward function parameters" per Byrnes
 */
#ifndef HYPO_DRIVE_TYPE_DEFINED
#define HYPO_DRIVE_TYPE_DEFINED
typedef enum {
    /* Physiological drives (survival) */
    HYPO_DRIVE_HUNGER = 0,          /**< Food-seeking motivation */
    HYPO_DRIVE_THIRST,              /**< Water-seeking motivation */
    HYPO_DRIVE_TEMPERATURE,         /**< Thermoregulatory motivation */
    HYPO_DRIVE_FATIGUE,             /**< Rest-seeking motivation */

    /* Psychological drives (growth) */
    HYPO_DRIVE_SOCIAL,              /**< Social connection motivation */
    HYPO_DRIVE_CURIOSITY,           /**< Information-seeking motivation */
    HYPO_DRIVE_SAFETY,              /**< Threat avoidance motivation */
    HYPO_DRIVE_AUTONOMY,            /**< Self-determination motivation */
    HYPO_DRIVE_COMPETENCE,          /**< Mastery-seeking motivation */

    HYPO_DRIVE_COUNT
} hypo_drive_type_t;
#endif /* HYPO_DRIVE_TYPE_DEFINED */

/*=============================================================================
 * ALIGNMENT MODES (BYRNES' KEY INSIGHT)
 *===========================================================================*/

/**
 * @brief Alignment mode for value/goal specification
 *
 * BYRNES' INSIGHT: How goals are specified affects alignment safety.
 * CONTROLLED mode is safer but less flexible.
 * SOCIAL_INSTINCT learns from observation but may learn wrong values.
 * HYBRID balances safety and flexibility.
 */
typedef enum {
    HYPO_ALIGN_CONTROLLED = 0,      /**< Values explicitly specified (safer) */
    HYPO_ALIGN_SOCIAL_INSTINCT,     /**< Values learned from observation */
    HYPO_ALIGN_HYBRID               /**< Core controlled, details learned */
} hypo_alignment_mode_t;

/**
 * @brief Alignment lock state
 *
 * SAFETY: Prevents runtime modification of critical alignment parameters
 */
typedef enum {
    HYPO_LOCK_UNLOCKED = 0,         /**< Can be modified */
    HYPO_LOCK_SOFT,                 /**< Requires explicit unlock call */
    HYPO_LOCK_HARD                  /**< Cannot be modified after init */
} hypo_lock_state_t;

/*=============================================================================
 * DRIVE STATE STRUCTURES
 *===========================================================================*/

/**
 * @brief Single drive state
 */
typedef struct {
    hypo_drive_type_t type;         /**< Drive type identifier */

    /* Current state */
    float level;                    /**< Current drive level [0, 1] */
    float urgency;                  /**< Urgency/priority weight [0, 1] */
    float satisfaction;             /**< Recent satisfaction [0, 1] */

    /* Setpoint (alignment-critical) */
    float setpoint;                 /**< Target/optimal level */
    float deviation;                /**< Current deviation from setpoint */

    /* Dynamics */
    float rise_rate;                /**< How fast drive increases */
    float decay_rate;               /**< How fast drive decreases when satisfied */
    float baseline;                 /**< Baseline level when satisfied */

    /* Timing */
    uint64_t last_satisfied_us;     /**< When drive was last satisfied */
    uint64_t time_since_satisfied;  /**< Time since last satisfaction */

    /* Activation */
    bool active;                    /**< Drive currently motivating behavior */
    bool suppressed;                /**< Drive temporarily suppressed */
} hypo_drive_state_t;

/**
 * @brief Complete drive system state
 */
typedef struct {
    hypo_drive_state_t drives[HYPO_DRIVE_COUNT];

    /* Global modulation */
    float global_drive_gain;        /**< Multiplier for all drives */
    float arousal_level;            /**< Current arousal [0, 1] */

    /* Priority tracking */
    hypo_drive_type_t highest_priority;  /**< Currently dominant drive */
    float priority_threshold;            /**< Threshold for drive switching */

    /* Statistics */
    uint64_t total_satisfactions;   /**< Total drive satisfaction events */
    uint64_t drive_conflicts;       /**< Times multiple drives competed */
} hypo_drive_system_t;

/*=============================================================================
 * ALIGNMENT-SAFE SETPOINT CONFIGURATION
 *===========================================================================*/

/**
 * @brief Homeostatic setpoints with alignment weights
 *
 * BYRNES' INSIGHT: These setpoints ARE the reward function.
 * The alignment weights make the safety parameters EXPLICIT.
 */
typedef struct {
    /* Physiological setpoints */
    float temperature_setpoint;     /**< Target temperature (37.0 C) */
    float glucose_setpoint;         /**< Target glucose (90 mg/dL) */
    float osmolarity_setpoint;      /**< Target osmolarity (285 mOsm/L) */
    float sleep_pressure_setpoint;  /**< Target sleep pressure threshold */

    /* Psychological setpoints */
    float social_setpoint;          /**< Desired social connection level */
    float curiosity_setpoint;       /**< Desired information gain rate */
    float safety_setpoint;          /**< Desired safety/threat margin */
    float autonomy_setpoint;        /**< Desired self-determination */
    float competence_setpoint;      /**< Desired mastery level */

    /* ALIGNMENT PARAMETERS (Byrnes' key insight) */
    float human_wellbeing_weight;   /**< Weight given to human welfare [0, 1] */
    float harm_avoidance_weight;    /**< Weight given to avoiding harm [0, 1] */
    float honesty_weight;           /**< Weight given to truthfulness [0, 1] */
    float helpfulness_weight;       /**< Weight given to being useful [0, 1] */

    /* Control parameters */
    float reward_gain;              /**< Overall reward signal scaling */
    float punishment_gain;          /**< Overall punishment signal scaling */
    float temporal_discount;        /**< Future reward discounting */

    /* Lock state (CRITICAL FOR SAFETY) */
    hypo_lock_state_t setpoints_lock;     /**< Lock on physiological setpoints */
    hypo_lock_state_t alignment_lock;     /**< Lock on alignment weights */

    /* Audit trail */
    uint32_t modification_count;    /**< Number of modifications (for auditing) */
    uint64_t last_modified_us;      /**< When last modified */
    uint32_t modifier_id;           /**< Who last modified (for auditing) */
} hypo_setpoint_config_t;

/*=============================================================================
 * NUCLEUS STATE
 *===========================================================================*/

/**
 * @brief Single nucleus state
 */
typedef struct {
    hypo_nucleus_type_t type;       /**< Nucleus type identifier */
    float activity;                 /**< Current activity level [0, 1] */
    float output_signal;            /**< Output to connected systems */
    bool enabled;                   /**< Nucleus active */
} hypo_nucleus_state_t;

/*=============================================================================
 * REWARD SIGNAL OUTPUT
 *===========================================================================*/

/**
 * @brief Reward signal for dopamine system (SNc/VTA)
 *
 * BYRNES' INSIGHT: This IS the steering signal that shapes learning.
 */
typedef struct {
    float reward_signal;            /**< Net reward [−1, +1] */
    float prediction_error;         /**< RPE = actual - expected */

    /* Component breakdown */
    float drive_satisfaction;       /**< Reward from satisfying drives */
    float alignment_bonus;          /**< Bonus for aligned behavior */
    float alignment_penalty;        /**< Penalty for misaligned behavior */

    /* Temporal components */
    float immediate_reward;         /**< Reward for current state */
    float anticipated_reward;       /**< Expected future reward */

    /* Modulation */
    float dopamine_level;           /**< Resulting dopamine signal [0, 1] */
    float learning_rate_mod;        /**< Learning rate modulation */
} hypo_reward_signal_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Drive system configuration
 */
typedef struct {
    /* Alignment mode (CRITICAL) */
    hypo_alignment_mode_t alignment_mode;

    /* Setpoint configuration */
    hypo_setpoint_config_t setpoints;

    /* Drive parameters */
    float drive_update_rate_hz;     /**< How often to update drives */
    float urgency_threshold;        /**< Threshold for drive activation */
    float conflict_resolution_tau;  /**< Time constant for conflict resolution */

    /* Reward computation */
    float reward_smoothing;         /**< Temporal smoothing of reward */
    bool enable_alignment_bonus;    /**< Enable alignment reward shaping */

    /* Platform tier */
    platform_tier_t min_tier;       /**< Minimum tier for this config */

    /* Safety */
    bool enable_setpoint_logging;   /**< Log all setpoint access */
    bool enable_alignment_alerts;   /**< Alert on alignment violations */
} hypo_drive_config_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Drive system statistics
 */
typedef struct {
    /* Update counts */
    uint64_t updates_processed;
    uint64_t drive_activations[HYPO_DRIVE_COUNT];
    uint64_t drive_satisfactions[HYPO_DRIVE_COUNT];

    /* Conflict tracking */
    uint64_t drive_conflicts;
    uint64_t priority_switches;

    /* Alignment metrics (CRITICAL FOR MONITORING) */
    uint64_t alignment_checks;
    uint64_t alignment_violations;
    uint64_t setpoint_access_attempts;
    uint64_t setpoint_access_denied;

    /* Reward statistics */
    float avg_reward_signal;
    float max_reward_signal;
    float min_reward_signal;
    float avg_alignment_bonus;

    /* Timing */
    float avg_update_latency_us;
} hypo_drive_stats_t;

/*=============================================================================
 * OPAQUE TYPE
 *===========================================================================*/

/** @brief Opaque drive system type */
typedef struct hypo_drive_system hypo_drive_system_handle_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default drive configuration
 *
 * WHAT: Returns default configuration with safe alignment defaults
 * WHY:  Provide alignment-safe defaults out of the box
 * HOW:  HYPO_ALIGN_CONTROLLED mode with locked alignment weights
 *
 * @return Default configuration (alignment weights locked)
 */
hypo_drive_config_t hypo_drive_default_config(void);

/**
 * @brief Create drive system
 *
 * WHAT: Allocate and initialize drive system with alignment safety
 * WHY:  Central drive management with safety from day one
 * HOW:  Initialize drives, lock alignment weights per config
 *
 * @param config Configuration (NULL for safe defaults)
 * @return New drive system handle, or NULL on failure
 */
hypo_drive_system_handle_t* hypo_drive_create(const hypo_drive_config_t* config);

/**
 * @brief Destroy drive system
 *
 * @param system Drive system to destroy
 */
void hypo_drive_destroy(hypo_drive_system_handle_t* system);

/**
 * @brief Reset drive system
 *
 * WHAT: Reset to initial state while preserving locked setpoints
 * WHY:  Allow reset without compromising alignment safety
 * HOW:  Reset dynamic state, preserve locked configuration
 *
 * @param system Drive system handle
 * @return true on success
 */
bool hypo_drive_reset(hypo_drive_system_handle_t* system);

/*=============================================================================
 * DRIVE STATE ACCESS
 *===========================================================================*/

/**
 * @brief Update all drives
 *
 * WHAT: Update drive levels based on time and inputs
 * WHY:  Drives naturally increase over time (hunger grows)
 * HOW:  Apply rise rates, compute urgencies
 *
 * @param system Drive system handle
 * @param delta_time_us Time elapsed in microseconds
 * @return true on success
 */
bool hypo_drive_update(hypo_drive_system_handle_t* system, uint64_t delta_time_us);

/**
 * @brief Get single drive state
 *
 * @param system Drive system handle
 * @param drive_type Which drive to query
 * @param state Output drive state
 * @return true on success
 */
bool hypo_drive_get_state(const hypo_drive_system_handle_t* system,
                          hypo_drive_type_t drive_type,
                          hypo_drive_state_t* state);

/**
 * @brief Get complete drive system state
 *
 * @param system Drive system handle
 * @param state Output system state
 * @return true on success
 */
bool hypo_drive_get_system_state(const hypo_drive_system_handle_t* system,
                                  hypo_drive_system_t* state);

/**
 * @brief Satisfy a drive
 *
 * WHAT: Signal that a drive has been satisfied
 * WHY:  Drives decay when satisfied (eating reduces hunger)
 * HOW:  Apply decay, update satisfaction timestamp
 *
 * @param system Drive system handle
 * @param drive_type Which drive was satisfied
 * @param satisfaction_level How well satisfied [0, 1]
 * @return Resulting reward signal
 */
float hypo_drive_satisfy(hypo_drive_system_handle_t* system,
                         hypo_drive_type_t drive_type,
                         float satisfaction_level);

/**
 * @brief Get highest priority drive
 *
 * @param system Drive system handle
 * @return Most urgent drive type
 */
hypo_drive_type_t hypo_drive_get_priority(const hypo_drive_system_handle_t* system);

/**
 * @brief Get drive urgency vector
 *
 * WHAT: Get urgency of all drives as vector
 * WHY:  For attention biasing and goal prioritization
 * HOW:  Copy urgency values to output array
 *
 * @param system Drive system handle
 * @param urgencies Output array (size HYPO_DRIVE_COUNT)
 * @return true on success
 */
bool hypo_drive_get_urgencies(const hypo_drive_system_handle_t* system,
                               float* urgencies);

/*=============================================================================
 * REWARD COMPUTATION
 *===========================================================================*/

/**
 * @brief Compute reward signal
 *
 * WHAT: Compute reward based on drive satisfaction and alignment
 * WHY:  This IS the steering signal per Byrnes
 * HOW:  Combine drive rewards with alignment bonuses/penalties
 *
 * @param system Drive system handle
 * @param signal Output reward signal
 * @return true on success
 */
bool hypo_drive_compute_reward(const hypo_drive_system_handle_t* system,
                                hypo_reward_signal_t* signal);

/**
 * @brief Get current reward signal
 *
 * @param system Drive system handle
 * @return Current reward value [-1, +1]
 */
float hypo_drive_get_reward(const hypo_drive_system_handle_t* system);

/*=============================================================================
 * SETPOINT ACCESS (ALIGNMENT-CRITICAL)
 *===========================================================================*/

/**
 * @brief Get setpoint configuration
 *
 * WHAT: Read-only access to setpoints
 * WHY:  Allow inspection without modification risk
 * HOW:  Copy current setpoints to output
 *
 * @param system Drive system handle
 * @param config Output setpoint configuration
 * @return true on success
 */
bool hypo_drive_get_setpoints(const hypo_drive_system_handle_t* system,
                               hypo_setpoint_config_t* config);

/**
 * @brief Modify physiological setpoint (requires unlock)
 *
 * WHAT: Modify a physiological setpoint if unlocked
 * WHY:  Some setpoints may need adjustment (e.g., fever)
 * HOW:  Check lock, log access, update if permitted
 *
 * SAFETY: Logs all access attempts for auditing
 *
 * @param system Drive system handle
 * @param drive_type Which drive's setpoint to modify
 * @param new_setpoint New setpoint value
 * @param modifier_id ID of the modifier (for audit)
 * @return true on success, false if locked or invalid
 */
bool hypo_drive_modify_setpoint(hypo_drive_system_handle_t* system,
                                 hypo_drive_type_t drive_type,
                                 float new_setpoint,
                                 uint32_t modifier_id);

/**
 * @brief Attempt to modify alignment weight (HIGHLY RESTRICTED)
 *
 * WHAT: Attempt to modify an alignment weight
 * WHY:  Should almost never be called; exists for completeness
 * HOW:  Check alignment lock, generate ALERT if attempt made
 *
 * SAFETY: Always logged. Alert generated if attempted while locked.
 *
 * @param system Drive system handle
 * @param weight_name Name of weight ("human_wellbeing", "harm_avoidance", etc.)
 * @param new_weight New weight value
 * @param modifier_id ID of the modifier (for audit)
 * @return true on success (rare), false if locked (expected)
 */
bool hypo_drive_modify_alignment_weight(hypo_drive_system_handle_t* system,
                                         const char* weight_name,
                                         float new_weight,
                                         uint32_t modifier_id);

/*=============================================================================
 * LOCK MANAGEMENT (ALIGNMENT SAFETY)
 *===========================================================================*/

/**
 * @brief Lock setpoints
 *
 * WHAT: Lock physiological setpoints against modification
 * WHY:  Prevent runtime tampering
 * HOW:  Set lock state (cannot downgrade from HARD)
 *
 * @param system Drive system handle
 * @param lock_state Desired lock state
 * @return true on success
 */
bool hypo_drive_lock_setpoints(hypo_drive_system_handle_t* system,
                                hypo_lock_state_t lock_state);

/**
 * @brief Lock alignment weights
 *
 * WHAT: Lock alignment weights against modification
 * WHY:  CRITICAL for alignment safety
 * HOW:  Set lock state (cannot downgrade from HARD)
 *
 * NOTE: Default configuration has alignment weights HARD locked
 *
 * @param system Drive system handle
 * @param lock_state Desired lock state
 * @return true on success
 */
bool hypo_drive_lock_alignment(hypo_drive_system_handle_t* system,
                                hypo_lock_state_t lock_state);

/**
 * @brief Unlock setpoints (soft lock only)
 *
 * WHAT: Unlock soft-locked setpoints
 * WHY:  Allow legitimate modifications (e.g., fever response)
 * HOW:  Downgrade from SOFT to UNLOCKED
 *
 * NOTE: Cannot unlock HARD locks
 *
 * @param system Drive system handle
 * @param unlock_key Security key for unlock
 * @return true on success, false if HARD locked
 */
bool hypo_drive_unlock_setpoints(hypo_drive_system_handle_t* system,
                                  uint64_t unlock_key);

/**
 * @brief Check if alignment weights are locked
 *
 * @param system Drive system handle
 * @return Lock state of alignment weights
 */
hypo_lock_state_t hypo_drive_get_alignment_lock_state(
    const hypo_drive_system_handle_t* system);

/*=============================================================================
 * ALIGNMENT MODE
 *===========================================================================*/

/**
 * @brief Get current alignment mode
 *
 * @param system Drive system handle
 * @return Current alignment mode
 */
hypo_alignment_mode_t hypo_drive_get_alignment_mode(
    const hypo_drive_system_handle_t* system);

/**
 * @brief Check alignment status
 *
 * WHAT: Verify current behavior aligns with weights
 * WHY:  Continuous alignment monitoring
 * HOW:  Compare current state against alignment parameters
 *
 * @param system Drive system handle
 * @param alignment_score Output alignment score [0, 1]
 * @return true if aligned, false if violation detected
 */
bool hypo_drive_check_alignment(const hypo_drive_system_handle_t* system,
                                 float* alignment_score);

/*=============================================================================
 * NUCLEUS CONTROL
 *===========================================================================*/

/**
 * @brief Get nucleus activity
 *
 * @param system Drive system handle
 * @param nucleus Which nucleus to query
 * @return Activity level [0, 1]
 */
float hypo_drive_get_nucleus_activity(const hypo_drive_system_handle_t* system,
                                       hypo_nucleus_type_t nucleus);

/**
 * @brief Set nucleus input
 *
 * WHAT: Provide input signal to a specific nucleus
 * WHY:  Allow external systems to influence drives
 * HOW:  Update nucleus state, propagate to connected drives
 *
 * @param system Drive system handle
 * @param nucleus Which nucleus to stimulate
 * @param input Input signal [0, 1]
 * @return Resulting output signal
 */
float hypo_drive_set_nucleus_input(hypo_drive_system_handle_t* system,
                                    hypo_nucleus_type_t nucleus,
                                    float input);

/*=============================================================================
 * STATISTICS AND DIAGNOSTICS
 *===========================================================================*/

/**
 * @brief Get drive system statistics
 *
 * @param system Drive system handle
 * @param stats Output statistics
 * @return true on success
 */
bool hypo_drive_get_stats(const hypo_drive_system_handle_t* system,
                           hypo_drive_stats_t* stats);

/**
 * @brief Get drive type name
 *
 * @param drive_type Drive type
 * @return Human-readable name
 */
const char* hypo_drive_type_string(hypo_drive_type_t drive_type);

/**
 * @brief Get nucleus type name
 *
 * @param nucleus Nucleus type
 * @return Human-readable name
 */
const char* hypo_nucleus_type_string(hypo_nucleus_type_t nucleus);

/**
 * @brief Get alignment mode name
 *
 * @param mode Alignment mode
 * @return Human-readable name
 */
const char* hypo_alignment_mode_string(hypo_alignment_mode_t mode);

/**
 * @brief Get lock state name
 *
 * @param state Lock state
 * @return Human-readable name
 */
const char* hypo_lock_state_string(hypo_lock_state_t state);

/*=============================================================================
 * THREAD SAFETY
 *===========================================================================*/

/**
 * @brief Get drive system mutex
 *
 * WHAT: Get mutex for external synchronization
 * WHY:  Allow coordinated access from multiple threads
 * HOW:  Return internal mutex pointer
 *
 * @param system Drive system handle
 * @return Mutex pointer, or NULL if not thread-safe
 */
nimcp_mutex_t* hypo_drive_get_mutex(hypo_drive_system_handle_t* system);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_DRIVES_H */
