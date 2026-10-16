/**
 * @file nimcp_executive_plasticity_bridge.h
 * @brief Executive Function - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between executive functions and synaptic plasticity
 * WHY:  Enable learning of cognitive control patterns from experience and feedback
 * HOW:  STDP for control associations, BCM for stabilization, reward
 *       modulation for reinforcement of successful control strategies
 *
 * THEORETICAL FOUNDATIONS:
 * - Badre & Wagner (2007): Left prefrontal cortex and cognitive control
 * - Ridderinkhof et al. (2004): Learning executive control
 * - Diamond (2013): Executive functions and their development
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex plasticity shapes executive abilities
 * - Dopaminergic signals modulate control learning
 * - ACC plasticity enhances conflict detection and resolution
 * - Repeated control demands strengthen executive circuits
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of control-outcome pairs
 * - BCM: Stabilize core executive patterns
 * - Homeostatic: Maintain balanced control resources
 * - Reward-modulated: Learn from successful inhibition and planning
 *
 * @see nimcp_executive.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_EXECUTIVE_PLASTICITY_BRIDGE_H
#define NIMCP_EXECUTIVE_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum executive synapses */
#define EXECUTIVE_PLASTICITY_MAX_SYNAPSES   256

/** @brief Default learning rate */
#define EXECUTIVE_PLASTICITY_DEFAULT_LR     0.01f

/** @brief Bio-async module ID */
#define BIO_MODULE_EXECUTIVE_PLASTICITY     0x0E41

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Executive synapse types
 */
typedef enum {
    EXEC_SYNAPSE_INHIBITION = 0,   /**< Response inhibition circuit */
    EXEC_SYNAPSE_FLEXIBILITY,       /**< Cognitive flexibility pathway */
    EXEC_SYNAPSE_PLANNING,          /**< Planning/sequencing circuit */
    EXEC_SYNAPSE_GOAL,              /**< Goal maintenance pathway */
    EXEC_SYNAPSE_CONFLICT,          /**< Conflict monitoring circuit */
    EXEC_SYNAPSE_ATTENTION          /**< Attentional control pathway */
} executive_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    EXEC_LEARN_SUCCESSFUL_INHIBITION = 0, /**< Successfully inhibited response */
    EXEC_LEARN_FAILED_INHIBITION,          /**< Failed to inhibit (error) */
    EXEC_LEARN_TASK_SWITCH_SUCCESS,        /**< Successful task switch */
    EXEC_LEARN_TASK_SWITCH_ERROR,          /**< Task switch error */
    EXEC_LEARN_GOAL_MAINTAINED,            /**< Goal successfully maintained */
    EXEC_LEARN_GOAL_LOST,                  /**< Goal was lost (distraction) */
    EXEC_LEARN_CONFLICT_RESOLVED,          /**< Conflict successfully resolved */
    EXEC_LEARN_CONFLICT_UNRESOLVED,        /**< Conflict remains unresolved */
    EXEC_LEARN_PLANNING_SUCCESS,           /**< Plan executed successfully */
    EXEC_LEARN_PLANNING_FAILURE            /**< Plan failed execution */
} executive_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    EXECUTIVE_PLASTICITY_STATE_IDLE = 0,
    EXECUTIVE_PLASTICITY_STATE_LEARNING,
    EXECUTIVE_PLASTICITY_STATE_CONSOLIDATING,
    EXECUTIVE_PLASTICITY_STATE_UPDATING,
    EXECUTIVE_PLASTICITY_STATE_ERROR
} executive_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Executive-Plasticity bridge configuration
 */
typedef struct {
    /* Learning parameters */
    float base_learning_rate;            /**< Base learning rate */
    float stdp_tau_plus_ms;              /**< STDP potentiation time constant */
    float stdp_tau_minus_ms;             /**< STDP depression time constant */
    float stdp_a_plus;                   /**< STDP potentiation magnitude */
    float stdp_a_minus;                  /**< STDP depression magnitude */

    /* BCM parameters */
    float bcm_tau_ms;                    /**< BCM threshold time constant */
    float bcm_target_rate;               /**< BCM target activity */

    /* Homeostatic parameters */
    float homeostatic_tau_ms;            /**< Homeostatic time constant */
    float target_inhibition;             /**< Target inhibition level */

    /* Reward modulation */
    float inhibition_learning_boost;     /**< Boost for successful inhibition */
    float error_learning_boost;          /**< Boost for error learning */
    float control_modulation;            /**< Control learning strength */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core pattern protection */
    bool protect_inhibition_circuits;    /**< Protect inhibition synapses */
    bool protect_goal_circuits;          /**< Protect goal maintenance synapses */
    float protection_strength;           /**< How strongly to protect core patterns */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} executive_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Executive synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    executive_synapse_type_t type;       /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
} executive_plasticity_synapse_t;

/**
 * @brief Executive control calibration state
 */
typedef struct {
    float inhibition_sensitivity;        /**< Sensitivity to inhibition demands */
    float control_calibration;           /**< Control calibration level */
    float conflict_sensitivity;          /**< Sensitivity to conflict */
    float planning_strength;             /**< Planning circuit strength */
    float flexibility_strength;          /**< Flexibility circuit strength */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_learning_us;           /**< Last learning event */
} executive_calibration_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    executive_plasticity_state_t state;  /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
} executive_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t successful_inhibition_events; /**< Successful inhibition events */
    uint64_t failed_inhibition_events;   /**< Failed inhibition corrections */
    uint64_t task_switch_events;         /**< Task switch learning events */
    uint64_t goal_maintenance_events;    /**< Goal maintenance learning */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
} executive_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct executive_plasticity_bridge executive_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*executive_plasticity_learn_callback_t)(
    executive_plasticity_bridge_t* bridge,
    executive_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Calibration update callback */
typedef void (*executive_plasticity_calibration_callback_t)(
    executive_plasticity_bridge_t* bridge,
    float old_calibration,
    float new_calibration,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
executive_plasticity_config_t executive_plasticity_config_default(void);

/**
 * @brief Create executive plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
executive_plasticity_bridge_t* executive_plasticity_create(
    const executive_plasticity_config_t* config
);

/**
 * @brief Destroy executive plasticity bridge
 * @param bridge Bridge to destroy
 */
void executive_plasticity_destroy(executive_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int executive_plasticity_reset(executive_plasticity_bridge_t* bridge);

//=============================================================================
// Synapse Management
//=============================================================================

/**
 * @brief Register a synapse for plasticity tracking
 * @param bridge Bridge handle
 * @param synapse_id Unique synapse ID
 * @param type Synapse type
 * @param initial_weight Initial weight
 * @return 0 on success, -1 on failure
 */
int executive_plasticity_register_synapse(
    executive_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    executive_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int executive_plasticity_unregister_synapse(
    executive_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int executive_plasticity_get_synapse(
    executive_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    executive_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int executive_plasticity_protect_synapse(
    executive_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    bool protect
);

//=============================================================================
// Learning Functions
//=============================================================================

/**
 * @brief Apply learning event
 * @param bridge Bridge handle
 * @param event Event type
 * @param magnitude Event magnitude [0-1]
 * @param synapse_id Target synapse
 * @param context Context strength
 * @return 0 on success, -1 on failure
 */
int executive_plasticity_learn(
    executive_plasticity_bridge_t* bridge,
    executive_learn_event_t event,
    float magnitude,
    uint32_t synapse_id,
    float context
);

/**
 * @brief Apply STDP to synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param pre_time Pre-synaptic spike time (ms)
 * @param post_time Post-synaptic spike time (ms)
 * @return Weight change, NAN on failure
 */
float executive_plasticity_apply_stdp(
    executive_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float pre_time,
    float post_time
);

/**
 * @brief Apply reward modulation
 * @param bridge Bridge handle
 * @param reward Reward signal [-1, 1]
 * @return 0 on success, -1 on failure
 */
int executive_plasticity_apply_reward(
    executive_plasticity_bridge_t* bridge,
    float reward
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int executive_plasticity_update_bcm(
    executive_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int executive_plasticity_homeostatic_update(
    executive_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int executive_plasticity_update_traces(
    executive_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int executive_plasticity_consolidate(executive_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get calibration state
 * @param bridge Bridge handle
 * @param state Output calibration state
 * @return 0 on success, -1 on failure
 */
int executive_plasticity_get_calibration_state(
    executive_plasticity_bridge_t* bridge,
    executive_calibration_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int executive_plasticity_get_state(
    executive_plasticity_bridge_t* bridge,
    executive_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int executive_plasticity_get_stats(
    executive_plasticity_bridge_t* bridge,
    executive_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int executive_plasticity_reset_stats(executive_plasticity_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register learning event callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int executive_plasticity_register_learn_callback(
    executive_plasticity_bridge_t* bridge,
    executive_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register calibration update callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int executive_plasticity_register_calibration_callback(
    executive_plasticity_bridge_t* bridge,
    executive_plasticity_calibration_callback_t callback,
    void* user_data
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Connect to bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int executive_plasticity_bio_async_connect(executive_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int executive_plasticity_bio_async_disconnect(executive_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool executive_plasticity_is_bio_async_connected(executive_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EXECUTIVE_PLASTICITY_BRIDGE_H */
