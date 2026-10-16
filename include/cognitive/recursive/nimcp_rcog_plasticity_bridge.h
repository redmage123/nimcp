/**
 * @file nimcp_rcog_plasticity_bridge.h
 * @brief Recursive Cognition - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between recursive cognition engine and synaptic plasticity
 * WHY:  Enable learning of recursive problem-solving strategies from experience
 * HOW:  STDP for recursion-outcome associations, BCM for stabilization, reward
 *       modulation for strategy refinement
 *
 * THEORETICAL FOUNDATIONS:
 * - Anderson (1983): Adaptive Control of Thought theory
 * - Schmidhuber (2015): Learning how to learn via self-referential weight matrices
 * - Botvinick (2008): Hierarchical reinforcement learning in the brain
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal-striatal circuits for hierarchical action learning
 * - Hippocampal replay for recursive sequence consolidation
 * - Dopaminergic modulation of recursive planning circuits
 * - Cortical plasticity for meta-cognitive skill acquisition
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of recursion-outcome pairs
 * - BCM: Stabilize effective recursive strategies
 * - Homeostatic: Maintain balanced recursion depth preferences
 * - Reward-modulated: Learn from problem-solving success
 *
 * @see nimcp_rcog_engine.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_RCOG_PLASTICITY_BRIDGE_H
#define NIMCP_RCOG_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum recursive synapses */
#define RCOG_PLASTICITY_MAX_SYNAPSES   256

/** @brief Default learning rate */
#define RCOG_PLASTICITY_DEFAULT_LR     0.01f

/** @brief Bio-async module ID */
#define BIO_MODULE_RCOG_PLASTICITY     0x0D61

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Recursive cognition synapse types
 */
typedef enum {
    RCOG_SYNAPSE_DEPTH_CONTROL = 0,      /**< Recursion depth control (PROTECTED) */
    RCOG_SYNAPSE_DECOMPOSITION,           /**< Decomposition strategy */
    RCOG_SYNAPSE_AGGREGATION,             /**< Result aggregation */
    RCOG_SYNAPSE_META_COGNITIVE,          /**< Meta-cognitive monitoring (PROTECTED) */
    RCOG_SYNAPSE_SELF_REFERENCE,          /**< Self-reference handling */
    RCOG_SYNAPSE_HIERARCHY                /**< Hierarchical processing */
} rcog_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    RCOG_LEARN_DEPTH_OPTIMAL = 0,        /**< Optimal recursion depth found */
    RCOG_LEARN_DEPTH_TOO_DEEP,           /**< Recursion too deep (wasted resources) */
    RCOG_LEARN_DEPTH_TOO_SHALLOW,        /**< Recursion too shallow (incomplete) */
    RCOG_LEARN_DECOMP_SUCCESS,           /**< Decomposition led to success */
    RCOG_LEARN_DECOMP_FAILURE,           /**< Decomposition strategy failed */
    RCOG_LEARN_AGGREGATION_GOOD,         /**< Good result aggregation */
    RCOG_LEARN_AGGREGATION_POOR,         /**< Poor result aggregation */
    RCOG_LEARN_SELF_REF_RESOLVED,        /**< Self-reference loop resolved */
    RCOG_LEARN_SELF_REF_STUCK,           /**< Self-reference loop stuck */
    RCOG_LEARN_META_INSIGHT              /**< Meta-cognitive insight gained */
} rcog_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    RCOG_PLASTICITY_STATE_IDLE = 0,
    RCOG_PLASTICITY_STATE_LEARNING,
    RCOG_PLASTICITY_STATE_CONSOLIDATING,
    RCOG_PLASTICITY_STATE_UPDATING,
    RCOG_PLASTICITY_STATE_ERROR
} rcog_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Recursive Cognition-Plasticity bridge configuration
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
    float target_depth;                  /**< Target recursion depth preference */

    /* Reward modulation */
    float depth_learning_boost;          /**< Boost for optimal depth learning */
    float decomp_learning_boost;         /**< Boost for decomposition success */
    float aggregation_modulation;        /**< Aggregation learning strength */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core pattern protection */
    bool protect_depth_control;          /**< Protect depth control weights */
    bool protect_meta_cognitive;         /**< Protect meta-cognitive weights */
    float protection_strength;           /**< How strongly to protect core patterns */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} rcog_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Recursive cognition synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    rcog_synapse_type_t type;            /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
} rcog_plasticity_synapse_t;

/**
 * @brief Recursive strategy learning state
 */
typedef struct {
    float depth_preference;              /**< Preferred recursion depth */
    float decomposition_calibration;     /**< Decomposition strategy calibration */
    float aggregation_strength;          /**< Aggregation confidence strength */
    float meta_cognitive_sensitivity;    /**< Meta-cognitive sensitivity */
    float self_reference_handling;       /**< Self-reference handling skill */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_learning_us;           /**< Last learning event */
} rcog_strategy_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    rcog_plasticity_state_t state;       /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
} rcog_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t depth_optimal_events;       /**< Optimal depth events */
    uint64_t depth_correction_events;    /**< Depth correction events */
    uint64_t decomp_success_events;      /**< Decomposition success learning */
    uint64_t aggregation_learning_events;/**< Aggregation learning events */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
} rcog_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct rcog_plasticity_bridge rcog_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*rcog_plasticity_learn_callback_t)(
    rcog_plasticity_bridge_t* bridge,
    rcog_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Strategy update callback */
typedef void (*rcog_plasticity_strategy_callback_t)(
    rcog_plasticity_bridge_t* bridge,
    float old_depth_pref,
    float new_depth_pref,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
rcog_plasticity_config_t rcog_plasticity_config_default(void);

/**
 * @brief Create recursive cognition plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
rcog_plasticity_bridge_t* rcog_plasticity_create(
    const rcog_plasticity_config_t* config
);

/**
 * @brief Destroy recursive cognition plasticity bridge
 * @param bridge Bridge to destroy
 */
void rcog_plasticity_destroy(rcog_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int rcog_plasticity_reset(rcog_plasticity_bridge_t* bridge);

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
int rcog_plasticity_register_synapse(
    rcog_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    rcog_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int rcog_plasticity_unregister_synapse(
    rcog_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int rcog_plasticity_get_synapse(
    rcog_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    rcog_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int rcog_plasticity_protect_synapse(
    rcog_plasticity_bridge_t* bridge,
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
int rcog_plasticity_learn(
    rcog_plasticity_bridge_t* bridge,
    rcog_learn_event_t event,
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
float rcog_plasticity_apply_stdp(
    rcog_plasticity_bridge_t* bridge,
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
int rcog_plasticity_apply_reward(
    rcog_plasticity_bridge_t* bridge,
    float reward
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int rcog_plasticity_update_bcm(
    rcog_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int rcog_plasticity_homeostatic_update(
    rcog_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int rcog_plasticity_update_traces(
    rcog_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int rcog_plasticity_consolidate(rcog_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get strategy learning state
 * @param bridge Bridge handle
 * @param state Output strategy state
 * @return 0 on success, -1 on failure
 */
int rcog_plasticity_get_strategy_state(
    rcog_plasticity_bridge_t* bridge,
    rcog_strategy_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int rcog_plasticity_get_state(
    rcog_plasticity_bridge_t* bridge,
    rcog_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int rcog_plasticity_get_stats(
    rcog_plasticity_bridge_t* bridge,
    rcog_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int rcog_plasticity_reset_stats(rcog_plasticity_bridge_t* bridge);

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
int rcog_plasticity_register_learn_callback(
    rcog_plasticity_bridge_t* bridge,
    rcog_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register strategy update callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int rcog_plasticity_register_strategy_callback(
    rcog_plasticity_bridge_t* bridge,
    rcog_plasticity_strategy_callback_t callback,
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
int rcog_plasticity_bio_async_connect(rcog_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int rcog_plasticity_bio_async_disconnect(rcog_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool rcog_plasticity_is_bio_async_connected(rcog_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RCOG_PLASTICITY_BRIDGE_H */
