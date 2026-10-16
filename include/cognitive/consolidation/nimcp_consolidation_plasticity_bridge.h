/**
 * @file nimcp_consolidation_plasticity_bridge.h
 * @brief Consolidation - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between memory consolidation and synaptic plasticity
 * WHY:  Enable learning of memory stabilization patterns from sleep-like phases
 * HOW:  STDP for replay-strengthening associations, BCM for stabilization, reward
 *       modulation for consolidation success learning
 *
 * THEORETICAL FOUNDATIONS:
 * - Tononi & Cirelli (2014): Sleep function and synaptic homeostasis
 * - Born & Wilhelm (2012): System consolidation during sleep
 * - Lewis & Durrant (2011): Overlapping memory replay in sleep
 *
 * BIOLOGICAL BASIS:
 * - Synaptic homeostasis hypothesis (SHY) during sleep
 * - Sharp-wave ripple-associated plasticity in hippocampus
 * - Slow oscillation-spindle coupling for systems consolidation
 * - Tag-and-capture for late LTP stabilization
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of replay-outcome pairs
 * - BCM: Stabilize consolidated memory patterns
 * - Homeostatic: Synaptic downscaling during sleep
 * - Reward-modulated: Learn from consolidation success
 *
 * @see nimcp_consolidation.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_CONSOLIDATION_PLASTICITY_BRIDGE_H
#define NIMCP_CONSOLIDATION_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum consolidation synapses */
#define CONSOLIDATION_PLASTICITY_MAX_SYNAPSES   256

/** @brief Default learning rate */
#define CONSOLIDATION_PLASTICITY_DEFAULT_LR     0.01f

/** @brief Bio-async module ID */
#define BIO_MODULE_CONSOLIDATION_PLASTICITY     0x0D61

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Consolidation synapse types
 */
typedef enum {
    CONSOLIDATION_SYNAPSE_REPLAY = 0,         /**< Memory replay */
    CONSOLIDATION_SYNAPSE_STABILIZATION,      /**< Synaptic stabilization (PROTECTED) */
    CONSOLIDATION_SYNAPSE_LTP,                /**< Long-term potentiation */
    CONSOLIDATION_SYNAPSE_SCHEMA,             /**< Schema integration */
    CONSOLIDATION_SYNAPSE_PRUNING,            /**< Synaptic pruning */
    CONSOLIDATION_SYNAPSE_TRANSFER            /**< Memory transfer (PROTECTED) */
} consolidation_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    CONSOLIDATION_LEARN_REPLAY_SUCCESS = 0,   /**< Successful memory replay */
    CONSOLIDATION_LEARN_REPLAY_FAILURE,       /**< Failed memory replay */
    CONSOLIDATION_LEARN_LTP_ACHIEVED,         /**< LTP successfully induced */
    CONSOLIDATION_LEARN_LTP_DECAY,            /**< LTP decayed */
    CONSOLIDATION_LEARN_SCHEMA_MATCH,         /**< Schema integration matched */
    CONSOLIDATION_LEARN_SCHEMA_MISMATCH,      /**< Schema integration mismatch */
    CONSOLIDATION_LEARN_PRUNING_APPLIED,      /**< Synaptic pruning applied */
    CONSOLIDATION_LEARN_TRANSFER_COMPLETE,    /**< Memory transfer complete */
    CONSOLIDATION_LEARN_HOMEOSTATIC_SCALE,    /**< Homeostatic scaling applied */
    CONSOLIDATION_LEARN_STABILIZATION_SUCCESS /**< Stabilization success */
} consolidation_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    CONSOLIDATION_PLASTICITY_STATE_IDLE = 0,
    CONSOLIDATION_PLASTICITY_STATE_LEARNING,
    CONSOLIDATION_PLASTICITY_STATE_CONSOLIDATING,
    CONSOLIDATION_PLASTICITY_STATE_UPDATING,
    CONSOLIDATION_PLASTICITY_STATE_ERROR
} consolidation_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Consolidation-Plasticity bridge configuration
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
    float target_stabilization;          /**< Target stabilization level */

    /* Reward modulation */
    float replay_learning_boost;         /**< Boost for successful replay */
    float ltp_learning_boost;            /**< Boost for LTP achievement */
    float schema_modulation;             /**< Schema learning strength */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core pattern protection */
    bool protect_stabilization;          /**< Protect stabilization weights */
    bool protect_memory_transfer;        /**< Protect memory transfer weights */
    float protection_strength;           /**< How strongly to protect core patterns */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} consolidation_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Consolidation synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    consolidation_synapse_type_t type;   /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
} consolidation_plasticity_synapse_t;

/**
 * @brief Memory consolidation learning state
 */
typedef struct {
    float replay_sensitivity;            /**< Sensitivity to replay signals */
    float stabilization_calibration;     /**< Stabilization calibration level */
    float ltp_sensitivity;               /**< Sensitivity to LTP signals */
    float schema_strength;               /**< Schema integration strength */
    float pruning_threshold;             /**< Current pruning threshold */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_learning_us;           /**< Last learning event */
} consolidation_memory_learning_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    consolidation_plasticity_state_t state;  /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
} consolidation_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t replay_success_events;      /**< Replay success events */
    uint64_t replay_failure_events;      /**< Replay failure corrections */
    uint64_t ltp_achieved_events;        /**< LTP achieved events */
    uint64_t stabilization_events;       /**< Stabilization success learning */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
} consolidation_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct consolidation_plasticity_bridge consolidation_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*consolidation_plasticity_learn_callback_t)(
    consolidation_plasticity_bridge_t* bridge,
    consolidation_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Stabilization update callback */
typedef void (*consolidation_plasticity_stabilization_callback_t)(
    consolidation_plasticity_bridge_t* bridge,
    float old_stabilization,
    float new_stabilization,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
consolidation_plasticity_config_t consolidation_plasticity_config_default(void);

/**
 * @brief Create consolidation plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
consolidation_plasticity_bridge_t* consolidation_plasticity_create(
    const consolidation_plasticity_config_t* config
);

/**
 * @brief Destroy consolidation plasticity bridge
 * @param bridge Bridge to destroy
 */
void consolidation_plasticity_destroy(consolidation_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int consolidation_plasticity_reset(consolidation_plasticity_bridge_t* bridge);

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
int consolidation_plasticity_register_synapse(
    consolidation_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    consolidation_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int consolidation_plasticity_unregister_synapse(
    consolidation_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int consolidation_plasticity_get_synapse(
    consolidation_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    consolidation_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int consolidation_plasticity_protect_synapse(
    consolidation_plasticity_bridge_t* bridge,
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
int consolidation_plasticity_learn(
    consolidation_plasticity_bridge_t* bridge,
    consolidation_learn_event_t event,
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
float consolidation_plasticity_apply_stdp(
    consolidation_plasticity_bridge_t* bridge,
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
int consolidation_plasticity_apply_reward(
    consolidation_plasticity_bridge_t* bridge,
    float reward
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int consolidation_plasticity_update_bcm(
    consolidation_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int consolidation_plasticity_homeostatic_update(
    consolidation_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int consolidation_plasticity_update_traces(
    consolidation_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int consolidation_plasticity_consolidate(consolidation_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get memory learning state
 * @param bridge Bridge handle
 * @param state Output learning state
 * @return 0 on success, -1 on failure
 */
int consolidation_plasticity_get_memory_state(
    consolidation_plasticity_bridge_t* bridge,
    consolidation_memory_learning_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int consolidation_plasticity_get_state(
    consolidation_plasticity_bridge_t* bridge,
    consolidation_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int consolidation_plasticity_get_stats(
    consolidation_plasticity_bridge_t* bridge,
    consolidation_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int consolidation_plasticity_reset_stats(consolidation_plasticity_bridge_t* bridge);

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
int consolidation_plasticity_register_learn_callback(
    consolidation_plasticity_bridge_t* bridge,
    consolidation_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register stabilization update callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int consolidation_plasticity_register_stabilization_callback(
    consolidation_plasticity_bridge_t* bridge,
    consolidation_plasticity_stabilization_callback_t callback,
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
int consolidation_plasticity_bio_async_connect(consolidation_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int consolidation_plasticity_bio_async_disconnect(consolidation_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool consolidation_plasticity_is_bio_async_connected(consolidation_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CONSOLIDATION_PLASTICITY_BRIDGE_H */
