/**
 * @file nimcp_collective_plasticity_bridge.h
 * @brief Collective Cognition - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between collective cognition engine and synaptic plasticity
 * WHY:  Enable learning of coordination strategies from group experience and feedback
 * HOW:  STDP for synchronization-reward associations, BCM for stabilization, reward
 *       modulation for collective policy learning
 *
 * THEORETICAL FOUNDATIONS:
 * - Couzin & Krause (2003): Self-organization and collective behavior
 * - Bonabeau (1999): Swarm intelligence and optimization
 * - Tomasello (2005): Understanding and sharing intentions
 *
 * BIOLOGICAL BASIS:
 * - Mirror neuron plasticity for social learning
 * - Hippocampal replay for consolidating shared experiences
 * - Oxytocin modulation of social reward pathways
 * - Prefrontal-temporal coupling for joint attention learning
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of coordination-outcome pairs
 * - BCM: Stabilize core synchronization patterns
 * - Homeostatic: Maintain balanced collective dynamics
 * - Reward-modulated: Learn from group success
 *
 * @see nimcp_collective_cognition.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_COLLECTIVE_PLASTICITY_BRIDGE_H
#define NIMCP_COLLECTIVE_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum collective synapses */
#define COLLECTIVE_PLASTICITY_MAX_SYNAPSES   256

/** @brief Default learning rate */
#define COLLECTIVE_PLASTICITY_DEFAULT_LR     0.01f

/** @brief Bio-async module ID */
#define BIO_MODULE_COLLECTIVE_PLASTICITY     0x1231

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Collective synapse types
 */
typedef enum {
    COLLECTIVE_SYNAPSE_SYNCHRONIZATION = 0, /**< Synchronization pathways */
    COLLECTIVE_SYNAPSE_COORDINATION,         /**< Coordination drive (PROTECTED) */
    COLLECTIVE_SYNAPSE_CONSENSUS,            /**< Consensus building */
    COLLECTIVE_SYNAPSE_TRUST,                /**< Trust network */
    COLLECTIVE_SYNAPSE_EMERGENCE,            /**< Emergent behavior */
    COLLECTIVE_SYNAPSE_SHARED_INTENT         /**< Shared intentionality (PROTECTED) */
} collective_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    COLLECTIVE_LEARN_SYNC_ACHIEVED = 0,       /**< Synchronization was achieved */
    COLLECTIVE_LEARN_SYNC_FAILED,              /**< Failed to synchronize */
    COLLECTIVE_LEARN_CONSENSUS_REACHED,        /**< Consensus was reached */
    COLLECTIVE_LEARN_CONSENSUS_FAILED,         /**< Consensus failed */
    COLLECTIVE_LEARN_COORDINATION_SUCCESS,     /**< Successful coordination */
    COLLECTIVE_LEARN_COORDINATION_FAILURE,     /**< Failed coordination */
    COLLECTIVE_LEARN_TRUST_CONFIRMED,          /**< Trust was confirmed */
    COLLECTIVE_LEARN_TRUST_VIOLATED,           /**< Trust was violated */
    COLLECTIVE_LEARN_EMERGENCE_DETECTED,       /**< Emergent behavior detected */
    COLLECTIVE_LEARN_GROUP_REWARD              /**< Group received reward */
} collective_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    COLLECTIVE_PLASTICITY_STATE_IDLE = 0,
    COLLECTIVE_PLASTICITY_STATE_LEARNING,
    COLLECTIVE_PLASTICITY_STATE_CONSOLIDATING,
    COLLECTIVE_PLASTICITY_STATE_UPDATING,
    COLLECTIVE_PLASTICITY_STATE_ERROR
} collective_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Collective-Plasticity bridge configuration
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
    float target_coordination;           /**< Target coordination level */

    /* Reward modulation */
    float sync_learning_boost;           /**< Boost for sync achievement */
    float consensus_learning_boost;      /**< Boost for consensus success */
    float trust_modulation;              /**< Trust learning strength */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core pattern protection */
    bool protect_coordination_drive;     /**< Protect coordination drive weights */
    bool protect_shared_intent;          /**< Protect shared intentionality weights */
    float protection_strength;           /**< How strongly to protect core patterns */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} collective_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Collective synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    collective_synapse_type_t type;      /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
} collective_plasticity_synapse_t;

/**
 * @brief Coordination learning state
 */
typedef struct {
    float sync_sensitivity;              /**< Sensitivity to synchronization */
    float coordination_calibration;      /**< Coordination calibration level */
    float consensus_sensitivity;         /**< Sensitivity to consensus */
    float trust_strength;                /**< Trust detection strength */
    float emergence_strength;            /**< Emergence behavior strength */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_learning_us;           /**< Last learning event */
} collective_coordination_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    collective_plasticity_state_t state; /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
} collective_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t sync_achieved_events;       /**< Sync achieved events */
    uint64_t sync_failed_events;         /**< Sync failure corrections */
    uint64_t consensus_reached_events;   /**< Consensus reached events */
    uint64_t coordination_success_events;/**< Coordination success learning */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
} collective_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct collective_plasticity_bridge collective_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*collective_plasticity_learn_callback_t)(
    collective_plasticity_bridge_t* bridge,
    collective_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Coordination update callback */
typedef void (*collective_plasticity_coordination_callback_t)(
    collective_plasticity_bridge_t* bridge,
    float old_coordination,
    float new_coordination,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
collective_plasticity_config_t collective_plasticity_config_default(void);

/**
 * @brief Create collective plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
collective_plasticity_bridge_t* collective_plasticity_create(
    const collective_plasticity_config_t* config
);

/**
 * @brief Destroy collective plasticity bridge
 * @param bridge Bridge to destroy
 */
void collective_plasticity_destroy(collective_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int collective_plasticity_reset(collective_plasticity_bridge_t* bridge);

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
int collective_plasticity_register_synapse(
    collective_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    collective_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int collective_plasticity_unregister_synapse(
    collective_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int collective_plasticity_get_synapse(
    collective_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    collective_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int collective_plasticity_protect_synapse(
    collective_plasticity_bridge_t* bridge,
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
int collective_plasticity_learn(
    collective_plasticity_bridge_t* bridge,
    collective_learn_event_t event,
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
float collective_plasticity_apply_stdp(
    collective_plasticity_bridge_t* bridge,
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
int collective_plasticity_apply_reward(
    collective_plasticity_bridge_t* bridge,
    float reward
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int collective_plasticity_update_bcm(
    collective_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int collective_plasticity_homeostatic_update(
    collective_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int collective_plasticity_update_traces(
    collective_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int collective_plasticity_consolidate(collective_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get coordination state
 * @param bridge Bridge handle
 * @param state Output coordination state
 * @return 0 on success, -1 on failure
 */
int collective_plasticity_get_coordination_state(
    collective_plasticity_bridge_t* bridge,
    collective_coordination_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int collective_plasticity_get_state(
    collective_plasticity_bridge_t* bridge,
    collective_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int collective_plasticity_get_stats(
    collective_plasticity_bridge_t* bridge,
    collective_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int collective_plasticity_reset_stats(collective_plasticity_bridge_t* bridge);

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
int collective_plasticity_register_learn_callback(
    collective_plasticity_bridge_t* bridge,
    collective_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register coordination update callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int collective_plasticity_register_coordination_callback(
    collective_plasticity_bridge_t* bridge,
    collective_plasticity_coordination_callback_t callback,
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
int collective_plasticity_bio_async_connect(collective_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int collective_plasticity_bio_async_disconnect(collective_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool collective_plasticity_is_bio_async_connected(collective_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COLLECTIVE_PLASTICITY_BRIDGE_H */
