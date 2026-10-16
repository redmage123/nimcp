/**
 * @file nimcp_gw_plasticity_bridge.h
 * @brief Global Workspace - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between global workspace and synaptic plasticity
 * WHY:  Enable learning of conscious access patterns from broadcast experience
 * HOW:  STDP for broadcast associations, BCM for stabilization, reward
 *       modulation for access learning
 *
 * THEORETICAL FOUNDATIONS:
 * - Baars (1988): Global Workspace Theory - learning through broadcast
 * - Dehaene (2011): Conscious access and learning
 * - Cleeremans (2011): Radical plasticity for consciousness
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal plasticity shapes workspace access
 * - Dopaminergic signals modulate broadcast success learning
 * - Repetitive broadcast strengthens coalition pathways
 * - Competition learning refines access thresholds
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of broadcast-outcome pairs
 * - BCM: Stabilize core broadcast mechanisms
 * - Homeostatic: Maintain balanced ignition thresholds
 * - Reward-modulated: Learn from broadcast success
 *
 * PROTECTED SYNAPSES:
 * - BROADCAST and INTEGRATION synapses protected by default
 * - Core GWT mechanisms preserved during learning
 *
 * @see nimcp_global_workspace.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_GW_PLASTICITY_BRIDGE_H
#define NIMCP_GW_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum GW synapses */
#define GW_PLASTICITY_MAX_SYNAPSES   256

/** @brief Default learning rate */
#define GW_PLASTICITY_DEFAULT_LR     0.01f

/** @brief Bio-async module ID */
#define BIO_MODULE_GW_PLASTICITY     0x0D51

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Global Workspace synapse types
 */
typedef enum {
    GW_SYNAPSE_BROADCAST = 0,       /**< Broadcast mechanism (PROTECTED) */
    GW_SYNAPSE_IGNITION,            /**< Ignition threshold */
    GW_SYNAPSE_COMPETITION,         /**< Coalition competition */
    GW_SYNAPSE_INTEGRATION,         /**< Information integration (PROTECTED) */
    GW_SYNAPSE_BINDING,             /**< Feature binding */
    GW_SYNAPSE_COALITION            /**< Coalition formation */
} gw_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    GW_LEARN_BROADCAST_SUCCESS = 0,     /**< Successful broadcast */
    GW_LEARN_BROADCAST_FAILURE,          /**< Failed broadcast attempt */
    GW_LEARN_IGNITION_TRIGGERED,         /**< Ignition cascade triggered */
    GW_LEARN_IGNITION_SUBTHRESHOLD,      /**< Subthreshold ignition */
    GW_LEARN_COMPETITION_WON,            /**< Won coalition competition */
    GW_LEARN_COMPETITION_LOST,           /**< Lost coalition competition */
    GW_LEARN_BINDING_FORMED,             /**< Feature binding formed */
    GW_LEARN_COALITION_STRENGTHENED      /**< Coalition strengthened */
} gw_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    GW_PLASTICITY_STATE_IDLE = 0,
    GW_PLASTICITY_STATE_LEARNING,
    GW_PLASTICITY_STATE_CONSOLIDATING,
    GW_PLASTICITY_STATE_UPDATING,
    GW_PLASTICITY_STATE_ERROR
} gw_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Global Workspace-Plasticity bridge configuration
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
    float target_ignition_rate;          /**< Target ignition rate */

    /* Reward modulation */
    float broadcast_success_boost;       /**< Boost for successful broadcasts */
    float competition_win_boost;         /**< Boost for competition wins */
    float ignition_modulation;           /**< Ignition learning strength */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core mechanism protection */
    bool protect_broadcast_synapses;     /**< Protect broadcast synapses */
    bool protect_integration_synapses;   /**< Protect integration synapses */
    float protection_strength;           /**< How strongly to protect core mechanisms */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} gw_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief GW synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    gw_synapse_type_t type;              /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
} gw_plasticity_synapse_t;

/**
 * @brief Workspace access learning state
 */
typedef struct {
    float broadcast_sensitivity;         /**< Sensitivity to broadcast */
    float ignition_calibration;          /**< Ignition threshold calibration */
    float competition_strength;          /**< Competition learning strength */
    float binding_strength;              /**< Binding learning strength */
    float coalition_strength;            /**< Coalition learning strength */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_learning_us;           /**< Last learning event */
} gw_access_learning_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    gw_plasticity_state_t state;         /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
} gw_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t broadcast_success_events;   /**< Broadcast success events */
    uint64_t broadcast_failure_events;   /**< Broadcast failure corrections */
    uint64_t ignition_events;            /**< Ignition learning events */
    uint64_t competition_events;         /**< Competition learning events */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
} gw_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct gw_plasticity_bridge gw_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*gw_plasticity_learn_callback_t)(
    gw_plasticity_bridge_t* bridge,
    gw_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Access learning update callback */
typedef void (*gw_plasticity_access_callback_t)(
    gw_plasticity_bridge_t* bridge,
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
gw_plasticity_config_t gw_plasticity_config_default(void);

/**
 * @brief Create GW plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
gw_plasticity_bridge_t* gw_plasticity_create(
    const gw_plasticity_config_t* config
);

/**
 * @brief Destroy GW plasticity bridge
 * @param bridge Bridge to destroy
 */
void gw_plasticity_destroy(gw_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int gw_plasticity_reset(gw_plasticity_bridge_t* bridge);

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
int gw_plasticity_register_synapse(
    gw_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    gw_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int gw_plasticity_unregister_synapse(
    gw_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int gw_plasticity_get_synapse(
    gw_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    gw_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int gw_plasticity_protect_synapse(
    gw_plasticity_bridge_t* bridge,
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
int gw_plasticity_learn(
    gw_plasticity_bridge_t* bridge,
    gw_learn_event_t event,
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
float gw_plasticity_apply_stdp(
    gw_plasticity_bridge_t* bridge,
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
int gw_plasticity_apply_reward(
    gw_plasticity_bridge_t* bridge,
    float reward
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int gw_plasticity_update_bcm(
    gw_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int gw_plasticity_homeostatic_update(
    gw_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int gw_plasticity_update_traces(
    gw_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int gw_plasticity_consolidate(gw_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get access learning state
 * @param bridge Bridge handle
 * @param state Output access learning state
 * @return 0 on success, -1 on failure
 */
int gw_plasticity_get_access_learning_state(
    gw_plasticity_bridge_t* bridge,
    gw_access_learning_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int gw_plasticity_get_state(
    gw_plasticity_bridge_t* bridge,
    gw_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int gw_plasticity_get_stats(
    gw_plasticity_bridge_t* bridge,
    gw_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int gw_plasticity_reset_stats(gw_plasticity_bridge_t* bridge);

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
int gw_plasticity_register_learn_callback(
    gw_plasticity_bridge_t* bridge,
    gw_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register access learning update callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int gw_plasticity_register_access_callback(
    gw_plasticity_bridge_t* bridge,
    gw_plasticity_access_callback_t callback,
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
int gw_plasticity_bio_async_connect(gw_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int gw_plasticity_bio_async_disconnect(gw_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool gw_plasticity_is_bio_async_connected(gw_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GW_PLASTICITY_BRIDGE_H */
